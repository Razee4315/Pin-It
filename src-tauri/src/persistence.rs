//! Persistence - Save and restore pinned window preferences.
//!
//! Saves process names and opacity settings to a JSON file in the app data directory.
//! On startup, finds matching windows and re-pins them.

use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;

/// Saved preference for a pinned app
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SavedPin {
    /// Process name (e.g., "chrome.exe")
    pub process_name: String,
    /// Window title at time of saving (for smarter matching)
    #[serde(default)]
    pub title: String,
    /// Saved opacity (0-255)
    pub opacity: u8,
}

/// Keyboard shortcut configuration
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ShortcutConfig {
    #[serde(default = "default_toggle_pin")]
    pub toggle_pin: String,
    #[serde(default = "default_opacity_up")]
    pub opacity_up: String,
    #[serde(default = "default_opacity_down")]
    pub opacity_down: String,
    #[serde(default = "default_toggle_window")]
    pub toggle_window: String,
}

fn default_toggle_pin() -> String {
    "super+ctrl+KeyT".to_string()
}
fn default_opacity_up() -> String {
    "super+ctrl+Equal".to_string()
}
fn default_opacity_down() -> String {
    "super+ctrl+Minus".to_string()
}
fn default_toggle_window() -> String {
    "super+ctrl+KeyP".to_string()
}

impl Default for ShortcutConfig {
    fn default() -> Self {
        Self {
            toggle_pin: default_toggle_pin(),
            opacity_up: default_opacity_up(),
            opacity_down: default_opacity_down(),
            toggle_window: default_toggle_window(),
        }
    }
}

