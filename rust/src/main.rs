//! PinIt — keep any window always on top on Windows.
//!
//! main wires the pieces together: the Slint window (ui/app.slint), the pin
//! bookkeeping (pins), the hidden helper window that owns the tray icon and
//! the hotkeys (host), and pinned.json (store).

#![windows_subsystem = "windows"]

mod autostart;
mod flash;
mod host;
mod logfile;
mod pins;
mod shortcuts;
mod store;
mod win;

use std::cell::RefCell;
use std::collections::HashMap;
use std::rc::Rc;
use std::time::Duration;

use raw_window_handle::{HasWindowHandle, RawWindowHandle};
use slint::{CloseRequestResponse, ComponentHandle, Image, Model, ModelRc, Rgba8Pixel, SharedPixelBuffer, SharedString, VecModel};

use crate::pins::Pins;
use crate::win::Hwnd;

slint::include_modules!();

/// "-dev" when PINIT_DEV is set: a development build then uses its own data
/// folder, Run-key value and single-instance lock, so it can run next to an
/// installed PinIt without touching it.
pub fn instance_suffix() -> &'static str {
    static SUFFIX: std::sync::OnceLock<&'static str> = std::sync::OnceLock::new();
    SUFFIX.get_or_init(|| if std::env::var_os("PINIT_DEV").is_some_and(|v| !v.is_empty()) { "-dev" } else { "" })
}

const HOTKEY_PIN: i32 = 1;
const HOTKEY_OPACITY_UP: i32 = 2;
const HOTKEY_OPACITY_DOWN: i32 = 3;
const HOTKEY_WINDOW: i32 = 4;
const OPACITY_STEP: i32 = 10;
const ICON_PX: u32 = 32;

const MENU_SHOW: u32 = 1;
const MENU_PICK: u32 = 2;
const MENU_UNPIN_ALL: u32 = 3;
const MENU_QUIT: u32 = 4;
const MENU_PINNED: u32 = 100;

#[derive(Clone, Copy, PartialEq)]
enum RowKey {
    Live(Hwnd),
    Waiting(usize),
}

struct App {
    me: std::rc::Weak<RefCell<App>>,
    ui: slint::Weak<AppWindow>,
    pins: Pins,
    state: store::State,
    rows: Vec<RowKey>,
    row_model: Rc<VecModel<PinRow>>,
    icons: HashMap<Hwnd, Option<Image>>,
    picker: Vec<(win::WindowInfo, Option<Image>)>,
    picker_shown: Vec<usize>,
    pick_model: Rc<VecModel<PickItem>>,
    edit_model: Rc<VecModel<ShortcutEdit>>,
    hotkey_problems: Vec<String>,
    visible: bool,
    reenforce_timer: slint::Timer,
    save_timer: slint::Timer,
}

type Shared = Rc<RefCell<App>>;

