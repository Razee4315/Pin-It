//! host — PinIt's hidden helper window and everything that needs one.
//!
//! One hidden top-level window on the UI thread receives what Windows sends
//! to an app rather than to a visible window: global hotkeys, the tray icon's
//! clicks, "Windows is shutting down", a second launch asking us to show
//! ourselves, and Explorer restarting (the tray icon must then be re-added).
//! Slint's event loop dispatches messages for every window on this thread, so
//! nothing here needs a loop or a thread of its own.
//!
//! It must be top-level, not message-only: message-only windows never see the
//! WM_QUERYENDSESSION broadcast.

use std::cell::RefCell;
use std::mem::size_of;

use windows::Win32::Foundation::{GetLastError, ERROR_ALREADY_EXISTS, HANDLE, HWND, LPARAM, LRESULT, POINT, WPARAM};
use windows::Win32::System::LibraryLoader::GetModuleHandleW;
use windows::Win32::System::Threading::CreateMutexW;
use windows::Win32::UI::Accessibility::{SetWinEventHook, UnhookWinEvent, HWINEVENTHOOK};
use windows::Win32::UI::Input::KeyboardAndMouse::{RegisterHotKey, UnregisterHotKey, HOT_KEY_MODIFIERS, MOD_NOREPEAT};
use windows::Win32::UI::Shell::{
    ShellExecuteW, Shell_NotifyIconW, NIF_ICON, NIF_INFO, NIF_MESSAGE, NIF_SHOWTIP, NIF_TIP, NIIF_NOSOUND, NIIF_USER,
    NIM_ADD, NIM_DELETE, NIM_MODIFY, NIM_SETVERSION, NOTIFYICONDATAW, NOTIFYICON_VERSION_4,
};
use windows::Win32::UI::WindowsAndMessaging::*;
use windows::core::{w, HSTRING, PCWSTR};

use crate::win::Hwnd;

/// What the helper window tells the app.
#[derive(Clone, Copy, Debug)]
pub enum Event {
    Hotkey(i32),
    TrayActivate,
    TrayMenu { x: i32, y: i32 },
    ShowRequested,
    ForegroundChanged,
    SessionEnding,
    SessionEndCancelled,
    SessionEnded,
}

const WM_TRAY: u32 = WM_APP + 1;
const WM_SHOW_REQUEST: u32 = WM_APP + 2;
const TRAY_ID: u32 = 1;

thread_local! {
    static HANDLER: RefCell<Option<Box<dyn Fn(Event)>>> = const { RefCell::new(None) };
    static HOST: RefCell<HWND> = const { RefCell::new(HWND(std::ptr::null_mut())) };
    static TASKBAR_CREATED: RefCell<u32> = const { RefCell::new(0) };
    static TRAY: RefCell<Option<TrayState>> = const { RefCell::new(None) };
    static HOOK: RefCell<Option<HWINEVENTHOOK>> = const { RefCell::new(None) };
}

struct TrayState {
    icon: HICON,
    tip: String,
}

/// Deliver an event. Most go through the event loop so the app is never
/// re-entered from inside one of its own calls (a tray menu runs a modal
/// loop, for example). Session-end events are delivered at once: Windows
/// expects an answer before it moves on.
fn emit(event: Event, now: bool) {
    if now {
        HANDLER.with(|h| {
            if let Ok(h) = h.try_borrow() {
                if let Some(h) = h.as_ref() {
                    h(event);
                }
            }
        });
    } else {
        let _ = slint::invoke_from_event_loop(move || {
            HANDLER.with(|h| {
                if let Some(h) = h.borrow().as_ref() {
                    h(event);
                }
            })
        });
    }
}

unsafe extern "system" fn wndproc(hwnd: HWND, msg: u32, wp: WPARAM, lp: LPARAM) -> LRESULT {
    unsafe {
        match msg {
            WM_HOTKEY => emit(Event::Hotkey(wp.0 as i32), false),
            WM_TRAY => {
                // NOTIFYICON_VERSION_4: the event is in LOWORD(lParam), the
                // anchor point in wParam.
                let event = (lp.0 & 0xffff) as u32;
                let (x, y) = ((wp.0 & 0xffff) as i16 as i32, ((wp.0 >> 16) & 0xffff) as i16 as i32);
                match event {
                    WM_CONTEXTMENU => emit(Event::TrayMenu { x, y }, false),
                    0x400 /* NIN_SELECT */ | 0x401 /* NIN_KEYSELECT */ => emit(Event::TrayActivate, false),
                    _ => {}
                }
            }
            WM_SHOW_REQUEST => emit(Event::ShowRequested, false),
            WM_QUERYENDSESSION => {
                emit(Event::SessionEnding, true);
                return LRESULT(1);
            }
            WM_ENDSESSION => {
                emit(if wp.0 != 0 { Event::SessionEnded } else { Event::SessionEndCancelled }, true);
                return LRESULT(0);
            }
            _ => {
                if msg != 0 && msg == TASKBAR_CREATED.with(|t| *t.borrow()) {
                    readd_tray();
                }
            }
        }
        DefWindowProcW(hwnd, msg, wp, lp)
    }
}

