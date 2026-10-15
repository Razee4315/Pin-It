//! autostart — "Start PinIt with Windows", kept where Windows itself reads
//! it: the per-user Run key. The registry is the single source of truth (the
//! installer can switch autostart on too, so a flag in pinned.json would
//! drift out of step).

use windows::Win32::System::Registry::{
    RegDeleteKeyValueW, RegGetValueW, RegSetKeyValueW, HKEY_CURRENT_USER, REG_SZ, RRF_RT_REG_SZ,
};
use windows::core::{w, HSTRING};

fn value_name() -> HSTRING {
    HSTRING::from(format!("PinIt{}", crate::instance_suffix()))
}

const RUN_KEY: windows::core::PCWSTR = w!("Software\\Microsoft\\Windows\\CurrentVersion\\Run");

pub fn is_enabled() -> bool {
    let mut size = 0u32;
    unsafe {
        RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, &value_name(), RRF_RT_REG_SZ, None, None, Some(&mut size)).is_ok()
    }
}

/// Launch hidden in the tray at login, not with the window open.
pub fn set_enabled(enabled: bool) -> bool {
    unsafe {
        if !enabled {
            let r = RegDeleteKeyValueW(HKEY_CURRENT_USER, RUN_KEY, &value_name());
            return r.is_ok() || !is_enabled();
        }
        let Ok(exe) = std::env::current_exe() else { return false };
        let command = format!("\"{}\" --minimized", exe.display());
        let wide: Vec<u16> = command.encode_utf16().chain(std::iter::once(0)).collect();
        RegSetKeyValueW(
            HKEY_CURRENT_USER,
            RUN_KEY,
            &value_name(),
            REG_SZ.0,
            Some(wide.as_ptr() as *const _),
            (wide.len() * 2) as u32,
        )
        .is_ok()
    }
}