fn main() {
    logfile::init();
    logfile::info!("PinIt {} starting", env!("CARGO_PKG_VERSION"));
    if host::hand_off_to_running_instance() {
        logfile::info!("Another instance is running; asked it to show");
        return;
    }
    let start_hidden = std::env::args().any(|a| a == "--minimized");

    let ui = AppWindow::new().expect("could not create the PinIt window");
    let state = store::load();
    let app: Shared = Rc::new_cyclic(|me| RefCell::new(App {
        me: me.clone(),
        ui: ui.as_weak(),
        pins: Pins::default(),
        state,
        rows: Vec::new(),
        row_model: Rc::new(VecModel::default()),
        icons: HashMap::new(),
        picker: Vec::new(),
        picker_shown: Vec::new(),
        pick_model: Rc::new(VecModel::default()),
        edit_model: Rc::new(VecModel::default()),
        hotkey_problems: Vec::new(),
        visible: false,
        reenforce_timer: slint::Timer::default(),
        save_timer: slint::Timer::default(),
    }));

    {
        let a = app.borrow();
        ui.set_pins(ModelRc::from(a.row_model.clone()));
        ui.set_pick_items(ModelRc::from(a.pick_model.clone()));
        ui.set_shortcut_edits(ModelRc::from(a.edit_model.clone()));
        ui.set_key_labels(ModelRc::new(VecModel::from(
            shortcuts::key_labels().into_iter().map(SharedString::from).collect::<Vec<_>>(),
        )));
        ui.set_version(env!("CARGO_PKG_VERSION").into());
        ui.set_animations(win::animations_enabled());
        ui.set_sound(a.state.settings.enable_sound);
        ui.set_notifications(a.state.settings.show_notifications);
        ui.set_autostart(autostart::is_enabled());
    }

    {
        let app = app.clone();
        host::create(move |event| on_host_event(&app, event));
    }
    host::add_tray("PinIt");

    let shortcuts = app.borrow().state.settings.shortcuts.clone();
    let failed = apply_shortcuts(&shortcuts);
    if !failed.is_empty() {
        logfile::warning!("Hotkeys unavailable: {}", failed.join(", "));
    }
    app.borrow_mut().hotkey_problems = failed;

    // Pins from last time: open windows are re-pinned now, the rest wait.
    {
        let mut a = app.borrow_mut();
        let saved = std::mem::take(&mut a.state.pins);
        a.pins.restore(saved);
        a.save_now();
        a.pins_changed();
        a.refresh_shortcut_views();
        a.refresh_status();
    }

    connect_ui(&ui, &app);

    {
        let app = app.clone();
        ui.window().on_close_requested(move || {
            let mut a = app.borrow_mut();
            a.visible = false;
            flash::hide();
            if !a.state.settings.has_seen_tray_notice {
                a.state.settings.has_seen_tray_notice = true;
                a.save_now();
                host::notify("PinIt is still running", "Your pins stay active. Find PinIt in the tray to open it again.");
            }
            CloseRequestResponse::HideWindow
        });
    }

    if !start_hidden {
        app.borrow_mut().show_window();
    }

    slint::run_event_loop_until_quit().expect("event loop failed");
}

fn connect_ui(ui: &AppWindow, app: &Shared) {
    let a = app.clone();
    ui.on_open_picker(move || a.borrow_mut().open_picker());

    let a = app.clone();
    ui.on_filter_changed(move |_| a.borrow_mut().filter_picker());

    let a = app.clone();
    ui.on_pick(move |i| {
        let mut app = a.borrow_mut();
        let Some(&index) = app.picker_shown.get(i as usize) else { return };
        let hwnd = app.picker[index].0.hwnd;
        if let Some(ui) = app.ui.upgrade() {
            ui.set_picker_open(false);
        }
        app.toggle(hwnd, false);
    });

    let a = app.clone();
    ui.on_set_opacity(move |i, v| {
        let mut app = a.borrow_mut();
        if let Some(RowKey::Live(hwnd)) = app.rows.get(i as usize).copied() {
            if app.pins.set_opacity(hwnd, v).is_ok() {
                app.update_rows_in_place();
                app.save_soon();
            }
        }
    });

    let a = app.clone();
    ui.on_toggle_click_through(move |i| {
        let mut app = a.borrow_mut();
        if let Some(RowKey::Live(hwnd)) = app.rows.get(i as usize).copied() {
            let on = app.pins.pinned().iter().find(|p| p.hwnd == hwnd).is_some_and(|p| p.click_through);
            match app.pins.set_click_through(hwnd, !on) {
                Ok(()) => {
                    app.update_rows_in_place();
                    app.save_soon();
                    app.toast(if on { "Clicks reach this window again." } else { "Clicks now pass through this window." });
                }
                Err(e) => app.toast(&e.message()),
            }
        }
    });

    let a = app.clone();
    ui.on_unpin(move |i| {
        let mut app = a.borrow_mut();
        match app.rows.get(i as usize).copied() {
            Some(RowKey::Live(hwnd)) => {
                flash::hide();
                app.toggle(hwnd, false);
            }
            Some(RowKey::Waiting(p)) => {
                app.pins.forget_pending(p);
                app.save_now();
                app.pins_changed();
            }
            None => {}
        }
    });

    let a = app.clone();
    ui.on_row_hovered(move |i, hovered| {
        let app = a.borrow();
        match (hovered, app.rows.get(i as usize)) {
            (true, Some(RowKey::Live(hwnd))) => flash::show(*hwnd, flash::Kind::Hover),
            _ => flash::hide(),
        }
    });

    let a = app.clone();
    ui.on_unpin_all(move || a.borrow_mut().unpin_all());

    let a = app.clone();
    ui.on_setting_changed(move |name, on| {
        let mut app = a.borrow_mut();
        match name.as_str() {
            "sound" => app.state.settings.enable_sound = on,
            "notifications" => app.state.settings.show_notifications = on,
            "autostart" => {
                if !autostart::set_enabled(on) {
                    app.toast("Couldn't change the startup setting.");
                }
                if let Some(ui) = app.ui.upgrade() {
                    ui.set_autostart(autostart::is_enabled());
                }
            }
            _ => {}
        }
        app.save_now();
    });

    let a = app.clone();
    ui.on_open_shortcuts(move || a.borrow_mut().open_shortcut_editor());

    let a = app.clone();
    ui.on_shortcut_chip(move |i, which| {
        let app = a.borrow();
        if let Some(mut edit) = app.edit_model.row_data(i as usize) {
            match which.as_str() {
                "win" => edit.win = !edit.win,
                "ctrl" => edit.ctrl = !edit.ctrl,
                "alt" => edit.alt = !edit.alt,
                _ => edit.shift = !edit.shift,
            }
            app.edit_model.set_row_data(i as usize, edit);
        }
    });

    let a = app.clone();
    ui.on_shortcut_key(move |i, key| {
        let app = a.borrow();
        if let Some(mut edit) = app.edit_model.row_data(i as usize) {
            edit.key_index = key;
            app.edit_model.set_row_data(i as usize, edit);
        }
    });

    let a = app.clone();
    ui.on_save_shortcuts(move || a.borrow_mut().save_shortcut_editor());

    let a = app.clone();
    ui.on_restore_default_shortcuts(move || a.borrow_mut().fill_shortcut_editor(&store::Shortcuts::default()));

    ui.on_open_url(|url| host::open_url(&url));

    let a = app.clone();
    ui.on_theme_changed(move || a.borrow().style_title_bar());
}