fn class_name() -> HSTRING {
    HSTRING::from(format!("PinItHost{}", crate::instance_suffix()))
}

/// Another PinIt is already running in this session: ask it to show itself.
/// Returns true if this process should exit.
pub fn hand_off_to_running_instance() -> bool {
    let name = HSTRING::from(format!("PinIt_SingleInstance_v2{}", crate::instance_suffix()));
    unsafe {
        // Kept for the life of the process; the installer uses it as AppMutex.
        let mutex: windows::core::Result<HANDLE> = CreateMutexW(None, false, &name);
        if mutex.is_ok() && GetLastError() != ERROR_ALREADY_EXISTS {
            std::mem::forget(mutex);
            return false;
        }
        if let Ok(other) = FindWindowW(&class_name(), PCWSTR::null()) {
            let mut pid = 0;
            GetWindowThreadProcessId(other, Some(&mut pid));
            let _ = AllowSetForegroundWindow(pid);
            let _ = PostMessageW(Some(other), WM_SHOW_REQUEST, WPARAM(0), LPARAM(0));
        }
        true
    }
}

pub fn create(handler: impl Fn(Event) + 'static) -> Hwnd {
    HANDLER.with(|h| *h.borrow_mut() = Some(Box::new(handler)));
    unsafe {
        let instance = GetModuleHandleW(None).unwrap_or_default();
        let class = class_name();
        let wc = WNDCLASSW {
            lpfnWndProc: Some(wndproc),
            hInstance: instance.into(),
            lpszClassName: PCWSTR(class.as_ptr()),
            ..Default::default()
        };
        RegisterClassW(&wc);
        let hwnd = CreateWindowExW(
            WS_EX_TOOLWINDOW,
            &class,
            w!("PinIt"),
            WS_OVERLAPPED,
            0,
            0,
            0,
            0,
            None,
            None,
            Some(instance.into()),
            None,
        )
        .expect("could not create PinIt's helper window");
        HOST.with(|h| *h.borrow_mut() = hwnd);
        TASKBAR_CREATED.with(|t| *t.borrow_mut() = RegisterWindowMessageW(w!("TaskbarCreated")));
        hwnd.0 as Hwnd
    }
}

fn host() -> HWND {
    HOST.with(|h| *h.borrow())
}

// ---- Hotkeys ---------------------------------------------------------------

/// Register (or replace) hotkey `id`. False if another app owns the combination.
pub fn register_hotkey(id: i32, mods: u32, vk: u32) -> bool {
    unsafe {
        let _ = UnregisterHotKey(Some(host()), id);
        RegisterHotKey(Some(host()), id, HOT_KEY_MODIFIERS(mods) | MOD_NOREPEAT, vk).is_ok()
    }
}

pub fn unregister_hotkey(id: i32) {
    unsafe {
        let _ = UnregisterHotKey(Some(host()), id);
    }
}

// ---- Foreground changes ----------------------------------------------------

unsafe extern "system" fn on_foreground(_: HWINEVENTHOOK, _: u32, _: HWND, _: i32, _: i32, _: u32, _: u32) {
    emit(Event::ForegroundChanged, false);
}

/// Hear about every foreground change while windows are pinned. Out of
/// context: nothing is injected into other apps; Windows queues the event to
/// this thread.
pub fn watch_foreground(on: bool) {
    HOOK.with(|hook| {
        let mut hook = hook.borrow_mut();
        match (on, hook.is_some()) {
            (true, false) => unsafe {
                let h = SetWinEventHook(
                    EVENT_SYSTEM_FOREGROUND,
                    EVENT_SYSTEM_FOREGROUND,
                    None,
                    Some(on_foreground),
                    0,
                    0,
                    WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS,
                );
                if !h.is_invalid() {
                    *hook = Some(h);
                }
            },
            (false, true) => unsafe {
                let _ = UnhookWinEvent(hook.take().unwrap());
            },
            _ => {}
        }
    });
}

// ---- Tray icon --------------------------------------------------------------

fn copy_wide(dst: &mut [u16], s: &str) {
    let src: Vec<u16> = s.encode_utf16().take(dst.len() - 1).collect();
    dst[..src.len()].copy_from_slice(&src);
    dst[src.len()] = 0;
}

