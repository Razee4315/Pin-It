//! store — PinIt's state in %LOCALAPPDATA%\PinIt\pinned.json.
//!
//! The same file and schema the Tauri and Qt builds use, so an existing
//! install's pins and settings carry straight over. Keys this build doesn't
//! know are kept and written back untouched.

use std::path::PathBuf;

use serde::{Deserialize, Serialize};
use serde_json::{Map, Value};

fn yes() -> bool {
    true
}
fn opaque() -> i32 {
    255
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct SavedPin {
    #[serde(default)]
    pub process_name: String,
    #[serde(default)]
    pub title: String,
    /// 8-bit alpha (0-255), as the first Rust build wrote it.
    #[serde(default = "opaque")]
    pub opacity: i32,
    #[serde(default)]
    pub click_through: bool,
    // What the window looked like before PinIt first touched it, so re-pinning
    // after a crash doesn't mistake PinIt's own changes for the app's.
    // Default true = "unknown, trust the window", right for older files.
    #[serde(default = "yes")]
    pub was_layered: bool,
    #[serde(default = "yes")]
    pub was_topmost: bool,
    #[serde(default = "yes")]
    pub was_click_through: bool,
    #[serde(flatten)]
    pub extra: Map<String, Value>,
}

impl SavedPin {
    /// A saved pin belongs to a live window of the same app showing the same
    /// title. Matching on the app alone pinned whichever of its windows came
    /// first.
    pub fn matches(&self, process: &str, title: &str) -> bool {
        !self.title.is_empty() && self.title == title && self.process_name.eq_ignore_ascii_case(process)
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Shortcuts {
    pub toggle_pin: String,
    pub opacity_up: String,
    pub opacity_down: String,
    pub toggle_window: String,
    #[serde(flatten)]
    pub extra: Map<String, Value>,
}

impl Default for Shortcuts {
    fn default() -> Self {
        Shortcuts {
            toggle_pin: "super+ctrl+KeyT".into(),
            opacity_up: "super+ctrl+Equal".into(),
            opacity_down: "super+ctrl+Minus".into(),
            toggle_window: "super+ctrl+KeyP".into(),
            extra: Map::new(),
        }
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Settings {
    pub enable_sound: bool,
    pub show_notifications: bool,
    pub has_seen_tray_notice: bool,
    pub start_with_windows: bool,
    pub shortcuts: Shortcuts,
    #[serde(flatten)]
    pub extra: Map<String, Value>,
}

impl Default for Settings {
    fn default() -> Self {
        Settings {
            enable_sound: true,
            show_notifications: false,
            has_seen_tray_notice: false,
            start_with_windows: false,
            shortcuts: Shortcuts::default(),
            extra: Map::new(),
        }
    }
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct State {
    /// An object keyed "<process>:<index>" — the schema the Tauri build chose.
    #[serde(with = "pin_map")]
    pub pins: Vec<SavedPin>,
    pub settings: Settings,
    #[serde(flatten)]
    pub extra: Map<String, Value>,
}

/// `pins` is a JSON object keyed "<process>:<index>"; the index after the last
/// colon keeps the pin order.
mod pin_map {
    use super::SavedPin;
    use serde::{Deserialize, Deserializer, Serialize, Serializer};
    use serde_json::{Map, Value};

    pub fn serialize<S: Serializer>(pins: &[SavedPin], s: S) -> Result<S::Ok, S::Error> {
        let mut map = Map::new();
        for (i, pin) in pins.iter().enumerate() {
            let value = serde_json::to_value(pin).map_err(serde::ser::Error::custom)?;
            map.insert(format!("{}:{i}", pin.process_name), value);
        }
        map.serialize(s)
    }

    pub fn deserialize<'de, D: Deserializer<'de>>(d: D) -> Result<Vec<SavedPin>, D::Error> {
        let map = Map::<String, Value>::deserialize(d)?;
        let index = |key: &str| key.rsplit(':').next().and_then(|n| n.parse().ok()).unwrap_or(u64::MAX);
        let mut entries: Vec<(u64, SavedPin)> = map
            .into_iter()
            .filter_map(|(k, v)| Some((index(&k), serde_json::from_value::<SavedPin>(v).ok()?)))
            .filter(|(_, pin)| !pin.process_name.is_empty())
            .collect();
        entries.sort_by_key(|(k, _)| *k);
        Ok(entries.into_iter().map(|(_, p)| p).collect())
    }
}

/// %LOCALAPPDATA%\PinIt — pinned.json and pinit.log. (PinIt-dev for a
/// development build, so it never touches an installed copy's pins.)
pub fn data_dir() -> PathBuf {
    let base = std::env::var_os("LOCALAPPDATA").map(PathBuf::from).unwrap_or_else(std::env::temp_dir);
    base.join(format!("PinIt{}", crate::instance_suffix()))
}

fn file() -> PathBuf {
    data_dir().join("pinned.json")
}

pub fn load() -> State {
    std::fs::read(file())
        .ok()
        .and_then(|bytes| serde_json::from_slice(&bytes).ok())
        .unwrap_or_default()
}

/// Write via a temporary file and rename, so a crash or power cut mid-write
/// can't leave a half-written pinned.json.
pub fn save(state: &State) {
    let dir = data_dir();
    let _ = std::fs::create_dir_all(&dir);
    let Ok(json) = serde_json::to_vec_pretty(state) else { return };
    let tmp = dir.join("pinned.json.tmp");
    if std::fs::write(&tmp, json).is_ok() {
        let _ = std::fs::rename(&tmp, file());
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn reads_the_qt_and_tauri_schema() {
        let json = r#"{
            "pins": {
                "b.exe:1": {"process_name": "b.exe", "title": "B", "opacity": 128},
                "a.exe:0": {"process_name": "a.exe", "title": "A", "opacity": 255,
                      "click_through": true, "was_layered": false, "future_key": 7}
            },
            "settings": {"enable_sound": false, "shortcuts": {"toggle_pin": "alt+KeyQ"}}
        }"#;
        let state: State = serde_json::from_str(json).unwrap();
        assert_eq!(state.pins.len(), 2);
        assert_eq!(state.pins[0].title, "A");
        assert!(state.pins[0].click_through);
        assert!(!state.pins[0].was_layered);
        assert!(state.pins[1].was_layered, "missing flags default to true");
        assert!(!state.settings.enable_sound);
        assert_eq!(state.settings.shortcuts.toggle_pin, "alt+KeyQ");
        assert_eq!(state.settings.shortcuts.toggle_window, "super+ctrl+KeyP");

        // Unknown keys survive a round trip.
        let again: State = serde_json::from_value(serde_json::to_value(&state).unwrap()).unwrap();
        assert_eq!(again, state);
        assert_eq!(again.pins[0].extra["future_key"], 7);
    }

    #[test]
    fn empty_or_broken_files_give_defaults() {
        let state: State = serde_json::from_str("{}").unwrap();
        assert!(state.pins.is_empty());
        assert!(state.settings.enable_sound);
        assert!(serde_json::from_str::<State>("not json").is_err());
    }

    #[test]
    fn matching_needs_app_and_title() {
        let pin: SavedPin = serde_json::from_str(r#"{"process_name":"Code.exe","title":"x"}"#).unwrap();
        assert!(pin.matches("code.exe", "x"));
        assert!(!pin.matches("code.exe", "y"));
        assert!(!pin.matches("other.exe", "x"));
    }
}
