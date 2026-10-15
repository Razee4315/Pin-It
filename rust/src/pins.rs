//! pins — which windows are pinned, and keeping them that way.
//!
//! Pure bookkeeping over `win`: no UI, no timers. The app calls `reenforce`
//! on a timer and when the foreground window changes, and saves whatever
//! `saved()` returns.

use crate::store::SavedPin;
use crate::win::{self, Hwnd};

#[derive(Clone, Debug)]
pub struct Pinned {
    pub hwnd: Hwnd,
    pub title: String,
    pub process: String,
    pub opacity: i32, // percent
    pub click_through: bool,
    was_layered: bool,       // had WS_EX_LAYERED before PinIt touched it
    was_topmost: bool,       // was already always-on-top (Task Manager, players)
    was_click_through: bool, // already let clicks through
    opacity_changed: bool,   // PinIt changed its opacity, so undo that on unpin
}

#[derive(Debug, PartialEq)]
pub enum PinError {
    NoWindow,
    Gone,
    NotPinnable,
    Blocked(String),
    NotPinned,
}

impl PinError {
    pub fn message(&self) -> String {
        match self {
            PinError::NoWindow => "No window to pin. Click a window first.".into(),
            PinError::Gone => "That window no longer exists.".into(),
            PinError::NotPinnable => "That window can't be pinned.".into(),
            PinError::Blocked(app) => {
                let app = if app.is_empty() { "this window" } else { app };
                format!("Can't pin {app}. It may be running as administrator.")
            }
            PinError::NotPinned => "Pin this window first to change its opacity.".into(),
        }
    }
}

#[derive(Default)]
pub struct Reenforced {
    pub list_changed: bool,
    pub titles_changed: bool,
    pub restored: Vec<Hwnd>,
}

#[derive(Default)]
pub struct Pins {
    pinned: Vec<Pinned>,
    pending: Vec<SavedPin>,
    session_ending: bool,
}

impl Pins {
    pub fn pinned(&self) -> &[Pinned] {
        &self.pinned
    }

    pub fn pending(&self) -> &[SavedPin] {
        &self.pending
    }

    pub fn is_pinned(&self, hwnd: Hwnd) -> bool {
        self.pinned.iter().any(|p| p.hwnd == hwnd)
    }

    pub fn is_active(&self) -> bool {
        !self.pinned.is_empty() || !self.pending.is_empty()
    }

    fn find(&mut self, hwnd: Hwnd) -> Option<&mut Pinned> {
        self.pinned.iter_mut().find(|p| p.hwnd == hwnd)
    }

    pub fn pin(&mut self, hwnd: Hwnd) -> Result<(), PinError> {
        if self.is_pinned(hwnd) {
            return Ok(());
        }
        if !win::is_valid_window(hwnd) {
            return Err(PinError::Gone);
        }
        if !win::is_pinnable(hwnd) {
            return Err(PinError::NotPinnable); // desktop, taskbar, Start, PinIt
        }
        let title = win::window_title(hwnd);
        let process = win::process_name(hwnd);
        let was_topmost = win::is_topmost(hwnd);
        // UIPI silently ignores SetWindowPos on elevated windows; checking that
        // the style took is how we notice.
        if !win::apply_topmost(hwnd) || !win::is_topmost(hwnd) {
            crate::logfile::warning!("Pin failed for {process} (likely elevated/UIPI)");
            return Err(PinError::Blocked(process));
        }
        crate::logfile::info!("Pinned a window of {process}");
        let was_click_through = win::is_click_through(hwnd);
        self.pending.retain(|p| !p.matches(&process, &title));
        self.pinned.push(Pinned {
            hwnd,
            title,
            process,
            opacity: 100,
            click_through: was_click_through,
            was_layered: win::is_layered(hwnd),
            was_topmost,
            was_click_through,
            opacity_changed: false,
        });
        Ok(())
    }

    /// Give a window back the way PinIt found it. False if it has closed.
    fn release(p: &Pinned) -> bool {
        if !win::is_valid_window(p.hwnd) {
            return false;
        }
        if p.click_through && !p.was_click_through {
            win::set_click_through(p.hwnd, false);
        }
        if p.opacity_changed {
            win::restore_opacity(p.hwnd, p.was_layered);
        }
        if !p.was_topmost {
            win::remove_topmost(p.hwnd);
        }
        true
    }

    pub fn unpin(&mut self, hwnd: Hwnd) -> Option<Pinned> {
        let i = self.pinned.iter().position(|p| p.hwnd == hwnd)?;
        let p = self.pinned.remove(i);
        Self::release(&p);
        Some(p)
    }

    /// Unpin every window and stop waiting for the pending ones.
    pub fn unpin_all(&mut self) -> usize {
        let n = self.pinned.len();
        for p in self.pinned.drain(..) {
            Self::release(&p);
        }
        self.pending.clear();
        n
    }

    /// Ok(true) = now pinned, Ok(false) = now unpinned.
    pub fn toggle(&mut self, hwnd: Hwnd) -> Result<bool, PinError> {
        if hwnd == 0 {
            return Err(PinError::NoWindow);
        }
        if self.unpin(hwnd).is_some() {
            return Ok(false);
        }
        self.pin(hwnd).map(|_| true)
    }

