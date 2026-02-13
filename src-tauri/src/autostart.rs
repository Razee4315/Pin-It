//! Auto-start management via Windows Registry.
//!
//! Adds/removes PinIt from HKCU\Software\Microsoft\Windows\CurrentVersion\Run

use windows::core::PCWSTR;
use windows::Win32::Foundation::ERROR_SUCCESS;
use windows::Win32::System::Registry::{
    RegCloseKey, RegDeleteValueW, RegOpenKeyExW, RegQueryValueExW, RegSetValueExW, HKEY,
    HKEY_CURRENT_USER, KEY_READ, KEY_SET_VALUE, REG_SZ,
};

const RUN_KEY: &str = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const APP_NAME: &str = "PinIt";

/// Encode a Rust string to a null-terminated wide string
fn to_wide(s: &str) -> Vec<u16> {
    s.encode_utf16().chain(std::iter::once(0)).collect()
}

/// Check if auto-start is enabled
pub fn is_enabled() -> bool {
    unsafe {
        let key_path = to_wide(RUN_KEY);
        let mut hkey = HKEY::default();

        let result = RegOpenKeyExW(
            HKEY_CURRENT_USER,
            PCWSTR(key_path.as_ptr()),
            0,
            KEY_READ,
            &mut hkey,
        );

        if result != ERROR_SUCCESS {
            return false;
        }

        let value_name = to_wide(APP_NAME);
        let exists = RegQueryValueExW(
            hkey,
            PCWSTR(value_name.as_ptr()),
            None,
            None,
            None,
            None,
        ) == ERROR_SUCCESS;

        let _ = RegCloseKey(hkey);
        exists
    }
}

/// Enable auto-start (add to registry)
pub fn enable() -> Result<(), String> {
    let exe_path = std::env::current_exe()
        .map_err(|e| format!("Could not get executable path: {}", e))?
        .to_string_lossy()
        .to_string();

    unsafe {
        let key_path = to_wide(RUN_KEY);
        let mut hkey = HKEY::default();

        let result = RegOpenKeyExW(
            HKEY_CURRENT_USER,
            PCWSTR(key_path.as_ptr()),
            0,
            KEY_SET_VALUE,
            &mut hkey,
        );

        if result != ERROR_SUCCESS {
            return Err(format!("Failed to open registry key: {:?}", result));
        }

        let value_name = to_wide(APP_NAME);
        let exe_wide = to_wide(&exe_path);
        let exe_bytes: &[u8] = std::slice::from_raw_parts(
            exe_wide.as_ptr() as *const u8,
            exe_wide.len() * 2,
        );

        let result = RegSetValueExW(
            hkey,
            PCWSTR(value_name.as_ptr()),
            0,
            REG_SZ,
            Some(exe_bytes),
        );

        let _ = RegCloseKey(hkey);

        if result != ERROR_SUCCESS {
            return Err(format!("Failed to set registry value: {:?}", result));
        }
    }

    log::info!("Auto-start enabled");
    Ok(())
}

/// Disable auto-start (remove from registry)
pub fn disable() -> Result<(), String> {
    unsafe {
        let key_path = to_wide(RUN_KEY);
        let mut hkey = HKEY::default();

        let result = RegOpenKeyExW(
            HKEY_CURRENT_USER,
            PCWSTR(key_path.as_ptr()),
            0,
            KEY_SET_VALUE,
            &mut hkey,
        );

        if result != ERROR_SUCCESS {
            return Err(format!("Failed to open registry key: {:?}", result));
        }

        let value_name = to_wide(APP_NAME);
        let _ = RegDeleteValueW(hkey, PCWSTR(value_name.as_ptr()));
        let _ = RegCloseKey(hkey);
    }

    log::info!("Auto-start disabled");
    Ok(())
}
