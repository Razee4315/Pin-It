//! Global hotkey registration and handling.
//!
//! Uses tauri-plugin-global-shortcut to register configurable global shortcuts.
//! The handler is set once via `on_shortcuts` at startup. Runtime updates use
//! individual `unregister`/`register` calls so the handler stays active.

use super::pin_manager;
use super::state::PinState;
use crate::persistence::ShortcutConfig;
use once_cell::sync::Lazy;
use serde::Serialize;
use std::str::FromStr;
use std::sync::RwLock;
use tauri::{AppHandle, Emitter, Manager};
use tauri_plugin_global_shortcut::{GlobalShortcutExt, Shortcut, ShortcutState};

/// Global current shortcut config — the handler reads from this to dispatch actions.
static CURRENT_CONFIG: Lazy<RwLock<ShortcutConfig>> =
    Lazy::new(|| RwLock::new(ShortcutConfig::default()));

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

/// Check if a parsed Shortcut matches a config string
fn matches_config(shortcut: &Shortcut, config_str: &str) -> bool {
    Shortcut::from_str(config_str).map_or(false, |s| shortcut == &s)
}

/// Register all global shortcuts for the app using the provided config.
/// Called once at startup — sets the handler that reads from CURRENT_CONFIG.
pub fn register_shortcuts(
    app: &AppHandle,
    config: &ShortcutConfig,
) -> Result<(), Box<dyn std::error::Error>> {
    // Store config globally for the handler
    *CURRENT_CONFIG.write().unwrap() = config.clone();

    let toggle_shortcut = Shortcut::from_str(&config.toggle_pin)?;
    let opacity_up_shortcut = Shortcut::from_str(&config.opacity_up)?;
    let opacity_down_shortcut = Shortcut::from_str(&config.opacity_down)?;
    let show_shortcut = Shortcut::from_str(&config.toggle_window)?;

    let app_handle = app.clone();

    let result = app.global_shortcut().on_shortcuts(
        [
            toggle_shortcut,
            opacity_up_shortcut,
            opacity_down_shortcut,
            show_shortcut,
        ],
        move |_app, shortcut, event| {
            if event.state != ShortcutState::Pressed {
                return;
            }
            // Read current config dynamically — supports hot-swapped shortcuts
            let config = CURRENT_CONFIG.read().unwrap();
            if matches_config(shortcut, &config.toggle_pin) {
                handle_toggle_pin(&app_handle);
            } else if matches_config(shortcut, &config.opacity_up) {
                handle_opacity_change(&app_handle, 5);
            } else if matches_config(shortcut, &config.opacity_down) {
                handle_opacity_change(&app_handle, -5);
            } else if matches_config(shortcut, &config.toggle_window) {
                handle_toggle_window(&app_handle);
            }
        },
    );

    match &result {
        Ok(_) => {
            log::info!(
                "Global shortcuts registered: {} (pin), {} (opacity+), {} (opacity-), {} (show)",
                config.toggle_pin,
                config.opacity_up,
                config.opacity_down,
                config.toggle_window
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

/// Update shortcuts at runtime. Unregisters old shortcuts individually,
/// then registers new ones individually. The handler from `register_shortcuts`
/// stays active and reads from the updated CURRENT_CONFIG.
pub fn update_shortcuts(app: &AppHandle, new_config: &ShortcutConfig) -> Result<(), String> {
    // Validate all new shortcuts first
    validate_shortcut(&new_config.toggle_pin)?;
    validate_shortcut(&new_config.opacity_up)?;
    validate_shortcut(&new_config.opacity_down)?;
    validate_shortcut(&new_config.toggle_window)?;

    let gs = app.global_shortcut();

    // Read old config and unregister each shortcut individually
    let old_config = CURRENT_CONFIG.read().unwrap().clone();
    let old_strings = [
        &old_config.toggle_pin,
        &old_config.opacity_up,
        &old_config.opacity_down,
        &old_config.toggle_window,
    ];
    for s in &old_strings {
        if let Ok(shortcut) = Shortcut::from_str(s) {
            if let Err(e) = gs.unregister(shortcut) {
                log::warn!("Failed to unregister {}: {}", s, e);
            }
        }
    }

    // Update the global config (handler will use this for routing)
    *CURRENT_CONFIG.write().unwrap() = new_config.clone();

    // Register new shortcuts individually
    let new_strings = [
        &new_config.toggle_pin,
        &new_config.opacity_up,
        &new_config.opacity_down,
        &new_config.toggle_window,
    ];
    for (i, s) in new_strings.iter().enumerate() {
        let shortcut =
            Shortcut::from_str(s).map_err(|e| format!("Invalid shortcut '{}': {}", s, e))?;
        if let Err(e) = gs.register(shortcut) {
            log::error!("Failed to register {}: {}", s, e);
            // Rollback: unregister any we just registered, restore old config
            for already in &new_strings[..i] {
                if let Ok(sc) = Shortcut::from_str(already) {
                    let _ = gs.unregister(sc);
                }
            }
            // Re-register old shortcuts
            *CURRENT_CONFIG.write().unwrap() = old_config.clone();
            for os in &old_strings {
                if let Ok(sc) = Shortcut::from_str(os) {
                    let _ = gs.register(sc);
                }
            }
            return Err(format!(
                "Failed to register '{}': {}. Rolled back to previous shortcuts.",
                s, e
            ));
        }
    }

    let _ = app.emit("shortcuts-updated", ());
    log::info!(
        "Shortcuts updated: {} (pin), {} (opacity+), {} (opacity-), {} (show)",
        new_config.toggle_pin,
        new_config.opacity_up,
        new_config.opacity_down,
        new_config.toggle_window
    );
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
                    let _ = app.emit(
                        "pin-toggled",
                        PinToggledPayload {
                            is_pinned,
                            title,
                            process_name: process,
                        },
                    );
                    log::info!("Window {} pinned: {}", hwnd.0 as isize, is_pinned);

                    // Update tray tooltip with current pin count
                    update_tray_tooltip(app);
                }
                Err(e) => {
                    log::error!("Failed to toggle pin: {}", e);
                    let user_msg =
                        format!("Cannot pin {} — it may be running as administrator", process);
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