fn on_host_event(app: &Shared, event: host::Event) {
    use host::Event::*;
    match event {
        Hotkey(HOTKEY_PIN) => {
            let fg = win::foreground_window();
            app.borrow_mut().toggle(fg, true);
        }
        Hotkey(id @ (HOTKEY_OPACITY_UP | HOTKEY_OPACITY_DOWN)) => {
            let delta = if id == HOTKEY_OPACITY_UP { OPACITY_STEP } else { -OPACITY_STEP };
            let mut a = app.borrow_mut();
            let fg = win::foreground_window();
            match a.pins.adjust_opacity(fg, delta) {
                Ok(_) => {
                    a.update_rows_in_place();
                    a.save_soon();
                }
                Err(e) => a.message(&e.message(), true),
            }
        }
        Hotkey(HOTKEY_WINDOW) => {
            let mut a = app.borrow_mut();
            let ours = a.main_hwnd();
            if a.visible && ours.is_some() && ours == Some(win::foreground_window()) {
                a.hide_window();
            } else {
                a.show_window();
            }
        }
        Hotkey(_) => {}
        TrayActivate => {
            let mut a = app.borrow_mut();
            if a.visible {
                a.hide_window();
            } else {
                a.show_window();
            }
        }
        TrayMenu { x, y } => tray_menu(app, x, y),
        ShowRequested => app.borrow_mut().show_window(),
        ForegroundChanged => app.borrow_mut().reenforce(),
        SessionEnding => {
            let mut a = app.borrow_mut();
            a.pins.set_session_ending(true);
            a.save_now();
        }
        SessionEndCancelled => app.borrow_mut().pins.set_session_ending(false),
        SessionEnded => {
            let mut a = app.borrow_mut();
            logfile::info!("Session ending: restored {} window(s), keeping pins for next login", a.pins.pinned().len());
            a.pins.release_all();
            a.save_now();
            host::remove_tray();
        }
    }
}

