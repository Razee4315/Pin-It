//! Pin Manager - Core window pinning logic using Windows APIs.
//!
//! Implements `SetWindowPos` with `HWND_TOPMOST` / `HWND_NOTOPMOST` to control
//! the always-on-top state of windows.

use super::error::PinError;
use super::state::PinState;
use windows::core::{PCWSTR, PWSTR};
use windows::Win32::Foundation::{BOOL, HWND, MAX_PATH};
use windows::Win32::System::Threading::{
    OpenProcess, QueryFullProcessImageNameW, PROCESS_NAME_WIN32, PROCESS_QUERY_LIMITED_INFORMATION,
};
use windows::Win32::UI::WindowsAndMessaging::{
    GetForegroundWindow, GetWindowLongW, GetWindowTextLengthW, GetWindowTextW,
    GetWindowThreadProcessId, RemovePropW, SetPropW, SetWindowPos, GWL_EXSTYLE, HWND_NOTOPMOST,
    HWND_TOPMOST, SWP_NOMOVE, SWP_NOSIZE, WS_EX_TOPMOST,
};

/// Property name used to tag windows as pinned by our app
const WINDOW_PINNED_PROP: &str = "PinIt_Pinned\0";

/// Pin a window to always stay on top
pub fn pin_window(hwnd: HWND) -> Result<bool, PinError> {
    unsafe {
        // Get window title
        let title = get_window_title(hwnd);
        let process_name = get_process_name(hwnd);

        // Set property to mark as pinned by us
        let prop_name: Vec<u16> = WINDOW_PINNED_PROP.encode_utf16().collect();
        SetPropW(hwnd, PCWSTR(prop_name.as_ptr()), windows::Win32::Foundation::HANDLE(1 as *mut std::ffi::c_void))
            .map_err(|e| PinError::SetPropertyFailed(e.to_string()))?;

        // Set HWND_TOPMOST
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE)
            .map_err(|e| PinError::SetWindowPosFailed(e.to_string()))?;

        // Track in our state
        PinState::add(hwnd, title, process_name);

        Ok(true)
    }
}

/// Unpin a window (remove always-on-top)
pub fn unpin_window(hwnd: HWND) -> Result<bool, PinError> {
    unsafe {
        // Remove our property marker
        let prop_name: Vec<u16> = WINDOW_PINNED_PROP.encode_utf16().collect();
        let _ = RemovePropW(hwnd, PCWSTR(prop_name.as_ptr()));

        // Remove TOPMOST
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE)
            .map_err(|e| PinError::SetWindowPosFailed(e.to_string()))?;

        // Remove from state
        PinState::remove(hwnd);

        Ok(false)
    }
}

/// Toggle pin state on a window
pub fn toggle_pin(hwnd: HWND) -> Result<bool, PinError> {
    if is_topmost(hwnd) || PinState::is_pinned(hwnd) {
        unpin_window(hwnd)
    } else {
        pin_window(hwnd)
    }
}

/// Check if a window has WS_EX_TOPMOST style
pub fn is_topmost(hwnd: HWND) -> bool {
    unsafe {
        let ex_style = GetWindowLongW(hwnd, GWL_EXSTYLE);
        (ex_style as u32 & WS_EX_TOPMOST.0) != 0
    }
}

/// Get the currently focused foreground window
pub fn get_foreground_window() -> Result<HWND, PinError> {
    unsafe {
        let hwnd = GetForegroundWindow();
        if hwnd.0.is_null() {
            Err(PinError::NoForegroundWindow)
        } else {
            Ok(hwnd)
        }
    }
}

/// Get window title as String
fn get_window_title(hwnd: HWND) -> String {
    unsafe {
        let length = GetWindowTextLengthW(hwnd);
        if length == 0 {
            return String::from("Unknown");
        }

        let mut buffer: Vec<u16> = vec![0; (length + 1) as usize];
        let copied = GetWindowTextW(hwnd, &mut buffer);
        if copied == 0 {
            return String::from("Unknown");
        }

        String::from_utf16_lossy(&buffer[..copied as usize])
    }
}

/// Get process name for a window
fn get_process_name(hwnd: HWND) -> String {
    unsafe {
        let mut process_id: u32 = 0;
        GetWindowThreadProcessId(hwnd, Some(&mut process_id));

        if process_id == 0 {
            return String::from("Unknown");
        }

        let handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, BOOL::from(false), process_id);
        if handle.is_err() {
            return String::from("Unknown");
        }

        let handle = handle.unwrap();
        let mut buffer: Vec<u16> = vec![0; MAX_PATH as usize];
        let mut size = buffer.len() as u32;

        if QueryFullProcessImageNameW(handle, PROCESS_NAME_WIN32, PWSTR(buffer.as_mut_ptr()), &mut size).is_ok() {
            let path = String::from_utf16_lossy(&buffer[..size as usize]);
            // Extract just the filename
            path.rsplit('\\').next().unwrap_or("Unknown").to_string()
        } else {
            String::from("Unknown")
        }
    }
}
