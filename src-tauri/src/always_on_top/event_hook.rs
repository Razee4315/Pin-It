//! Window event hooks using SetWinEventHook.
//!
//! Tracks window location changes, minimize/restore, and destruction
//! to keep borders synced and clean up state.

use super::state::PinState;
use once_cell::sync::Lazy;
use std::sync::Mutex;
use windows::Win32::Foundation::HWND;
use windows::Win32::UI::Accessibility::{SetWinEventHook, UnhookWinEvent, HWINEVENTHOOK};
use windows::Win32::UI::WindowsAndMessaging::{
    EVENT_OBJECT_DESTROY, EVENT_OBJECT_FOCUS, EVENT_OBJECT_LOCATIONCHANGE,
    EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_MINIMIZEEND, EVENT_SYSTEM_MINIMIZESTART,
    EVENT_SYSTEM_MOVESIZEEND,
};

/// WINEVENT flags - not exported by windows crate
const WINEVENT_OUTOFCONTEXT: u32 = 0x0000;
const WINEVENT_SKIPOWNPROCESS: u32 = 0x0002;

/// Thread-safe storage for event hooks
static EVENT_HOOKS: Lazy<Mutex<Vec<isize>>> = Lazy::new(|| Mutex::new(Vec::new()));

/// Initialize window event hooks
pub fn init_event_hooks() -> Result<(), String> {
    let mut hooks = EVENT_HOOKS.lock().unwrap();
    
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
#[allow(dead_code)]
pub fn cleanup_event_hooks() {
    let mut hooks = EVENT_HOOKS.lock().unwrap();
    for hook_ptr in hooks.drain(..) {
        unsafe {
            let hook = HWINEVENTHOOK(hook_ptr as *mut std::ffi::c_void);
            let _ = UnhookWinEvent(hook);
        }
    }
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
            // Window moved or resized - update border position
            // TODO: Update border overlay position
        }
        EVENT_SYSTEM_MINIMIZESTART => {
            // Window minimized - hide border
            // TODO: Hide border overlay
        }
        EVENT_SYSTEM_MINIMIZEEND => {
            // Window restored - show border
            // TODO: Show border overlay
        }
        EVENT_OBJECT_DESTROY => {
            // Window destroyed - cleanup state
            PinState::cleanup(hwnd);
            log::info!("Cleaned up destroyed window: {}", hwnd.0 as isize);
        }
        EVENT_OBJECT_FOCUS => {
            // Window gained focus - verify topmost is still set
            // Some apps may reset TOPMOST, we could re-apply here
        }
        _ => {}
    }
}
