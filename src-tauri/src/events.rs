//! Event names emitted to the frontend.
//!
//! Single source of truth on the Rust side — the frontend listens for these
//! exact strings (see src/types.ts).

pub const PIN_TOGGLED: &str = "pin-toggled";
pub const PIN_ERROR: &str = "pin-error";
pub const OPACITY_CHANGED: &str = "opacity-changed";
pub const WINDOW_DESTROYED: &str = "window-destroyed";
pub const SHORTCUTS_UPDATED: &str = "shortcuts-updated";
