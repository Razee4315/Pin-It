//! Tauri IPC commands for the Always on Top functionality.

use crate::always_on_top::{pin_manager, state::PinState, transparency, PinError};

/// Toggle pin state on the foreground window
#[tauri::command]
pub fn toggle_pin_foreground() -> Result<bool, PinError> {
    let hwnd = pin_manager::get_foreground_window()?;
    pin_manager::toggle_pin(hwnd)
}

/// Pin a specific window by its handle
#[tauri::command]
pub fn pin_window(hwnd: isize) -> Result<bool, PinError> {
    use windows::Win32::Foundation::HWND;
    let hwnd = HWND(hwnd as *mut std::ffi::c_void);
    pin_manager::pin_window(hwnd)
}

/// Unpin a specific window by its handle
#[tauri::command]
pub fn unpin_window(hwnd: isize) -> Result<bool, PinError> {
    use windows::Win32::Foundation::HWND;
    let hwnd = HWND(hwnd as *mut std::ffi::c_void);
    pin_manager::unpin_window(hwnd)
}

/// Get list of all pinned windows
#[tauri::command]
pub fn get_pinned_windows() -> Vec<state::PinnedWindow> {
    PinState::get_all()
}

/// Adjust opacity of foreground window (delta can be negative)
#[tauri::command]
pub fn adjust_opacity(delta: i32) -> Result<u8, PinError> {
    let hwnd = pin_manager::get_foreground_window()?;

    // Only adjust if window is pinned
    if !PinState::is_pinned(hwnd) {
        return Err(PinError::WindowExcluded);
    }

    transparency::adjust_opacity(hwnd, delta)
}

/// Set opacity of a specific pinned window
#[tauri::command]
pub fn set_window_opacity(hwnd: isize, percent: u8) -> Result<(), PinError> {
    use windows::Win32::Foundation::HWND;
    let hwnd = HWND(hwnd as *mut std::ffi::c_void);
    transparency::set_opacity(hwnd, percent)
}

/// Check if a window is currently topmost
#[tauri::command]
pub fn is_window_topmost(hwnd: isize) -> bool {
    use windows::Win32::Foundation::HWND;
    let hwnd = HWND(hwnd as *mut std::ffi::c_void);
    pin_manager::is_topmost(hwnd)
}

/// Bring a pinned window to focus
#[tauri::command]
pub fn focus_window(hwnd: isize) -> Result<(), PinError> {
    use windows::Win32::Foundation::HWND;
    use windows::Win32::UI::WindowsAndMessaging::{
        IsIconic, SetForegroundWindow, ShowWindow, SW_RESTORE,
    };
    let hwnd = HWND(hwnd as *mut std::ffi::c_void);

    if !pin_manager::is_valid_window(hwnd) {
        return Err(PinError::NoForegroundWindow);
    }

    unsafe {
        // Restore if minimized
        if IsIconic(hwnd).as_bool() {
            let _ = ShowWindow(hwnd, SW_RESTORE);
        }
        let _ = SetForegroundWindow(hwnd);
    }

    Ok(())
}

/// Get the count of currently pinned windows
#[tauri::command]
pub fn get_pinned_count() -> usize {
    PinState::get_all().len()
}

/// Check if auto-start is enabled
#[tauri::command]
pub fn get_auto_start() -> bool {
    crate::autostart::is_enabled()
}

/// Set auto-start enabled/disabled
#[tauri::command]
pub fn set_auto_start(enabled: bool) -> Result<(), String> {
    if enabled {
        crate::autostart::enable()
    } else {
        crate::autostart::disable()
    }
}

/// Get sound enabled setting
#[tauri::command]
pub fn get_sound_enabled() -> bool {
    crate::persistence::get_settings().enable_sound
}

/// Set sound enabled setting
#[tauri::command]
pub fn set_sound_enabled(enabled: bool) {
    let mut settings = crate::persistence::get_settings();
    settings.enable_sound = enabled;
    crate::persistence::update_settings(settings);
}

/// Get whether user has seen the tray notice
#[tauri::command]
pub fn get_has_seen_tray_notice() -> bool {
    crate::persistence::get_settings().has_seen_tray_notice
}

/// Mark tray notice as seen
#[tauri::command]
pub fn set_has_seen_tray_notice() {
    let mut settings = crate::persistence::get_settings();
    settings.has_seen_tray_notice = true;
    crate::persistence::update_settings(settings);
}

// Re-export PinnedWindow for command return type
pub use crate::always_on_top::state;
