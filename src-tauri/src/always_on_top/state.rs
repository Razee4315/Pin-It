//! State management for tracking pinned windows.

use once_cell::sync::Lazy;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::sync::RwLock;
use windows::Win32::Foundation::HWND;

/// Global state for tracking all pinned windows
static PINNED_WINDOWS: Lazy<RwLock<HashMap<isize, PinnedWindow>>> =
    Lazy::new(|| RwLock::new(HashMap::new()));

/// Information about a pinned window
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct PinnedWindow {
    /// Window handle (as isize for serialization)
    pub hwnd: isize,
    /// Window title at time of pinning
    pub title: String,
    /// Process name
    pub process_name: String,
    /// Current opacity (0-255)
    pub opacity: u8,
    /// Original opacity before modification
    pub original_opacity: Option<u8>,
}

/// Global state manager
pub struct PinState;

impl PinState {
    /// Add a window to the pinned list
    pub fn add(hwnd: HWND, title: String, process_name: String) {
        let mut windows = PINNED_WINDOWS.write().unwrap_or_else(|e| e.into_inner());
        windows.insert(
            hwnd.0 as isize,
            PinnedWindow {
                hwnd: hwnd.0 as isize,
                title,
                process_name,
                opacity: 255,
                original_opacity: None,
            },
        );
    }

    /// Remove a window from the pinned list
    pub fn remove(hwnd: HWND) -> Option<PinnedWindow> {
        let mut windows = PINNED_WINDOWS.write().unwrap_or_else(|e| e.into_inner());
        windows.remove(&(hwnd.0 as isize))
    }

    /// Check if a window is pinned
    pub fn is_pinned(hwnd: HWND) -> bool {
        let windows = PINNED_WINDOWS.read().unwrap_or_else(|e| e.into_inner());
        windows.contains_key(&(hwnd.0 as isize))
    }

    /// Get a pinned window's info
    #[allow(dead_code)]
    pub fn get(hwnd: HWND) -> Option<PinnedWindow> {
        let windows = PINNED_WINDOWS.read().unwrap_or_else(|e| e.into_inner());
        windows.get(&(hwnd.0 as isize)).cloned()
    }

    /// Update opacity for a pinned window
    pub fn set_opacity(hwnd: HWND, opacity: u8) {
        let mut windows = PINNED_WINDOWS.write().unwrap_or_else(|e| e.into_inner());
        if let Some(window) = windows.get_mut(&(hwnd.0 as isize)) {
            if window.original_opacity.is_none() {
                window.original_opacity = Some(window.opacity);
            }
            window.opacity = opacity;
        }
    }

    /// Get all pinned windows
    pub fn get_all() -> Vec<PinnedWindow> {
        let windows = PINNED_WINDOWS.read().unwrap_or_else(|e| e.into_inner());
        windows.values().cloned().collect()
    }

    /// Remove stale windows whose handles are no longer valid
    pub fn cleanup_stale() {
        let mut windows = PINNED_WINDOWS.write().unwrap_or_else(|e| e.into_inner());
        windows.retain(|_, win| {
            let hwnd = HWND(win.hwnd as *mut std::ffi::c_void);
            super::pin_manager::is_valid_window(hwnd)
        });
    }

    /// Clear pinned state for a destroyed window
    pub fn cleanup(hwnd: HWND) {
        Self::remove(hwnd);
    }
}
