//! flash — a brief coloured outline drawn around another app's window.
//!
//! PinIt's on-screen feedback: it appears where the user is looking the
//! moment a window is pinned or unpinned, and points out which window a row
//! in the list belongs to. The outline is a click-through, never-focused
//! layered window that fades out by itself (or simply disappears when Windows
//! animations are off).

use std::cell::RefCell;
use std::mem::size_of;
use std::time::{Duration, Instant};

use windows::Win32::Foundation::{HWND, LPARAM, LRESULT, POINT, SIZE, WPARAM};
use windows::Win32::Graphics::Gdi::{
    CreateCompatibleDC, CreateDIBSection, DeleteDC, DeleteObject, GetDC, ReleaseDC, SelectObject, AC_SRC_ALPHA,
    AC_SRC_OVER, BITMAPINFO, BITMAPINFOHEADER, BI_RGB, BLENDFUNCTION, DIB_RGB_COLORS, HBITMAP, HDC,
};
use windows::Win32::System::LibraryLoader::GetModuleHandleW;
use windows::Win32::UI::HiDpi::GetDpiForWindow;
use windows::Win32::UI::WindowsAndMessaging::*;
use windows::core::w;

use crate::win::{self, Hwnd};

#[derive(Clone, Copy, PartialEq)]
pub enum Kind {
    Pinned,
    Unpinned,
    Hover,
}

const HOLD: Duration = Duration::from_millis(300);
const FADE: Duration = Duration::from_millis(250);

struct Overlay {
    hwnd: HWND,
    dc: HDC,
    bitmap: HBITMAP,
    old: windows::Win32::Graphics::Gdi::HGDIOBJ,
    pos: POINT,
    size: SIZE,
    started: Instant,
    persistent: bool,
}

impl Drop for Overlay {
    fn drop(&mut self) {
        unsafe {
            let _ = DestroyWindow(self.hwnd);
            SelectObject(self.dc, self.old);
            let _ = DeleteObject(self.bitmap.into());
            let _ = DeleteDC(self.dc);
        }
    }
}

thread_local! {
    static CURRENT: RefCell<Option<Overlay>> = const { RefCell::new(None) };
    static TIMER: RefCell<Option<slint::Timer>> = const { RefCell::new(None) };
}

unsafe extern "system" fn wndproc(hwnd: HWND, msg: u32, wp: WPARAM, lp: LPARAM) -> LRESULT {
    unsafe {
        if msg == WM_NCHITTEST {
            return LRESULT(HTTRANSPARENT as isize);
        }
        DefWindowProcW(hwnd, msg, wp, lp)
    }
}

fn register_class() {
    static ONCE: std::sync::Once = std::sync::Once::new();
    ONCE.call_once(|| unsafe {
        let wc = WNDCLASSW {
            lpfnWndProc: Some(wndproc),
            hInstance: GetModuleHandleW(None).unwrap_or_default().into(),
            lpszClassName: w!("PinItOutline"),
            ..Default::default()
        };
        RegisterClassW(&wc);
    });
}

/// Show the outline around `target`. `Hover` stays until `hide()`; the others
/// fade out by themselves.
pub fn show(target: Hwnd, kind: Kind) {
    hide();
    let Some(rect) = win::frame_rect(target) else { return };
    if unsafe { IsIconic(HWND(target as *mut _)).as_bool() } {
        return;
    }
    let scale = unsafe { GetDpiForWindow(HWND(target as *mut _)) }.max(96) as f32 / 96.0;
    let border = (4.0 * scale).round();
    let radius = if is_windows_11() { 8.0 * scale } else { 0.0 };
    let color: [u8; 3] = match kind {
        Kind::Pinned => [0xc4, 0x94, 0x64],
        Kind::Unpinned => [0x8a, 0x85, 0x7c],
        Kind::Hover => [0xc4, 0x94, 0x64],
    };

    // Grow the overlay a little so the outline sits just outside the window.
    let pad = (border / 2.0) as i32;
    let (x, y) = (rect.left - pad, rect.top - pad);
    let (w, h) = (rect.right - rect.left + 2 * pad, rect.bottom - rect.top + 2 * pad);
    if w <= 0 || h <= 0 || w > 16384 || h > 16384 {
        return;
    }

    register_class();
    unsafe {
        let Ok(hwnd) = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            w!("PinItOutline"),
            w!(""),
            WS_POPUP,
            x,
            y,
            w,
            h,
            None,
            None,
            Some(GetModuleHandleW(None).unwrap_or_default().into()),
            None,
        ) else {
            return;
        };

        let screen = GetDC(None);
        let dc = CreateCompatibleDC(Some(screen));
        ReleaseDC(None, screen);
        let info = BITMAPINFO {
            bmiHeader: BITMAPINFOHEADER {
                biSize: size_of::<BITMAPINFOHEADER>() as u32,
                biWidth: w,
                biHeight: -h,
                biPlanes: 1,
                biBitCount: 32,
                biCompression: BI_RGB.0,
                ..Default::default()
            },
            ..Default::default()
        };
        let mut bits: *mut core::ffi::c_void = std::ptr::null_mut();
        let Ok(bitmap) = CreateDIBSection(Some(dc), &info, DIB_RGB_COLORS, &mut bits, None, 0) else {
            let _ = DeleteDC(dc);
            let _ = DestroyWindow(hwnd);
            return;
        };
        let pixels = std::slice::from_raw_parts_mut(bits as *mut u8, (w * h * 4) as usize);
        paint_outline(pixels, w, h, border, radius, color);
        let old = SelectObject(dc, bitmap.into());

        let overlay = Overlay {
            hwnd,
            dc,
            bitmap,
            old,
            pos: POINT { x, y },
            size: SIZE { cx: w, cy: h },
            started: Instant::now(),
            persistent: kind == Kind::Hover,
        };
        overlay.update(255);
        let _ = ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        CURRENT.with(|c| *c.borrow_mut() = Some(overlay));
    }

    if kind != Kind::Hover {
        let timer = slint::Timer::default();
        timer.start(slint::TimerMode::Repeated, Duration::from_millis(16), tick);
        TIMER.with(|t| *t.borrow_mut() = Some(timer));
    }
}

