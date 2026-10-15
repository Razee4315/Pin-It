//! logfile — a small diagnostic log at %LOCALAPPDATA%\PinIt\pinit.log, in the
//! Qt build's format, so user-reported problems can be looked into.
//!
//! Only process names go in, never window titles: a title can carry a
//! document name, a mail subject or a chat message.
//!
//! Once the file passes 512 KB it becomes pinit.log.old and a fresh one
//! starts; the older .old is dropped.

use std::fmt;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::sync::{Mutex, OnceLock};

const MAX_BYTES: u64 = 512 * 1024;

static PATH: OnceLock<PathBuf> = OnceLock::new();
static LOCK: Mutex<()> = Mutex::new(());

/// Start logging. Call once, first thing in main.
pub fn init() {
    let dir = crate::store::data_dir();
    let _ = std::fs::create_dir_all(&dir);
    let _ = PATH.set(dir.join("pinit.log"));

    // A panic aborts the process (panic = "abort"), so this is the only
    // chance to record where it happened.
    let default_hook = std::panic::take_hook();
    std::panic::set_hook(Box::new(move |info| {
        write('F', format_args!("{info}"));
        default_hook(info);
    }));
}

macro_rules! info {
    ($($arg:tt)*) => { $crate::logfile::write('I', format_args!($($arg)*)) };
}
macro_rules! warning {
    ($($arg:tt)*) => { $crate::logfile::write('W', format_args!($($arg)*)) };
}
pub(crate) use {info, warning};

pub fn write(level: char, msg: fmt::Arguments) {
    let Some(path) = PATH.get() else { return };
    let line = format_line(&timestamp(), level, &msg.to_string());
    let _guard = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    rotate(path, MAX_BYTES);
    if let Ok(mut f) = std::fs::OpenOptions::new().create(true).append(true).open(path) {
        let _ = f.write_all(line.as_bytes());
    }
}

/// "2026-10-15 18:52:01.123 [I] message", one line per entry.
fn format_line(time: &str, level: char, msg: &str) -> String {
    let msg = msg.trim_end().replace(['\r', '\n'], " ");
    format!("{time} [{level}] {msg}\r\n")
}

/// Move `path` to `path.old` once it is bigger than `max`.
fn rotate(path: &Path, max: u64) {
    if std::fs::metadata(path).is_ok_and(|m| m.len() > max) {
        let mut old = path.as_os_str().to_owned();
        old.push(".old");
        let _ = std::fs::remove_file(&old);
        let _ = std::fs::rename(path, &old);
    }
}

fn timestamp() -> String {
    // Local time, as the Qt build wrote it.
    let t = unsafe { windows::Win32::System::SystemInformation::GetLocalTime() };
    format!(
        "{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}",
        t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn one_line_per_entry() {
        assert_eq!(format_line("T", 'I', "PinIt starting"), "T [I] PinIt starting\r\n");
        assert_eq!(format_line("T", 'F', "panicked at x:\nboom\n"), "T [F] panicked at x: boom\r\n");
    }

    #[test]
    fn rotates_past_the_limit() {
        let dir = std::env::temp_dir().join(format!("pinit-log-test-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let log = dir.join("pinit.log");
        let old = dir.join("pinit.log.old");
        std::fs::write(&old, "older").unwrap();

        std::fs::write(&log, "12345").unwrap();
        rotate(&log, 10);
        assert!(log.exists(), "small enough: left alone");

        std::fs::write(&log, "0123456789ab").unwrap();
        rotate(&log, 10);
        assert!(!log.exists());
        assert_eq!(std::fs::read_to_string(&old).unwrap(), "0123456789ab");

        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn timestamp_shape() {
        let t = timestamp();
        assert_eq!(t.len(), "2026-10-15 18:52:01.123".len());
        assert_eq!(&t[4..5], "-");
        assert_eq!(&t[19..20], ".");
    }
}
