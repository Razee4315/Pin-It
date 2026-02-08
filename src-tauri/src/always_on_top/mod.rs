//! Always on Top module - Core functionality for pinning windows
//!
//! This module provides the ability to pin windows to stay always on top,
//! using Windows APIs similar to PowerToys' implementation.

pub mod pin_manager;
pub mod state;
pub mod hotkey;
pub mod event_hook;
pub mod transparency;
pub mod error;

pub use error::PinError;