    pub fn set_opacity(&mut self, hwnd: Hwnd, percent: i32) -> Result<i32, PinError> {
        let p = self.find(hwnd).ok_or(PinError::NotPinned)?;
        let percent = percent.clamp(win::MIN_OPACITY, win::MAX_OPACITY);
        if !win::set_opacity_percent(hwnd, percent) {
            return Err(PinError::Gone);
        }
        p.opacity = percent;
        p.opacity_changed = true;
        Ok(percent)
    }

    pub fn adjust_opacity(&mut self, hwnd: Hwnd, delta: i32) -> Result<i32, PinError> {
        let current = self.find(hwnd).ok_or(PinError::NotPinned)?.opacity;
        self.set_opacity(hwnd, current + delta)
    }

    pub fn set_click_through(&mut self, hwnd: Hwnd, enabled: bool) -> Result<(), PinError> {
        let opacity = self.find(hwnd).ok_or(PinError::NotPinned)?.opacity;
        // Windows only honours click-through on a layered window, and a layered
        // window needs an alpha to be drawn at all: setting the opacity does both.
        if enabled && !win::is_layered(hwnd) {
            self.set_opacity(hwnd, opacity)?;
        }
        if !win::set_click_through(hwnd, enabled) {
            return Err(PinError::Gone);
        }
        if let Some(p) = self.find(hwnd) {
            p.click_through = enabled;
        }
        Ok(())
    }

    /// Windows 11 sometimes drops the topmost flag; put it back, sweep out
    /// closed windows, follow title changes, and pin a pending window that has
    /// just opened.
    pub fn reenforce(&mut self) -> Reenforced {
        let mut out = Reenforced::default();
        // While Windows is closing every app, a window disappearing is not the
        // user closing it; dropping its pin would lose it across the restart.
        if self.session_ending {
            return out;
        }
        let before = self.pinned.len();
        self.pinned.retain(|p| win::is_valid_window(p.hwnd));
        out.list_changed = self.pinned.len() != before;

        for p in &mut self.pinned {
            if !win::is_topmost(p.hwnd) {
                win::apply_topmost(p.hwnd);
            }
            let title = win::window_title(p.hwnd);
            if !title.is_empty() && title != p.title {
                p.title = title;
                out.titles_changed = true;
            }
        }

        // Only the window in front is checked: a window that has just opened
        // is the foreground one, and one window costs next to nothing.
        let fg = win::foreground_window();
        if !self.pending.is_empty() && fg != 0 && !self.is_pinned(fg) && win::is_pinnable(fg) {
            let (title, process) = (win::window_title(fg), win::process_name(fg));
            if let Some(saved) = self.pending.iter().find(|s| s.matches(&process, &title)).cloned() {
                if self.apply_saved(fg, &saved) {
                    out.restored.push(fg);
                    out.list_changed = true;
                }
            }
        }
        out
    }

    fn apply_saved(&mut self, hwnd: Hwnd, saved: &SavedPin) -> bool {
        if self.pin(hwnd).is_err() {
            return false;
        }
        // After a crash the window still carries PinIt's changes from last
        // time, which pin() just recorded as the app's own. The saved flags
        // know better.
        if let Some(p) = self.find(hwnd) {
            p.was_layered &= saved.was_layered;
            p.was_topmost &= saved.was_topmost;
            p.was_click_through &= saved.was_click_through;
        }
        let percent = win::alpha_to_percent(saved.opacity);
        if percent < 100 {
            let _ = self.set_opacity(hwnd, percent);
        }
        if saved.click_through {
            let _ = self.set_click_through(hwnd, true);
        }
        true
    }

    /// At startup: re-pin saved windows that are open; the rest wait for
    /// their window (PinIt usually starts before the apps after a reboot).
    pub fn restore(&mut self, saved: Vec<SavedPin>) -> Vec<Hwnd> {
        self.pending = saved.clone();
        let live = win::enumerate_windows();
        let mut restored = Vec::new();
        for s in &saved {
            if let Some(w) = live.iter().find(|w| !self.is_pinned(w.hwnd) && s.matches(&w.process, &w.title)) {
                if self.apply_saved(w.hwnd, s) {
                    restored.push(w.hwnd);
                }
            }
        }
        restored
    }

    pub fn forget_pending(&mut self, index: usize) {
        if index < self.pending.len() {
            self.pending.remove(index);
        }
    }

    pub fn set_session_ending(&mut self, ending: bool) {
        self.session_ending = ending;
    }

    /// On quit: give every window back. After a manual quit the pins are
    /// forgotten; at logoff/shutdown they are kept for the next login.
    pub fn release_all(&mut self) {
        for p in &self.pinned {
            Self::release(p);
        }
        if !self.session_ending {
            self.pinned.clear();
            self.pending.clear();
        }
    }

    /// What goes into pinned.json: live pins, then the ones still waiting.
    pub fn saved(&self) -> Vec<SavedPin> {
        self.pinned
            .iter()
            .map(|p| SavedPin {
                process_name: p.process.clone(),
                title: p.title.clone(),
                opacity: win::percent_to_alpha(p.opacity) as i32,
                click_through: p.click_through,
                was_layered: p.was_layered,
                was_topmost: p.was_topmost,
                was_click_through: p.was_click_through,
                extra: Default::default(),
            })
            .chain(self.pending.iter().cloned())
            .collect()
    }
}
