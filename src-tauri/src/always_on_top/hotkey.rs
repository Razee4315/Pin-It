//! Global hotkey registration and handling.
//!
//! Uses tauri-plugin-global-shortcut to register Win+Ctrl+T and transparency shortcuts.

use super::pin_manager;
use tauri::{AppHandle, Emitter};
use tauri_plugin_global_shortcut::{Code, GlobalShortcutExt, Modifiers, Shortcut, ShortcutState};

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
            match pin_manager::toggle_pin(hwnd) {
                Ok(is_pinned) => {
                    // Emit event to frontend
                    let _ = app.emit("pin-toggled", is_pinned);
                    log::info!("Window {} pinned: {}", hwnd.0 as isize, is_pinned);
                }
                Err(e) => {
                    log::error!("Failed to toggle pin: {}", e);
                    let _ = app.emit("pin-error", e.to_string());
                }
            }
        }
        Err(e) => {
            log::warn!("No foreground window: {}", e);
        }
    }
}

/// Handle opacity change hotkey
fn handle_opacity_change(app: &AppHandle, delta: i32) {
    match pin_manager::get_foreground_window() {
        Ok(hwnd) => {
            if super::state::PinState::is_pinned(hwnd) {
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