fn tray_menu(app: &Shared, x: i32, y: i32) {
    // Build the menu, then let go of the app: the menu runs a modal loop.
    let (items, pinned): (Vec<host::MenuItem>, Vec<Hwnd>) = {
        let a = app.borrow();
        let mut items = vec![
            host::MenuItem::Command { id: MENU_SHOW, text: "Show PinIt".into(), checked: false, enabled: true },
            host::MenuItem::Separator,
        ];
        let pinned: Vec<Hwnd> = a.pins.pinned().iter().map(|p| p.hwnd).collect();
        for (i, p) in a.pins.pinned().iter().enumerate() {
            let mut title: String = p.title.chars().take(48).collect();
            if p.title.chars().count() > 48 {
                title.push('…');
            }
            items.push(host::MenuItem::Command { id: MENU_PINNED + i as u32, text: title, checked: true, enabled: true });
        }
        if !pinned.is_empty() {
            items.push(host::MenuItem::Separator);
        }
        items.push(host::MenuItem::Command { id: MENU_PICK, text: "Pin a window…".into(), checked: false, enabled: true });
        items.push(host::MenuItem::Command {
            id: MENU_UNPIN_ALL,
            text: "Unpin all".into(),
            checked: false,
            enabled: a.pins.is_active(),
        });
        items.push(host::MenuItem::Separator);
        items.push(host::MenuItem::Command { id: MENU_QUIT, text: "Quit PinIt".into(), checked: false, enabled: true });
        (items, pinned)
    };
    let (x, y) = if x == 0 && y == 0 { host::cursor_pos() } else { (x, y) };
    let Some(choice) = host::popup_menu(&items, x, y, MENU_SHOW) else { return };

    let mut a = app.borrow_mut();
    match choice {
        MENU_SHOW => a.show_window(),
        MENU_PICK => {
            a.show_window();
            a.open_picker();
        }
        MENU_UNPIN_ALL => a.unpin_all(),
        MENU_QUIT => {
            logfile::info!("Restored and cleared {} pinned window(s) on manual quit", a.pins.pinned().len());
            a.pins.release_all();
            a.save_now();
            host::remove_tray();
            flash::hide();
            let _ = slint::quit_event_loop();
        }
        id if id >= MENU_PINNED => {
            if let Some(&hwnd) = pinned.get((id - MENU_PINNED) as usize) {
                a.toggle(hwnd, true);
            }
        }
        _ => {}
    }
}

/// Make a shortcut set live. Returns the names of the actions Windows
/// refused (another app owns the keys) or that aren't valid.
fn apply_shortcuts(s: &store::Shortcuts) -> Vec<String> {
    let mut failed = Vec::new();
    for (id, keys, name) in [
        (HOTKEY_PIN, &s.toggle_pin, "Pin / unpin"),
        (HOTKEY_OPACITY_UP, &s.opacity_up, "Increase opacity"),
        (HOTKEY_OPACITY_DOWN, &s.opacity_down, "Decrease opacity"),
        (HOTKEY_WINDOW, &s.toggle_window, "Show / hide PinIt"),
    ] {
        let ok = match shortcuts::parse(keys) {
            Some((mods, vk)) if shortcuts::has_safe_modifier(mods) => host::register_hotkey(id, mods, vk),
            _ => {
                host::unregister_hotkey(id);
                false
            }
        };
        if !ok {
            failed.push(name.to_string());
        }
    }
    failed
}

fn to_image(rgba: Vec<u8>, size: u32) -> Image {
    let buffer = SharedPixelBuffer::<Rgba8Pixel>::clone_from_slice(&rgba, size, size);
    Image::from_rgba8_premultiplied(buffer)
}

fn initial_and_tint(process: &str) -> (SharedString, slint::Color) {
    const TINTS: [u32; 6] = [0xd8c6f0, 0xffe08a, 0xc3e4cf, 0xf6cab8, 0xbedcf2, 0xf2cfe2];
    let initial = process.chars().find(|c| c.is_alphanumeric()).map(|c| c.to_uppercase().to_string()).unwrap_or("?".into());
    let hash = process.to_lowercase().bytes().fold(0u32, |h, b| h.wrapping_mul(31).wrapping_add(b as u32));
    (initial.into(), slint::Color::from_rgb_u8((TINTS[hash as usize % 6] >> 16) as u8, (TINTS[hash as usize % 6] >> 8) as u8, TINTS[hash as usize % 6] as u8))
}

impl App {
    fn ui(&self) -> Option<AppWindow> {
        self.ui.upgrade()
    }

    fn main_hwnd(&self) -> Option<Hwnd> {
        let ui = self.ui()?;
        let handle = ui.window().window_handle();
        match handle.window_handle().ok()?.as_raw() {
            RawWindowHandle::Win32(h) => Some(h.hwnd.get()),
            _ => None,
        }
    }