pub fn hide() {
    TIMER.with(|t| t.borrow_mut().take());
    CURRENT.with(|c| c.borrow_mut().take());
}

fn tick() {
    let done = CURRENT.with(|c| {
        let c = c.borrow();
        let Some(o) = c.as_ref() else { return true };
        if o.persistent {
            return false;
        }
        let t = o.started.elapsed();
        if t < HOLD {
            return false;
        }
        if !win::animations_enabled() {
            return t >= HOLD + FADE;
        }
        let f = (t - HOLD).as_secs_f32() / FADE.as_secs_f32();
        if f >= 1.0 {
            return true;
        }
        let eased = 1.0 - (1.0 - f) * (1.0 - f);
        o.update(((1.0 - eased) * 255.0) as u8);
        false
    });
    if done {
        hide();
    }
}

impl Overlay {
    fn update(&self, alpha: u8) {
        let blend = BLENDFUNCTION {
            BlendOp: AC_SRC_OVER as u8,
            BlendFlags: 0,
            SourceConstantAlpha: alpha,
            AlphaFormat: AC_SRC_ALPHA as u8,
        };
        let src = POINT { x: 0, y: 0 };
        unsafe {
            let _ = UpdateLayeredWindow(
                self.hwnd,
                None,
                Some(&self.pos),
                Some(&self.size),
                Some(self.dc),
                Some(&src),
                windows::Win32::Foundation::COLORREF(0),
                Some(&blend),
                ULW_ALPHA,
            );
        }
    }
}

/// Draw an anti-aliased rounded-rectangle outline into premultiplied BGRA.
/// Only the band along the edges is computed; the inside stays transparent.
fn paint_outline(px: &mut [u8], w: i32, h: i32, border: f32, radius: f32, rgb: [u8; 3]) {
    let half = border / 2.0;
    let (cx, cy) = (w as f32 / 2.0, h as f32 / 2.0);
    let (hx, hy) = (cx - half, cy - half); // half extents of the stroke's centre line
    let r = radius.min(hx).min(hy);
    let band = (border + radius + 2.0).ceil() as i32;

    let mut shade = |x: i32, y: i32| {
        let (px_, py_) = (x as f32 + 0.5 - cx, y as f32 + 0.5 - cy);
        let qx = px_.abs() - (hx - r);
        let qy = py_.abs() - (hy - r);
        let outside = (qx.max(0.0).powi(2) + qy.max(0.0).powi(2)).sqrt();
        let d = outside + qx.max(qy).min(0.0) - r; // signed distance to centre line
        let a = (half + 0.5 - d.abs()).clamp(0.0, 1.0);
        if a > 0.0 {
            let i = ((y * w + x) * 4) as usize;
            let a8 = (a * 255.0) as u32;
            px[i] = ((rgb[2] as u32 * a8) / 255) as u8;
            px[i + 1] = ((rgb[1] as u32 * a8) / 255) as u8;
            px[i + 2] = ((rgb[0] as u32 * a8) / 255) as u8;
            px[i + 3] = a8 as u8;
        }
    };

    for y in 0..h {
        if y < band || y >= h - band {
            for x in 0..w {
                shade(x, y);
            }
        } else {
            for x in (0..band.min(w)).chain((w - band).max(0)..w) {
                shade(x, y);
            }
        }
    }
}

fn is_windows_11() -> bool {
    // Windows 11 is build 22000 and later. RtlGetVersion isn't fooled by the
    // compatibility shims that make GetVersionEx lie.
    #[repr(C)]
    struct OsVersionInfo {
        size: u32,
        major: u32,
        minor: u32,
        build: u32,
        platform: u32,
        csd: [u16; 128],
    }
    #[link(name = "ntdll")]
    unsafe extern "system" {
        fn RtlGetVersion(info: *mut OsVersionInfo) -> i32;
    }
    let mut info = OsVersionInfo { size: size_of::<OsVersionInfo>() as u32, major: 0, minor: 0, build: 0, platform: 0, csd: [0; 128] };
    unsafe { RtlGetVersion(&mut info) };
    info.build >= 22000
}
