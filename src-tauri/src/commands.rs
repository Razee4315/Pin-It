//! Tauri IPC commands for the Always on Top functionality.

use crate::always_on_top::{
    hotkey::{self, PinToggledPayload},
    pin_manager,
    state::{PinState, PinnedWindow},
    transparency, PinError,
};
use tauri::Emitter;
use windows::Win32::Foundation::HWND;

/// Convert an IPC window handle (isize) to a Win32 HWND
fn to_hwnd(hwnd: isize) -> HWND {
    HWND(hwnd as *mut std::ffi::c_void)
}

/// Pin a specific window by its handle.
/// Emits the same pin-toggled event as the hotkey path so the UI gets
/// its toast/sound/refresh from one place.
#[tauri::command]
pub fn pin_window(app: tauri::AppHandle, hwnd: isize) -> Result<bool, PinError> {
    let hwnd = to_hwnd(hwnd);
    let title = pin_manager::get_window_title_pub(hwnd);
    let process_name = pin_manager::get_process_name_pub(hwnd);
    let result = pin_manager::pin_window(hwnd)?;
    let _ = app.emit(
        crate::events::PIN_TOGGLED,
        PinToggledPayload {
            is_pinned: true,
            title,
            process_name,
        },
    );
    hotkey::update_tray_tooltip(&app);
    Ok(result)
}

/// Unpin a specific window by its handle
#[tauri::command]
pub fn unpin_window(app: tauri::AppHandle, hwnd: isize) -> Result<bool, PinError> {
    let hwnd = to_hwnd(hwnd);
    let title = pin_manager::get_window_title_pub(hwnd);
    let process_name = pin_manager::get_process_name_pub(hwnd);
    let result = pin_manager::unpin_window(hwnd)?;
    let _ = app.emit(
        crate::events::PIN_TOGGLED,
        PinToggledPayload {
            is_pinned: false,
            title,
            process_name,
        },
    );
    // The tray tooltip previously went stale when unpinning via the UI
    // button (only the hotkey path updated it)
    hotkey::update_tray_tooltip(&app);
    Ok(result)
}

/// List visible top-level windows that could be pinned (for the picker)
#[tauri::command]
pub fn list_pinnable_windows() -> Vec<pin_manager::PinnableWindow> {
    let mut windows = pin_manager::list_pinnable();
    windows.sort_by(|a, b| {
        a.process_name
            .to_lowercase()
            .cmp(&b.process_name.to_lowercase())
    });
    windows
}

/// Get list of all pinned windows (sorted by process name)
#[tauri::command]
pub fn get_pinned_windows() -> Vec<PinnedWindow> {
    let mut windows = PinState::get_all();
    windows.sort_by(|a, b| {
        a.process_name
            .to_lowercase()
            .cmp(&b.process_name.to_lowercase())
    });
    windows
}

/// Set opacity of a specific pinned window
#[tauri::command]
pub fn set_window_opacity(hwnd: isize, percent: u8) -> Result<(), PinError> {
    transparency::set_opacity(to_hwnd(hwnd), percent)
}

/// Bring a pinned window to focus
#[tauri::command]
pub fn focus_window(hwnd: isize) -> Result<(), PinError> {
    use windows::Win32::UI::WindowsAndMessaging::{
        IsIconic, SetForegroundWindow, ShowWindow, SW_RESTORE,
    };
    let hwnd = to_hwnd(hwnd);

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

/// Get the current shortcut configuration
#[tauri::command]
pub fn get_shortcut_config() -> crate::persistence::ShortcutConfig {
    crate::persistence::get_shortcut_config()
}

/// Update shortcut configuration (validates, saves, and re-registers)
#[tauri::command]
pub fn set_shortcut_config(
    app: tauri::AppHandle,
    config: crate::persistence::ShortcutConfig,
) -> Result<(), String> {
    crate::always_on_top::hotkey::update_shortcuts(&app, &config)?;
    crate::persistence::update_shortcut_config(config);
    Ok(())
}

/// Reset shortcuts to defaults
#[tauri::command]
pub fn reset_shortcut_config(
    app: tauri::AppHandle,
) -> Result<crate::persistence::ShortcutConfig, String> {
    let defaults = crate::persistence::ShortcutConfig::default();
    crate::always_on_top::hotkey::update_shortcuts(&app, &defaults)?;
    crate::persistence::update_shortcut_config(defaults.clone());
    Ok(defaults)
}