    fn style_title_bar(&self) {
        let (Some(ui), Some(hwnd)) = (self.ui(), self.main_hwnd()) else { return };
        let c = ui.get_window_color();
        let rgb = ((c.red() as u32) << 16) | ((c.green() as u32) << 8) | c.blue() as u32;
        win::style_title_bar(hwnd, ui.get_dark(), rgb);
    }

    fn show_window(&mut self) {
        let Some(ui) = self.ui() else { return };
        let _ = ui.show();
        self.visible = true;
        self.style_title_bar();
        if let Some(hwnd) = self.main_hwnd() {
            win::force_foreground(hwnd);
        }
    }

    fn hide_window(&mut self) {
        flash::hide();
        if let Some(ui) = self.ui() {
            let _ = ui.hide();
        }
        self.visible = false;
    }

    /// A short message: in the window while the user is looking at it, as a
    /// notification otherwise. `important` messages (errors) always show;
    /// the rest only when "Show a notification" is on.
    fn message(&mut self, text: &str, important: bool) {
        let looking = self.visible && self.main_hwnd() == Some(win::foreground_window());
        if looking {
            self.toast(text);
        } else if important || self.state.settings.show_notifications {
            host::notify("PinIt", text);
        }
    }

    fn toast(&self, text: &str) {
        if let Some(ui) = self.ui() {
            ui.set_toast_visible(false);
            ui.set_toast_text(text.into());
            ui.set_toast_visible(true);
        }
    }

    /// Pin or unpin `hwnd`. `from_hotkey`: the user is looking at that window,
    /// not at PinIt.
    fn toggle(&mut self, hwnd: Hwnd, from_hotkey: bool) {
        let title = win::window_title(hwnd);
        match self.pins.toggle(hwnd) {
            Ok(pinned) => {
                flash::show(hwnd, if pinned { flash::Kind::Pinned } else { flash::Kind::Unpinned });
                if pinned && self.state.settings.enable_sound {
                    win::play_pin_sound();
                }
                let verb = if pinned { "Pinned" } else { "Unpinned" };
                let text = if title.is_empty() { verb.to_string() } else { format!("{verb}: {title}") };
                if from_hotkey {
                    if self.state.settings.show_notifications {
                        host::notify("PinIt", &text);
                    }
                } else {
                    self.toast(&text);
                }
                self.save_now();
                self.pins_changed();
            }
            Err(e) => self.message(&e.message(), true),
        }
    }

    fn unpin_all(&mut self) {
        flash::hide();
        let n = self.pins.unpin_all();
        self.save_now();
        self.pins_changed();
        self.toast(&match n {
            0 => "Nothing to unpin.".to_string(),
            1 => "Unpinned 1 window.".to_string(),
            n => format!("Unpinned {n} windows."),
        });
    }

    fn reenforce(&mut self) {
        let r = self.pins.reenforce();
        for &hwnd in &r.restored {
            flash::show(hwnd, flash::Kind::Pinned);
        }
        if r.list_changed {
            self.save_now();
            self.pins_changed();
        } else if r.titles_changed {
            self.save_soon();
            self.update_rows_in_place();
        }
    }

    fn save_now(&mut self) {
        self.save_timer.stop();
        self.state.pins = self.pins.saved();
        store::save(&self.state);
    }

    /// Opacity changes arrive in bursts while a slider is dragged: write the
    /// file once things settle.
    fn save_soon(&mut self) {
        let state_pins = self.pins.saved();
        let mut state = self.state.clone();
        state.pins = state_pins;
        self.save_timer.start(slint::TimerMode::SingleShot, Duration::from_millis(600), move || store::save(&state));
    }

