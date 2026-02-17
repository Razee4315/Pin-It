//! Window event hooks using SetWinEventHook.
//!
//! Tracks window location changes, minimize/restore, and destruction
//! to keep borders synced and clean up state.

use super::pin_manager;
use super::state::PinState;
use once_cell::sync::Lazy;
use std::sync::Mutex;
use tauri::{AppHandle, Emitter};
use windows::Win32::Foundation::HWND;
use windows::Win32::UI::Accessibility::{SetWinEventHook, UnhookWinEvent, HWINEVENTHOOK};
use windows::Win32::UI::WindowsAndMessaging::{
    SetWindowPos, EVENT_OBJECT_DESTROY, EVENT_OBJECT_FOCUS, EVENT_OBJECT_LOCATIONCHANGE,
    EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_MINIMIZEEND, EVENT_SYSTEM_MINIMIZESTART,
    EVENT_SYSTEM_MOVESIZEEND, HWND_TOPMOST, SWP_NOMOVE, SWP_NOSIZE,
};

/// WINEVENT flags - not exported by windows crate
const WINEVENT_OUTOFCONTEXT: u32 = 0x0000;
const WINEVENT_SKIPOWNPROCESS: u32 = 0x0002;

/// Thread-safe storage for event hooks
static EVENT_HOOKS: Lazy<Mutex<Vec<isize>>> = Lazy::new(|| Mutex::new(Vec::new()));

/// Global app handle for emitting events from the C callback
static APP_HANDLE: Lazy<Mutex<Option<AppHandle>>> = Lazy::new(|| Mutex::new(None));

/// Store the app handle for use in event callbacks
pub fn set_app_handle(handle: AppHandle) {
    let mut app = APP_HANDLE.lock().unwrap_or_else(|e| e.into_inner());
    *app = Some(handle);
}

/// Emit an event via the stored app handle
fn emit_event(event: &str) {
    if let Some(handle) = APP_HANDLE.lock().unwrap_or_else(|e| e.into_inner()).as_ref() {
        let _ = handle.emit(event, ());
    }
}

/// Initialize window event hooks
pub fn init_event_hooks() -> Result<(), String> {
    let mut hooks = EVENT_HOOKS.lock().unwrap_or_else(|e| e.into_inner());

    // Already initialized
    if !hooks.is_empty() {
        return Ok(());
    }

    unsafe {
        let events = [
            EVENT_OBJECT_LOCATIONCHANGE,
            EVENT_SYSTEM_MINIMIZESTART,
            EVENT_SYSTEM_MINIMIZEEND,
            EVENT_SYSTEM_MOVESIZEEND,
            EVENT_SYSTEM_FOREGROUND,
            EVENT_OBJECT_DESTROY,
            EVENT_OBJECT_FOCUS,
        ];

        for event in events {
            let hook = SetWinEventHook(
                event,
                event,
                None,
                Some(win_event_callback),
                0,
                0,
                WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS,
            );

            if hook.0.is_null() {
                log::warn!("Failed to set event hook for event {}", event);
            } else {
                hooks.push(hook.0 as isize);
            }
        }
    }

    log::info!("Window event hooks initialized");
    Ok(())
}

/// Cleanup event hooks on shutdown
pub fn cleanup_event_hooks() {
    let mut hooks = EVENT_HOOKS.lock().unwrap_or_else(|e| e.into_inner());
    for hook_ptr in hooks.drain(..) {
        unsafe {
            let hook = HWINEVENTHOOK(hook_ptr as *mut std::ffi::c_void);
            let _ = UnhookWinEvent(hook);
        }
    }
    log::info!("Window event hooks cleaned up");
}

/// Callback for all window events
unsafe extern "system" fn win_event_callback(
    _hook: HWINEVENTHOOK,
    event: u32,
    hwnd: HWND,
    id_object: i32,
    _id_child: i32,
    _event_thread: u32,
    _event_time: u32,
) {
    // Only handle window-level events (id_object == 0)
    if id_object != 0 {
        return;
    }

    // Only process events for windows we're tracking
    if !PinState::is_pinned(hwnd) {
        return;
    }

    match event {
        EVENT_OBJECT_LOCATIONCHANGE => {
            // Window moved or resized - no action needed currently
        }
        EVENT_SYSTEM_MINIMIZESTART => {
            // Window minimized - no action needed currently
        }
        EVENT_SYSTEM_MINIMIZEEND => {
            // Window restored from minimize - re-enforce topmost
            // Win11 can strip TOPMOST after minimize/restore cycles
            re_enforce_topmost(hwnd);
        }
        EVENT_SYSTEM_MOVESIZEEND => {
            // Window finished moving/resizing - re-enforce topmost
            re_enforce_topmost(hwnd);
        }
        EVENT_OBJECT_DESTROY => {
            // Window destroyed - cleanup state
            PinState::cleanup(hwnd);
            // Also clean up any other stale windows
            PinState::cleanup_stale();
            log::info!("Cleaned up destroyed window: {}", hwnd.0 as isize);
            // Notify frontend to refresh the pinned windows list
            emit_event("window-destroyed");
        }
        EVENT_OBJECT_FOCUS | EVENT_SYSTEM_FOREGROUND => {
            // Window gained focus - verify topmost is still set
            // Win11's DWM compositor and Snap Layouts can strip TOPMOST
            re_enforce_topmost(hwnd);
        }
        _ => {}
    }
}

/// Re-apply HWND_TOPMOST if Windows stripped it (common on Win11)
unsafe fn re_enforce_topmost(hwnd: HWND) {
    if !pin_manager::is_topmost(hwnd) {
        if pin_manager::is_valid_window(hwnd) {
            let _ = SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            log::debug!("Re-enforced topmost on window: {}", hwnd.0 as isize);
        } else {
            // Window handle is no longer valid, clean up
            PinState::cleanup(hwnd);
            emit_event("window-destroyed");
        }
    }
}
