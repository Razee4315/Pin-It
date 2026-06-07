//! Custom error types for the Always on Top module.

use thiserror::Error;

#[derive(Error, Debug)]
pub enum PinError {
    #[error("No foreground window found")]
    NoForegroundWindow,

    #[error("Failed to set window position: {0}")]
    SetWindowPosFailed(String),

    #[error("Cannot pin {0} — it may be running as administrator")]
    AccessDenied(String),

    #[error("Failed to set property: {0}")]
    SetPropertyFailed(String),

    #[error("Failed to set transparency: {0}")]
    TransparencyFailed(String),
}

impl serde::Serialize for PinError {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.serialize_str(&self.to_string())
    }
}