    /// The pin list changed: rebuild the rows, the timers and the tray.
    fn pins_changed(&mut self) {
        let mut keys: Vec<RowKey> = self.pins.pinned().iter().map(|p| RowKey::Live(p.hwnd)).collect();
        keys.extend((0..self.pins.pending().len()).map(RowKey::Waiting));
        let live: Vec<Hwnd> = self.pins.pinned().iter().map(|p| p.hwnd).collect();
        self.icons.retain(|h, _| live.contains(h));
        for &h in &live {
            self.icons.entry(h).or_insert_with(|| win::window_icon_rgba(h, ICON_PX).map(|px| to_image(px, ICON_PX)));
        }

        let rows: Vec<PinRow> = keys.iter().map(|k| self.row_for(*k)).collect();
        if keys == self.rows {
            for (i, row) in rows.into_iter().enumerate() {
                if self.row_model.row_data(i).as_ref() != Some(&row) {
                    self.row_model.set_row_data(i, row);
                }
            }
        } else {
            self.row_model.set_vec(rows);
            self.rows = keys;
        }

        let active = self.pins.is_active();
        host::watch_foreground(active);
        if active && !self.reenforce_timer.running() {
            let me = self.me.clone();
            self.reenforce_timer.start(slint::TimerMode::Repeated, Duration::from_secs(2), move || {
                if let Some(app) = me.upgrade() {
                    app.borrow_mut().reenforce();
                }
            });
        } else if !active {
            self.reenforce_timer.stop();
        }
        self.refresh_status();
    }

    fn update_rows_in_place(&mut self) {
        for (i, key) in self.rows.clone().into_iter().enumerate() {
            let row = self.row_for(key);
            if self.row_model.row_data(i).as_ref() != Some(&row) {
                self.row_model.set_row_data(i, row);
            }
        }
    }

    fn row_for(&self, key: RowKey) -> PinRow {
        match key {
            RowKey::Live(hwnd) => {
                let p = self.pins.pinned().iter().find(|p| p.hwnd == hwnd).expect("row for a pinned window");
                let (initial, tint) = initial_and_tint(&p.process);
                let icon = self.icons.get(&hwnd).cloned().flatten();
                PinRow {
                    title: p.title.as_str().into(),
                    process: p.process.as_str().into(),
                    opacity: p.opacity,
                    click_through: p.click_through,
                    has_icon: icon.is_some(),
                    icon: icon.unwrap_or_default(),
                    initial,
                    tint,
                    waiting: false,
                }
            }
            RowKey::Waiting(i) => {
                let s = &self.pins.pending()[i];
                let (initial, tint) = initial_and_tint(&s.process_name);
                PinRow {
                    title: s.title.as_str().into(),
                    process: s.process_name.as_str().into(),
                    opacity: win::alpha_to_percent(s.opacity),
                    click_through: s.click_through,
                    icon: Image::default(),
                    has_icon: false,
                    initial,
                    tint,
                    waiting: true,
                }
            }
        }
    }

    /// Hotkey warning, live count, tray tooltip.
    fn refresh_status(&self) {
        let warning = if self.hotkey_problems.is_empty() {
            String::new()
        } else {
            format!(
                "Shortcut unavailable: {}. Another app is using the keys. Pick different ones in Shortcuts.",
                self.hotkey_problems.join(", ")
            )
        };
        let n = self.pins.pinned().len();
        let mut tip = match n {
            0 => "PinIt".to_string(),
            1 => "PinIt: 1 window pinned".to_string(),
            n => format!("PinIt: {n} windows pinned"),
        };
        if !self.hotkey_problems.is_empty() {
            tip.push_str(" (a shortcut is unavailable)");
        }
        host::set_tray_tip(&tip);
        if let Some(ui) = self.ui() {
            ui.set_hotkey_warning(warning.into());
        }
    }

    /// The pin shortcut, shown under the list.
    fn refresh_shortcut_views(&self) {
        let Some(ui) = self.ui() else { return };
        let keys = shortcuts::display_tokens(&self.state.settings.shortcuts.toggle_pin);
        ui.set_pin_keys(ModelRc::new(VecModel::from(keys.into_iter().map(SharedString::from).collect::<Vec<_>>())));
    }

    // ---- Window picker ------------------------------------------------------

    fn open_picker(&mut self) {
        let Some(ui) = self.ui() else { return };
        self.picker = win::enumerate_windows()
            .into_iter()
            .map(|w| {
                let icon = win::window_icon_rgba(w.hwnd, ICON_PX).map(|px| to_image(px, ICON_PX));
                (w, icon)
            })
            .collect();
        ui.set_pick_filter("".into());
        self.filter_picker();
        ui.set_picker_open(true);
        ui.invoke_focus_search();
    }

