//! Custom error types for the Always on Top module.

use thiserror::Error;

#[derive(Error, Debug)]
pub enum PinError {
    #[error("No foreground window found")]
    NoForegroundWindow,

    #[error("Failed to set window position: {0}")]
    SetWindowPosFailed(String),

    #[allow(dead_code)]
    #[error("Failed to get window info: {0}")]
    GetWindowInfoFailed(String),

    #[error("Failed to set property: {0}")]
    SetPropertyFailed(String),

    #[error("Failed to set transparency: {0}")]
    TransparencyFailed(String),

    #[error("Window is excluded from pinning")]
    WindowExcluded,

    #[allow(dead_code)]
    #[error("Windows API error: {0}")]
    WindowsApiError(String),
}

impl serde::Serialize for PinError {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.serialize_str(&self.to_string())
    }
}
