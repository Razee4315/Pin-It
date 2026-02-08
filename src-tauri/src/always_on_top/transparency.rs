//! Transparency control using SetLayeredWindowAttributes.
//!
//! Allows adjusting window opacity for pinned windows.

use super::error::PinError;
use super::state::PinState;
use windows::Win32::Foundation::{COLORREF, HWND};
use windows::Win32::UI::WindowsAndMessaging::{
    GetLayeredWindowAttributes, GetWindowLongW, SetLayeredWindowAttributes, SetWindowLongW,
    SetWindowPos, GWL_EXSTYLE, LWA_ALPHA, SWP_FRAMECHANGED, SWP_NOMOVE, SWP_NOSIZE,
    SWP_NOZORDER, WS_EX_LAYERED,
};

/// Minimum opacity percentage (20%)
const MIN_OPACITY_PERCENT: u8 = 20;
/// Maximum opacity percentage (100%)  
const MAX_OPACITY_PERCENT: u8 = 100;

/// Set window opacity as percentage (0-100)
pub fn set_opacity(hwnd: HWND, percent: u8) -> Result<(), PinError> {
    let percent = percent.clamp(MIN_OPACITY_PERCENT, MAX_OPACITY_PERCENT);

    unsafe {
        // Get current extended style
        let ex_style = GetWindowLongW(hwnd, GWL_EXSTYLE);

        // Add WS_EX_LAYERED if not present
        if (ex_style as u32 & WS_EX_LAYERED.0) == 0 {
            SetWindowLongW(hwnd, GWL_EXSTYLE, ex_style | WS_EX_LAYERED.0 as i32);
        }

        // Calculate alpha (0-255)
        let alpha = ((255u32 * percent as u32) / 100) as u8;

        // Set alpha
        SetLayeredWindowAttributes(hwnd, COLORREF(0), alpha, LWA_ALPHA)
            .map_err(|e| PinError::TransparencyFailed(e.to_string()))?;

        // Update state
        PinState::set_opacity(hwnd, alpha);
    }

    Ok(())
}

/// Adjust opacity by delta percentage (can be negative)
pub fn adjust_opacity(hwnd: HWND, delta: i32) -> Result<u8, PinError> {
    let current_percent = get_opacity_percent(hwnd);
    let new_percent = (current_percent as i32 + delta).clamp(
        MIN_OPACITY_PERCENT as i32,
        MAX_OPACITY_PERCENT as i32,
    ) as u8;

    set_opacity(hwnd, new_percent)?;
    Ok(new_percent)
}

/// Get current opacity as percentage
pub fn get_opacity_percent(hwnd: HWND) -> u8 {
    unsafe {
        let ex_style = GetWindowLongW(hwnd, GWL_EXSTYLE);

        // If not layered, it's fully opaque
        if (ex_style as u32 & WS_EX_LAYERED.0) == 0 {
            return 100;
        }

        let mut alpha: u8 = 255;
        let mut _color = COLORREF(0);
        let mut _flags = LWA_ALPHA;

        if GetLayeredWindowAttributes(hwnd, Some(&mut _color), Some(&mut alpha), Some(&mut _flags))
            .is_ok()
        {
            ((alpha as u32 * 100) / 255) as u8
        } else {
            100
        }
    }
}

/// Restore window to full opacity and remove WS_EX_LAYERED
#[allow(dead_code)]
pub fn restore_opacity(hwnd: HWND) -> Result<(), PinError> {
    unsafe {
        // Set to fully opaque first
        SetLayeredWindowAttributes(hwnd, COLORREF(0), 255, LWA_ALPHA)
            .map_err(|e| PinError::TransparencyFailed(e.to_string()))?;

        // Remove WS_EX_LAYERED to fully restore
        let ex_style = GetWindowLongW(hwnd, GWL_EXSTYLE);
        if (ex_style as u32 & WS_EX_LAYERED.0) != 0 {
            SetWindowLongW(hwnd, GWL_EXSTYLE, ex_style & !(WS_EX_LAYERED.0 as i32));

            // Force redraw
            let _ = SetWindowPos(
                hwnd,
                None,
                0,
                0,
                0,
                0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED,
            );
        }
    }
    Ok(())
}