    fn filter_picker(&mut self) {
        let Some(ui) = self.ui() else { return };
        let filter = ui.get_pick_filter().to_lowercase();
        self.picker_shown = (0..self.picker.len())
            .filter(|&i| {
                let w = &self.picker[i].0;
                filter.is_empty() || w.title.to_lowercase().contains(&filter) || w.process.to_lowercase().contains(&filter)
            })
            .collect();
        let items: Vec<PickItem> = self
            .picker_shown
            .iter()
            .map(|&i| {
                let (w, icon) = &self.picker[i];
                let (initial, tint) = initial_and_tint(&w.process);
                PickItem {
                    title: w.title.as_str().into(),
                    process: w.process.as_str().into(),
                    has_icon: icon.is_some(),
                    icon: icon.clone().unwrap_or_default(),
                    initial,
                    tint,
                    pinned: self.pins.is_pinned(w.hwnd),
                }
            })
            .collect();
        self.pick_model.set_vec(items);
    }

    // ---- Shortcut editor ----------------------------------------------------

    fn open_shortcut_editor(&mut self) {
        let current = self.state.settings.shortcuts.clone();
        self.fill_shortcut_editor(&current);
        if let Some(ui) = self.ui() {
            ui.set_shortcut_error("".into());
            ui.set_shortcuts_open(true);
        }
    }

    fn fill_shortcut_editor(&mut self, s: &store::Shortcuts) {
        let labels = shortcuts::key_labels();
        let edit = |label: &str, keys: &str| {
            let p = shortcuts::split(keys);
            ShortcutEdit {
                label: label.into(),
                win: p.win,
                ctrl: p.ctrl,
                alt: p.alt,
                shift: p.shift,
                key_index: labels.iter().position(|l| l.eq_ignore_ascii_case(&p.key)).map_or(0, |i| i as i32),
            }
        };
        self.edit_model.set_vec(vec![
            edit("Pin / unpin window", &s.toggle_pin),
            edit("Increase opacity", &s.opacity_up),
            edit("Decrease opacity", &s.opacity_down),
            edit("Show / hide PinIt", &s.toggle_window),
        ]);
        if let Some(ui) = self.ui() {
            ui.set_shortcut_error("".into());
        }
    }

    /// Nothing is accepted until Windows has registered the whole new set.
    fn save_shortcut_editor(&mut self) {
        let Some(ui) = self.ui() else { return };
        let labels = shortcuts::key_labels();
        let strings: Vec<String> = self
            .edit_model
            .iter()
            .map(|e| {
                shortcuts::build(&shortcuts::Parts {
                    win: e.win,
                    ctrl: e.ctrl,
                    alt: e.alt,
                    shift: e.shift,
                    key: labels.get(e.key_index as usize).cloned().unwrap_or_default(),
                })
            })
            .collect();
        let names = ["Pin / unpin window", "Increase opacity", "Decrease opacity", "Show / hide PinIt"];
        for (s, name) in strings.iter().zip(names) {
            match shortcuts::parse(s) {
                Some((mods, _)) if shortcuts::has_safe_modifier(mods) => {}
                _ => {
                    ui.set_shortcut_error(format!("“{name}” needs Win, Ctrl or Alt.").into());
                    return;
                }
            }
        }
        if strings.iter().enumerate().any(|(i, s)| strings[..i].contains(s)) {
            ui.set_shortcut_error("Two actions can't share the same keys.".into());
            return;
        }

        let mut candidate = self.state.settings.shortcuts.clone();
        candidate.toggle_pin = strings[0].clone();
        candidate.opacity_up = strings[1].clone();
        candidate.opacity_down = strings[2].clone();
        candidate.toggle_window = strings[3].clone();

        let failed = apply_shortcuts(&candidate);
        if !failed.is_empty() {
            // Put the working set back and say which one Windows refused.
            let old = self.state.settings.shortcuts.clone();
            self.hotkey_problems = apply_shortcuts(&old);
            ui.set_shortcut_error(
                format!("Another app already uses the keys for: {}. Pick different ones.", failed.join(", ")).into(),
            );
            return;
        }
        self.state.settings.shortcuts = candidate;
        self.hotkey_problems.clear();
        self.save_now();
        self.refresh_shortcut_views();
        self.refresh_status();
        ui.set_shortcuts_open(false);
        self.toast("Shortcuts saved.");
    }
}
