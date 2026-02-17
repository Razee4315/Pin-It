//! Global hotkey registration and handling.
//!
//! Uses tauri-plugin-global-shortcut to register Win+Ctrl+T and transparency shortcuts.

use super::pin_manager;
use super::state::PinState;
use serde::Serialize;
use tauri::{AppHandle, Emitter};
use tauri_plugin_global_shortcut::{Code, GlobalShortcutExt, Modifiers, Shortcut, ShortcutState};

/// Event payload for pin toggle notifications
#[derive(Clone, Serialize)]
pub struct PinToggledPayload {
    pub is_pinned: bool,
    pub title: String,
    pub process_name: String,
}

/// Register all global shortcuts for the app
pub fn register_shortcuts(app: &AppHandle) -> Result<(), Box<dyn std::error::Error>> {
    // Win+Ctrl+T - Toggle pin on foreground window
    let toggle_shortcut = Shortcut::new(Some(Modifiers::SUPER | Modifiers::CONTROL), Code::KeyT);

    // Win+Ctrl+= - Increase opacity
    let opacity_up_shortcut =
        Shortcut::new(Some(Modifiers::SUPER | Modifiers::CONTROL), Code::Equal);

    // Win+Ctrl+- - Decrease opacity
    let opacity_down_shortcut =
        Shortcut::new(Some(Modifiers::SUPER | Modifiers::CONTROL), Code::Minus);

    let app_handle = app.clone();
    let toggle_clone = toggle_shortcut.clone();
    let up_clone = opacity_up_shortcut.clone();
    let down_clone = opacity_down_shortcut.clone();

    app.global_shortcut().on_shortcuts(
        [toggle_shortcut, opacity_up_shortcut, opacity_down_shortcut],
        move |_app, shortcut, event| {
            if event.state != ShortcutState::Pressed {
                return;
            }

            if shortcut == &toggle_clone {
                handle_toggle_pin(&app_handle);
            } else if shortcut == &up_clone {
                handle_opacity_change(&app_handle, 10);
            } else if shortcut == &down_clone {
                handle_opacity_change(&app_handle, -10);
            }
        },
    )?;

    log::info!("Global shortcuts registered: Win+Ctrl+T, Win+Ctrl+=, Win+Ctrl+-");

    Ok(())
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
                    let _ = app.emit("pin-error", e.to_string());
                }
            }
        }
        Err(_) => {
            log::warn!("No foreground window to pin");
            let _ = app.emit("pin-error", "No window to pin — click on a window first");
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