fn tray_data() -> NOTIFYICONDATAW {
    NOTIFYICONDATAW {
        cbSize: size_of::<NOTIFYICONDATAW>() as u32,
        hWnd: host(),
        uID: TRAY_ID,
        ..Default::default()
    }
}

pub fn add_tray(tip: &str) {
    unsafe {
        let instance = GetModuleHandleW(None).unwrap_or_default();
        let size = GetSystemMetrics(SM_CXSMICON);
        let icon = LoadImageW(Some(instance.into()), PCWSTR(1 as _), IMAGE_ICON, size, size, LR_DEFAULTCOLOR)
            .map(|h| HICON(h.0))
            .unwrap_or_default();
        TRAY.with(|t| *t.borrow_mut() = Some(TrayState { icon, tip: tip.to_string() }));
    }
    readd_tray();
}

fn readd_tray() {
    TRAY.with(|t| {
        let t = t.borrow();
        let Some(state) = t.as_ref() else { return };
        let mut data = tray_data();
        data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
        data.uCallbackMessage = WM_TRAY;
        data.hIcon = state.icon;
        copy_wide(&mut data.szTip, &state.tip);
        unsafe {
            let _ = Shell_NotifyIconW(NIM_ADD, &data);
            data.Anonymous.uVersion = NOTIFYICON_VERSION_4;
            let _ = Shell_NotifyIconW(NIM_SETVERSION, &data);
        }
    });
}

pub fn set_tray_tip(tip: &str) {
    TRAY.with(|t| {
        if let Some(state) = t.borrow_mut().as_mut() {
            state.tip = tip.to_string();
        }
    });
    let mut data = tray_data();
    data.uFlags = NIF_TIP | NIF_SHOWTIP;
    copy_wide(&mut data.szTip, tip);
    unsafe {
        let _ = Shell_NotifyIconW(NIM_MODIFY, &data);
    }
}

/// A notification from the tray icon. Windows 10/11 shows it as a toast.
pub fn notify(title: &str, message: &str) {
    let mut data = tray_data();
    data.uFlags = NIF_INFO;
    data.dwInfoFlags = NIIF_USER | NIIF_NOSOUND;
    TRAY.with(|t| {
        if let Some(state) = t.borrow().as_ref() {
            data.hBalloonIcon = state.icon;
        }
    });
    copy_wide(&mut data.szInfoTitle, title);
    copy_wide(&mut data.szInfo, message);
    unsafe {
        let _ = Shell_NotifyIconW(NIM_MODIFY, &data);
    }
}

pub fn remove_tray() {
    let data = tray_data();
    unsafe {
        let _ = Shell_NotifyIconW(NIM_DELETE, &data);
    }
}

pub enum MenuItem {
    Command { id: u32, text: String, checked: bool, enabled: bool },
    Separator,
}

/// Show a native popup menu at (x, y); returns the chosen command id.
/// Blocks in a modal loop until the menu closes.
pub fn popup_menu(items: &[MenuItem], x: i32, y: i32, default_id: u32) -> Option<u32> {
    unsafe {
        let menu = CreatePopupMenu().ok()?;
        for item in items {
            match item {
                MenuItem::Separator => {
                    let _ = AppendMenuW(menu, MF_SEPARATOR, 0, PCWSTR::null());
                }
                MenuItem::Command { id, text, checked, enabled } => {
                    let mut flags = MF_STRING;
                    if *checked {
                        flags |= MF_CHECKED;
                    }
                    if !*enabled {
                        flags |= MF_GRAYED;
                    }
                    // "&" marks an access key in menus; show it literally.
                    let label = HSTRING::from(text.replace('&', "&&"));
                    let _ = AppendMenuW(menu, flags, *id as usize, &label);
                }
            }
        }
        let _ = SetMenuDefaultItem(menu, default_id, 0);
        // Without this the menu won't close when the user clicks elsewhere.
        let _ = SetForegroundWindow(host());
        let chosen = TrackPopupMenuEx(
            menu,
            (TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN).0,
            x,
            y,
            host(),
            None,
        );
        let _ = PostMessageW(Some(host()), WM_NULL, WPARAM(0), LPARAM(0));
        let _ = DestroyMenu(menu);
        (chosen.0 != 0).then_some(chosen.0 as u32)
    }
}

pub fn cursor_pos() -> (i32, i32) {
    let mut p = POINT::default();
    unsafe {
        let _ = GetCursorPos(&mut p);
    }
    (p.x, p.y)
}

pub fn open_url(url: &str) {
    unsafe {
        ShellExecuteW(None, w!("open"), &HSTRING::from(url), PCWSTR::null(), PCWSTR::null(), SW_SHOWNORMAL);
    }
}
