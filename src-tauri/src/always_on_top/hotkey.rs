//! Global hotkey registration and handling.
//!
//! Uses tauri-plugin-global-shortcut to register configurable global shortcuts.
//! The handler is set once via `Builder::with_handler()` at plugin init time.
//! Runtime updates use `unregister_all` + individual `register` calls — the
//! global handler dispatches based on the current CURRENT_CONFIG.

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

/// Global handler called by the plugin for ALL registered shortcuts.
/// Set once via `Builder::with_handler()` in lib.rs — never changes.
/// Reads CURRENT_CONFIG to dispatch to the correct action.
pub fn handle_shortcut(app: &AppHandle, shortcut: &Shortcut, event: tauri_plugin_global_shortcut::ShortcutEvent) {
    if event.state != ShortcutState::Pressed {
        return;
    }
    let config = CURRENT_CONFIG.read().unwrap();
    if matches_config(shortcut, &config.toggle_pin) {
        handle_toggle_pin(app);
    } else if matches_config(shortcut, &config.opacity_up) {
        handle_opacity_change(app, 5);
    } else if matches_config(shortcut, &config.opacity_down) {
        handle_opacity_change(app, -5);
    } else if matches_config(shortcut, &config.toggle_window) {
        handle_toggle_window(app);
    }
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

/// Register all global shortcuts using plain `register()` calls.
/// Best-effort: registers what it can, warns about failures, returns Ok
/// if at least one shortcut registered. Only returns Err if ALL failed.
pub fn register_shortcuts(
    app: &AppHandle,
    config: &ShortcutConfig,
) -> Result<(), Box<dyn std::error::Error>> {
    // Store config globally for the handler
    *CURRENT_CONFIG.write().unwrap() = config.clone();

    let gs = app.global_shortcut();
    let shortcut_entries: [(&str, &str); 4] = [
        ("Pin/Unpin", &config.toggle_pin),
        ("Opacity +", &config.opacity_up),
        ("Opacity -", &config.opacity_down),
        ("Show/Hide", &config.toggle_window),
    ];

    let mut registered = 0u32;
    let mut failed_names: Vec<&str> = Vec::new();

    for (label, s) in &shortcut_entries {
        match Shortcut::from_str(s) {
            Ok(shortcut) => {
                if let Err(e) = gs.register(shortcut) {
                    log::warn!("Could not register {} ({}): {}", label, s, e);
                    failed_names.push(label);
                } else {
                    registered += 1;
                }
            }
            Err(e) => {
                log::warn!("Invalid shortcut for {} ({}): {}", label, s, e);
                failed_names.push(label);
            }
        }
    }

    if !failed_names.is_empty() {
        let msg = format!(
            "Some shortcuts unavailable: {}. Another app or PinIt instance may be using them.",
            failed_names.join(", ")
        );
        log::warn!("{}", msg);
        let _ = app.emit("pin-error", &msg);
    }

    if registered > 0 {
        log::info!(
            "Global shortcuts registered: {}/{} — {} (pin), {} (opacity+), {} (opacity-), {} (show)",
            registered,
            shortcut_entries.len(),
            config.toggle_pin,
            config.opacity_up,
            config.opacity_down,
            config.toggle_window
        );
        Ok(())
    } else {
        let msg = "Could not register any shortcuts — another app or PinIt instance may be using them.".to_string();
        log::error!("{}", msg);
        let _ = app.emit("pin-error", &msg);
        Err(msg.into())
    }
}

/// Check that all shortcut strings in a config are unique
fn check_duplicates(config: &ShortcutConfig) -> Result<(), String> {
    let shortcuts = [
        ("Pin/Unpin", &config.toggle_pin),
        ("Opacity +", &config.opacity_up),
        ("Opacity -", &config.opacity_down),
        ("Show/Hide", &config.toggle_window),
    ];
    for i in 0..shortcuts.len() {
        for j in (i + 1)..shortcuts.len() {
            if shortcuts[i].1 == shortcuts[j].1 {
                return Err(format!(
                    "'{}' and '{}' cannot use the same shortcut.",
                    shortcuts[i].0, shortcuts[j].0
                ));
            }
        }
    }
    Ok(())
}

/// Update shortcuts at runtime. Only unregisters/re-registers shortcuts that
/// actually changed — unchanged ones are left alone to avoid OS conflicts.
pub fn update_shortcuts(app: &AppHandle, new_config: &ShortcutConfig) -> Result<(), String> {
    // Validate all new shortcuts first
    validate_shortcut(&new_config.toggle_pin)?;
    validate_shortcut(&new_config.opacity_up)?;
    validate_shortcut(&new_config.opacity_down)?;
    validate_shortcut(&new_config.toggle_window)?;
    check_duplicates(new_config)?;

    let old_config = CURRENT_CONFIG.read().unwrap().clone();
    let gs = app.global_shortcut();

    // Collect pairs of (old, new, label) — only process changed ones
    let pairs: Vec<(&str, &str, &str)> = vec![
        (&*old_config.toggle_pin, &*new_config.toggle_pin, "Pin/Unpin"),
        (&*old_config.opacity_up, &*new_config.opacity_up, "Opacity +"),
        (&*old_config.opacity_down, &*new_config.opacity_down, "Opacity -"),
        (&*old_config.toggle_window, &*new_config.toggle_window, "Show/Hide"),
    ];

    let changed: Vec<_> = pairs.iter().filter(|(old, new, _)| old != new).collect();
    if changed.is_empty() {
        return Ok(()); // Nothing changed
    }

    // Unregister only the old shortcuts that are being replaced
    for (old_str, _, label) in &changed {
        if let Ok(sc) = Shortcut::from_str(old_str) {
            if let Err(e) = gs.unregister(sc) {
                log::warn!("Could not unregister old {} ({}): {}", label, old_str, e);
            }
        }
    }

    // Register the new shortcuts
    for (i, (_, new_str, label)) in changed.iter().enumerate() {
        match Shortcut::from_str(new_str) {
            Ok(sc) => {
                if let Err(e) = gs.register(sc) {
                    log::error!("Failed to register {} ({}): {}", label, new_str, e);
                    // Rollback: unregister any we just registered, re-register old ones
                    for (_, prev_new, _) in &changed[..i] {
                        if let Ok(s) = Shortcut::from_str(prev_new) {
                            let _ = gs.unregister(s);
                        }
                    }
                    for (prev_old, _, _) in &changed[..=i] {
                        if let Ok(s) = Shortcut::from_str(prev_old) {
                            let _ = gs.register(s);
                        }
                    }
                    return Err(format!(
                        "Shortcut unavailable for {} — another app may be using it.",
                        label
                    ));
                }
            }
            Err(e) => {
                return Err(format!("Invalid shortcut for {}: {}", label, e));
            }
        }
    }

    // All good — update global config
    *CURRENT_CONFIG.write().unwrap() = new_config.clone();
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
