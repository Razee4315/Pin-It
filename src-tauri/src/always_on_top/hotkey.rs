//! Global hotkey registration and handling.
//!
//! Uses tauri-plugin-global-shortcut to register configurable global shortcuts.

use super::pin_manager;
use super::state::PinState;
use crate::persistence::ShortcutConfig;
use serde::Serialize;
use std::str::FromStr;
use tauri::{AppHandle, Emitter, Manager};
use tauri_plugin_global_shortcut::{GlobalShortcutExt, Shortcut, ShortcutState};

/// Event payload for pin toggle notifications
#[derive(Clone, Serialize)]
pub struct PinToggledPayload {
    pub is_pinned: bool,
    pub title: String,
    pub process_name: String,
}

/// Validate a shortcut string can be parsed
pub fn validate_shortcut(shortcut_str: &str) -> Result<(), String> {
    Shortcut::from_str(shortcut_str)
        .map(|_| ())
        .map_err(|e| format!("Invalid shortcut '{}': {}", shortcut_str, e))
}

/// Register all global shortcuts for the app using the provided config
pub fn register_shortcuts(
    app: &AppHandle,
    config: &ShortcutConfig,
) -> Result<(), Box<dyn std::error::Error>> {
    let toggle_shortcut = Shortcut::from_str(&config.toggle_pin)?;
    let opacity_up_shortcut = Shortcut::from_str(&config.opacity_up)?;
    let opacity_down_shortcut = Shortcut::from_str(&config.opacity_down)?;
    let show_shortcut = Shortcut::from_str(&config.toggle_window)?;

    let app_handle = app.clone();
    let toggle_clone = toggle_shortcut;
    let up_clone = opacity_up_shortcut;
    let down_clone = opacity_down_shortcut;
    let show_clone = show_shortcut;

    let result = app.global_shortcut().on_shortcuts(
        [toggle_shortcut, opacity_up_shortcut, opacity_down_shortcut, show_shortcut],
        move |_app, shortcut, event| {
            if event.state != ShortcutState::Pressed {
                return;
            }

            if shortcut == &toggle_clone {
                handle_toggle_pin(&app_handle);
            } else if shortcut == &up_clone {
                handle_opacity_change(&app_handle, 5);
            } else if shortcut == &down_clone {
                handle_opacity_change(&app_handle, -5);
            } else if shortcut == &show_clone {
                handle_toggle_window(&app_handle);
            }
        },
    );

    match &result {
        Ok(_) => {
            log::info!(
                "Global shortcuts registered: {} (pin), {} (opacity+), {} (opacity-), {} (show)",
                config.toggle_pin, config.opacity_up, config.opacity_down, config.toggle_window
            );
        }
        Err(e) => {
            log::error!("Failed to register shortcuts: {}", e);
            let _ = app.emit(
                "pin-error",
                format!(
                    "Could not register shortcuts — another app may be using them: {}",
                    e
                ),
            );
        }
    }

    result.map_err(|e| e.into())
}

/// Unregister all shortcuts, then re-register with new config.
/// On failure, attempts rollback to default config.
pub fn update_shortcuts(
    app: &AppHandle,
    config: &ShortcutConfig,
) -> Result<(), String> {
    // Validate all shortcuts first
    validate_shortcut(&config.toggle_pin)?;
    validate_shortcut(&config.opacity_up)?;
    validate_shortcut(&config.opacity_down)?;
    validate_shortcut(&config.toggle_window)?;

    // Unregister all existing shortcuts
    app.global_shortcut()
        .unregister_all()
        .map_err(|e| format!("Failed to unregister shortcuts: {}", e))?;

    // Register with new config
    match register_shortcuts(app, config) {
        Ok(_) => {
            let _ = app.emit("shortcuts-updated", ());
            Ok(())
        }
        Err(e) => {
            // Rollback to defaults
            log::warn!("Failed to register new shortcuts, rolling back to defaults: {}", e);
            let defaults = ShortcutConfig::default();
            let _ = register_shortcuts(app, &defaults);
            Err(format!("Failed to register shortcuts: {}. Rolled back to defaults.", e))
        }
    }
}

/// Handle toggle pin hotkey
fn handle_toggle_pin(app: &AppHandle) {
    match pin_manager::get_foreground_window() {
        Ok(hwnd) => {
            // Get window info before toggle (title may be needed for toast)
            let title = pin_manager::get_window_title_pub(hwnd);
            let process = pin_manager::get_process_name_pub(hwnd);

            match pin_manager::toggle_pin(hwnd) {
                Ok(is_pinned) => {
                    // Emit rich event to frontend for toast notification
                    let _ = app.emit("pin-toggled", PinToggledPayload {
                        is_pinned,
                        title,
                        process_name: process,
                    });
                    log::info!("Window {} pinned: {}", hwnd.0 as isize, is_pinned);

                    // Update tray tooltip with current pin count
                    update_tray_tooltip(app);
                }
                Err(e) => {
                    log::error!("Failed to toggle pin: {}", e);
                    let user_msg = format!("Cannot pin {} — it may be running as administrator", process);
                    let _ = app.emit("pin-error", user_msg);
                }
            }
        }
        Err(_) => {
            log::warn!("No foreground window to pin");
            let _ = app.emit("pin-error", "No window to pin — click on a window first");
        }
    }
}

/// Handle toggle PinIt window visibility
fn handle_toggle_window(app: &AppHandle) {
    if let Some(window) = app.get_webview_window("main") {
        match window.is_visible() {
            Ok(true) => {
                let _ = window.hide();
            }
            _ => {
                let _ = window.show();
                let _ = window.set_focus();
            }
        }
    }
}

/// Handle opacity change hotkey
fn handle_opacity_change(app: &AppHandle, delta: i32) {
    match pin_manager::get_foreground_window() {
        Ok(hwnd) => {
            if PinState::is_pinned(hwnd) {
                match super::transparency::adjust_opacity(hwnd, delta) {
                    Ok(new_opacity) => {
                        let _ = app.emit("opacity-changed", new_opacity);
                        log::info!("Opacity changed to {}%", new_opacity);
                    }
                    Err(e) => {
                        log::error!("Failed to adjust opacity: {}", e);
                    }
                }
            }
        }
        Err(_) => {}
    }
}

/// Update the tray icon tooltip with current pin count
pub fn update_tray_tooltip(app: &AppHandle) {
    let count = PinState::get_all().len();
    let tooltip = if count == 0 {
        "PinIt - No windows pinned".to_string()
    } else {
        format!(
            "PinIt - {} window{} pinned",
            count,
            if count == 1 { "" } else { "s" }
        )
    };

    if let Some(tray) = app.tray_by_id("main-tray") {
        let _ = tray.set_tooltip(Some(&tooltip));
    }
}
