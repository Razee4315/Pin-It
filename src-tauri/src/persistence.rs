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

/// User preferences / settings
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct UserSettings {
    #[serde(default = "default_true")]
    pub enable_sound: bool,
    #[serde(default)]
    pub has_seen_tray_notice: bool,
}

fn default_true() -> bool {
    true
}

impl Default for UserSettings {
    fn default() -> Self {
        Self {
            enable_sound: true,
            has_seen_tray_notice: false,
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

/// Load saved state from disk
pub fn load() -> SavedState {
    let Some(path) = get_save_path() else {
        return SavedState::default();
    };

    match fs::read_to_string(&path) {
        Ok(content) => serde_json::from_str(&content).unwrap_or_default(),
        Err(_) => SavedState::default(),
    }
}

/// Save current state to disk
pub fn save(state: &SavedState) {
    let Some(path) = get_save_path() else {
        log::warn!("Could not determine save path");
        return;
    };

    // Ensure directory exists
    if let Some(parent) = path.parent() {
        let _ = fs::create_dir_all(parent);
    }

    match serde_json::to_string_pretty(state) {
        Ok(json) => {
            if let Err(e) = fs::write(&path, json) {
                log::error!("Failed to save state: {}", e);
            } else {
                log::debug!("State saved to {:?}", path);
            }
        }
        Err(e) => {
            log::error!("Failed to serialize state: {}", e);
        }
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

    unsafe {
        use windows::Win32::Foundation::{BOOL, HWND, LPARAM};
        use windows::Win32::UI::WindowsAndMessaging::{
            EnumWindows, GetWindowLongW, IsWindowVisible, GWL_EXSTYLE, GWL_STYLE, WS_EX_TOOLWINDOW,
            WS_VISIBLE,
        };

        // Collect all visible top-level windows
        unsafe extern "system" fn enum_callback(hwnd: HWND, lparam: LPARAM) -> BOOL {
            let windows = &mut *(lparam.0 as *mut Vec<HWND>);

            // Only consider visible, non-tool windows
            if !IsWindowVisible(hwnd).as_bool() {
                return BOOL::from(true);
            }

            let style = GetWindowLongW(hwnd, GWL_STYLE) as u32;
            let ex_style = GetWindowLongW(hwnd, GWL_EXSTYLE) as u32;

            if (style & WS_VISIBLE.0) != 0 && (ex_style & WS_EX_TOOLWINDOW.0) == 0 {
                windows.push(hwnd);
            }

            BOOL::from(true)
        }

        let mut windows: Vec<HWND> = Vec::new();
        let _ = EnumWindows(
            Some(enum_callback),
            LPARAM(&mut windows as *mut Vec<HWND> as isize),
        );

        // Build a lookup: process_name -> Vec<(hwnd, title)>
        let mut window_map: HashMap<String, Vec<(HWND, String)>> = HashMap::new();
        for hwnd in &windows {
            let process_name = crate::always_on_top::pin_manager::get_process_name_pub(*hwnd);
            let title = crate::always_on_top::pin_manager::get_window_title_pub(*hwnd);
            window_map.entry(process_name).or_default().push((*hwnd, title));
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
                            let percent = ((saved.opacity as u32 * 100) / 255) as u8;
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
}
