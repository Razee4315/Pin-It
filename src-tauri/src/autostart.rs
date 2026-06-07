//! Auto-start management via Windows Registry.
//!
//! Adds/removes PinIt from HKCU\Software\Microsoft\Windows\CurrentVersion\Run

use windows::core::PCWSTR;
use windows::Win32::Foundation::ERROR_SUCCESS;
use windows::Win32::System::Registry::{
    RegCloseKey, RegDeleteValueW, RegOpenKeyExW, RegQueryValueExW, RegSetValueExW, HKEY,
    HKEY_CURRENT_USER, KEY_READ, KEY_SET_VALUE, REG_SAM_FLAGS, REG_SZ,
};

const RUN_KEY: &str = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const APP_NAME: &str = "PinIt";

/// Encode a Rust string to a null-terminated wide string
fn to_wide(s: &str) -> Vec<u16> {
    s.encode_utf16().chain(std::iter::once(0)).collect()
}

/// Open the HKCU Run key with the given access, run `f` against it, and
/// always close the key afterwards. Deduplicates the open/close boilerplate
/// shared by is_enabled/enable/disable.
fn with_run_key<T>(access: REG_SAM_FLAGS, f: impl FnOnce(HKEY) -> T) -> Result<T, String> {
    unsafe {
        let key_path = to_wide(RUN_KEY);
        let mut hkey = HKEY::default();

        let result = RegOpenKeyExW(
            HKEY_CURRENT_USER,
            PCWSTR(key_path.as_ptr()),
            0,
            access,
            &mut hkey,
        );

        if result != ERROR_SUCCESS {
            return Err(format!("Failed to open registry key: {:?}", result));
        }

        let value = f(hkey);
        let _ = RegCloseKey(hkey);
        Ok(value)
    }
}

/// Check if auto-start is enabled
pub fn is_enabled() -> bool {
    with_run_key(KEY_READ, |hkey| unsafe {
        let value_name = to_wide(APP_NAME);
        RegQueryValueExW(hkey, PCWSTR(value_name.as_ptr()), None, None, None, None) == ERROR_SUCCESS
    })
    .unwrap_or(false)
}

/// Enable auto-start (add to registry)
pub fn enable() -> Result<(), String> {
    // Quote the path: unquoted Run-key values containing spaces are ambiguous
    // ("C:\Program Files\..." could resolve to "C:\Program.exe")
    let exe_path = format!(
        "\"{}\"",
        std::env::current_exe()
            .map_err(|e| format!("Could not get executable path: {}", e))?
            .to_string_lossy()
    );

    let result = with_run_key(KEY_SET_VALUE, |hkey| unsafe {
        let value_name = to_wide(APP_NAME);
        let exe_wide = to_wide(&exe_path);
        let exe_bytes: &[u8] =
            std::slice::from_raw_parts(exe_wide.as_ptr() as *const u8, exe_wide.len() * 2);

        RegSetValueExW(
            hkey,
            PCWSTR(value_name.as_ptr()),
            0,
            REG_SZ,
            Some(exe_bytes),
        )
    })?;

    if result != ERROR_SUCCESS {
        return Err(format!("Failed to set registry value: {:?}", result));
    }

    log::info!("Auto-start enabled");
    Ok(())
}

/// Disable auto-start (remove from registry)
pub fn disable() -> Result<(), String> {
    with_run_key(KEY_SET_VALUE, |hkey| unsafe {
        let value_name = to_wide(APP_NAME);
        let _ = RegDeleteValueW(hkey, PCWSTR(value_name.as_ptr()));
    })?;

    log::info!("Auto-start disabled");
    Ok(())
}