impl ShortcutConfig {
    /// (label, shortcut string) pairs for every action, in a fixed order.
    /// Single source of truth for iterating the config — used for
    /// registration, duplicate checks, and diffing on update.
    pub fn entries(&self) -> [(&'static str, &str); 4] {
        [
            ("Pin/Unpin", self.toggle_pin.as_str()),
            ("Opacity +", self.opacity_up.as_str()),
            ("Opacity -", self.opacity_down.as_str()),
            ("Show/Hide", self.toggle_window.as_str()),
        ]
    }
}

/// User preferences / settings
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct UserSettings {
    #[serde(default = "default_true")]
    pub enable_sound: bool,
    #[serde(default)]
    pub has_seen_tray_notice: bool,
    #[serde(default)]
    pub shortcuts: ShortcutConfig,
}

fn default_true() -> bool {
    true
}

impl Default for UserSettings {
    fn default() -> Self {
        Self {
            enable_sound: true,
            has_seen_tray_notice: false,
            shortcuts: ShortcutConfig::default(),
        }
    }
}

/// All saved preferences
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct SavedState {
    /// Map of process_name -> SavedPin
    pub pins: HashMap<String, SavedPin>,
    /// User settings
    #[serde(default)]
    pub settings: UserSettings,
}

/// Get the path to the preferences file
fn get_save_path() -> Option<PathBuf> {
    let app_data = dirs::data_local_dir()?;
    let dir = app_data.join("PinIt");
    Some(dir.join("pinned.json"))
}

/// Load saved state from disk.
/// Falls back to the .bak copy if the main file is corrupted, and only
/// resets to defaults (with a warning) when both are unreadable.
pub fn load() -> SavedState {
    let Some(path) = get_save_path() else {
        return SavedState::default();
    };

    match read_state(&path) {
        Some(state) => state,
        None => {
            let backup = path.with_extension("json.bak");
            match read_state(&backup) {
                Some(state) => {
                    log::warn!("State file unreadable, restored from backup {:?}", backup);
                    state
                }
                None => {
                    if path.exists() {
                        log::warn!("State file {:?} corrupted and no usable backup; starting fresh", path);
                    }
                    SavedState::default()
                }
            }
        }
    }
}

/// Read and parse a state file; None if missing or corrupted
fn read_state(path: &std::path::Path) -> Option<SavedState> {
    let content = fs::read_to_string(path).ok()?;
    match serde_json::from_str(&content) {
        Ok(state) => Some(state),
        Err(e) => {
            log::warn!("Failed to parse {:?}: {}", path, e);
            None
        }
    }
}

/// Save current state to disk atomically: write to a temp file, keep the
/// previous file as .bak, then rename over the target. A crash mid-write
/// can no longer truncate pinned.json.
pub fn save(state: &SavedState) {
    let Some(path) = get_save_path() else {
        log::warn!("Could not determine save path");
        return;
    };

    // Ensure directory exists
    if let Some(parent) = path.parent() {
        let _ = fs::create_dir_all(parent);
    }

    let json = match serde_json::to_string_pretty(state) {
        Ok(json) => json,
        Err(e) => {
            log::error!("Failed to serialize state: {}", e);
            return;
        }
    };

    let tmp = path.with_extension("json.tmp");
    if let Err(e) = fs::write(&tmp, &json) {
        log::error!("Failed to write temp state file: {}", e);
        return;
    }

    // Keep the last good copy around for recovery
    if path.exists() {
        let _ = fs::copy(&path, path.with_extension("json.bak"));
    }

    if let Err(e) = fs::rename(&tmp, &path) {
        log::error!("Failed to replace state file: {}", e);
        let _ = fs::remove_file(&tmp);
    } else {
        log::debug!("State saved to {:?}", path);
    }
}

/// Save current pinned windows state (preserves existing settings)
pub fn save_current() {
    let pinned = crate::always_on_top::state::PinState::get_all();
    let mut state = load(); // Preserve existing settings
    state.pins.clear();

    for win in pinned {
        // Use process_name + hwnd as key to allow multiple windows of same process
        let key = format!("{}:{}", win.process_name, win.hwnd);
        state.pins.insert(
            key,
            SavedPin {
                process_name: win.process_name,
                title: win.title,
                opacity: win.opacity,
            },
        );
    }

    save(&state);
}

/// Get a specific setting value
pub fn get_settings() -> UserSettings {
    load().settings
}

/// Update settings and save
pub fn update_settings(settings: UserSettings) {
    let mut state = load();
    state.settings = settings;
    save(&state);
}

/// Get shortcut configuration
pub fn get_shortcut_config() -> ShortcutConfig {
    load().settings.shortcuts
}

/// Update shortcut configuration and save
pub fn update_shortcut_config(config: ShortcutConfig) {
    let mut state = load();
    state.settings.shortcuts = config;
    save(&state);
}

/// Restore pinned windows from saved state.
/// Enumerates all top-level windows, matches by process name + title, and re-pins them.
/// For each saved pin, only the best matching window is pinned (title match preferred).
pub fn restore() {
    let state = load();
    if state.pins.is_empty() {
        log::info!("No saved pins to restore");
        return;
    }

    log::info!("Restoring {} saved pin(s)", state.pins.len());

    use windows::Win32::Foundation::HWND;

    // Build a lookup: process_name -> Vec<(hwnd, title)>
    let mut window_map: HashMap<String, Vec<(HWND, String)>> = HashMap::new();
    for (hwnd, title, process_name) in crate::always_on_top::pin_manager::enumerate_windows() {
        window_map.entry(process_name).or_default().push((hwnd, title));
    }

    // Track which hwnds we've already pinned to avoid double-pinning
    let mut pinned_hwnds: std::collections::HashSet<isize> = std::collections::HashSet::new();

    for saved in state.pins.values() {
        if let Some(candidates) = window_map.get(&saved.process_name) {
            // Prefer exact title match, fall back to first available
            let best = candidates.iter()
                .find(|(hwnd, title)| !pinned_hwnds.contains(&(hwnd.0 as isize)) && !saved.title.is_empty() && title == &saved.title)
                .or_else(|| candidates.iter().find(|(hwnd, _)| !pinned_hwnds.contains(&(hwnd.0 as isize))));

            if let Some((hwnd, _)) = best {
                if let Ok(true) = crate::always_on_top::pin_manager::pin_window(*hwnd) {
                    pinned_hwnds.insert(hwnd.0 as isize);
                    log::info!("Restored pin for: {} (title: {})", saved.process_name, saved.title);

                    if saved.opacity < 255 {
                        let percent =
                            crate::always_on_top::transparency::alpha_to_percent(saved.opacity);
                        let _ = crate::always_on_top::transparency::set_opacity(*hwnd, percent);
                    }
                }
            }
        }
    }

    let count = pinned_hwnds.len();
    if count > 0 {
        log::info!("Successfully restored {} pinned window(s)", count);
    }
}
