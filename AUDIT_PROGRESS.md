# Audit implementation progress

Branch: `audit/full-implementation`. Status values: todo / in-progress / done / verified / blocked.

An item is `verified` only once the build is warning-clean, the unit tests pass,
and the affected flow has been exercised (the "How verified" column says how).

## Findings

| ID | Tier | Sev | Summary | Status | How verified / why blocked |
|----|------|-----|---------|--------|----------------------------|
| F-01 | 1 | P1 | Hotkey pins shell windows / PinIt itself | verified |  |
| F-02 | 1 | P1 | Toast on every pin/unpin, no setting | verified | Harness: outline overlay appears on pin and unpin, matches the DWM frame to the pixel, is click-through, takes no focus, gone after ~0.6 s; toasts now gated by the setting |
| F-03 | 1 | P2 | Hotkey registration failure is only a toast | verified | Harness: with the user's own PinIt holding the default keys all four failed; warning label and tray tooltip name them and clear when fixed (screenshot checked) |
| F-04 | 1 | P2 | Tray double-click opens then hides | verified | Harness: tray Trigger shows the window, the following DoubleClick leaves it shown |
| F-05 | 1 | P2 | Unpin strips an app's own always-on-top | verified | Harness: a window already topmost stays topmost after pin+unpin; an ordinary one does not |
| F-06 | 1 | P2 | Titles never refreshed | verified | Harness: retitled a pinned foreign window; manager, row and pinned.json all carry the new title |
| F-07 | 1 | P3 | Buried window is hidden instead of raised | verified | Harness: toggling a visible unfocused window keeps it shown and focuses it; toggling the focused window hides it |
| F-08 | 1 | P3 | 2 s polling only | verified | Harness: topmost stripped right after a timer tick was restored within 0.6 s of another window coming to the front |
| F-09 | 1 | P3 | Session-ending flag sticks after cancelled shutdown | verified | Harness: WM_QUERYENDSESSION then WM_ENDSESSION(FALSE) → quit clears pins; without the cancel they are kept |
| F-10 | 1 | P3 | Messages vanish without a tray | verified | Harness: notify() with the window focused shows the in-window message; list geometry unchanged (screenshot checked) |
| F-11 | 2 | P0 | Restart fix is unreleased | todo | |
| F-12 | 2 | P1 | Restore is one-shot and erases unmatched pins | verified | Harness: 4 saved pins, 1 open at start → 1 pinned at saved opacity, 3 waiting rows; file keeps all 4 after an unrelated pin; awaited window pinned and outlined within 0.8 s of opening; late-titled window pinned once the title matches; another window of the same app ignored; forget/quit/session-end all checked |
| F-13 | 2 | P1 | Restore fallback pins the wrong window | verified |  |
| F-14 | 2 | P1 | Pinned row clips its unpin button | verified |  |
| F-15 | 2 | P2 | Pinned list is only 98 px tall | verified | Measured in harness: list area 254 px at the default size (was 98), 108 px at minimum height with the hotkey warning showing, grows with the window; screenshots checked |
| F-16 | 2 | P2 | Slider not synced with opacity hotkeys | verified | Harness: setOpacity(60) outside the UI moves the row slider and label to 60%; dragging the slider still sets opacity |
| F-17 | 2 | P2 | Opacity hotkey silent on unpinned window | verified | Harness: adjustForegroundOpacity on an unpinned window emits 'Pin this window first…' |
| F-18 | 2 | P2 | Opacity hotkeys do not repeat | verified | Harness with injected held keys: pin hotkey fired once, opacity hotkey fired 4 times |
| F-19 | 2 | P2 | List order is hash order | verified | Harness: 6 pins listed in pin order; order kept after an unpin |
| F-20 | 2 | P2 | List rebuilt from scratch on every change | verified | Harness: first row object identical before/after 4 more pins and an unpin; removed row disappears |
| F-21 | 2 | P2 | Picker lists shell / cloaked windows | verified | Harness on this desktop: 12 windows offered vs 16 under the old rule; no shell, cloaked, untitled or own windows; Program Manager gone |
| F-22 | 2 | P2 | Picker: no search, icons, empty state; dead-end OK | verified | Harness: icons shown (13/13), search filters case-insensitively and keeps a row selected, empty states for no match / no windows, Pin disabled when nothing is selectable, Up/Down/Enter from the search box work; screenshot checked |
| F-23 | 2 | P2 | Accessible names and contrast | verified | Harness: accessible names reach QAccessible and follow retitling; rendered contrast primary 4.98, separators 5.06, avatar 10.5, muted 4.93; unpin colour pairs 5.76 / 4.92; Tab order top to bottom; focus visibly changes primary, slider, checkbox without resizing; Space/arrow keys work |
| F-24 | 2 | P3 | Layered style left behind after crash re-pin | verified | Harness: instance destroyed without cleanup leaves the window topmost+layered; next instance restores it and unpin removes both |
| F-25 | 3 | P2 | Shift-only shortcuts accepted | verified | Unit test shortcutNeedsWinCtrlOrAlt + harness: Shift+A from config is refused at registration |
| F-26 | 3 | P2 | Shortcuts saved before registration is tested | verified | Harness: with Win+Ctrl+T held by the running PinIt, OK keeps the dialog open, names Pin/Unpin, restores the previous hotkeys and saves nothing; a free set is registered, saved and clears the standing warning |
| F-27 | 3 | P2 | Autostart checkbox ignores the registry | done |  |
| F-28 | 3 | P3 | Empty-state hint chips go stale | verified | Harness: after rebinding, the empty-state hint shows the new pin shortcut (screenshot checked) |
| F-29 | 3 | P3 | Opacity cheat-sheet row assumes shared modifiers | verified | Harness: opacity shortcuts with different modifiers render as Increase/Decrease rows; same modifiers keep the combined row; long shortcuts elide the description |
| F-30 | 3 | P3 | Unsupported keys shown as "A" | verified | Unit tests (named/function keys, every offered key round-trips, unknown key preserved) + harness: F13–F17 register and display; NumpadAdd is shown as written, reported unavailable, not rewritten |
| F-31 | 3 | P3 | Shortcuts dialog accessibility | verified | Harness: all 16 checkboxes and 4 key lists carry '<action>: <part>' accessible names; errors are an inline label with an accessibility alert, no message boxes |
| F-32 | 3 | P3 | Quit forgets pins with no hint | todo | |
| F-33 | 3 | P3 | Uninstaller leaves Run key; no AppMutex | todo | |
| F-34 | 3 | P3 | Second launch only flashes the taskbar | done |  |
| F-35 | 3 | P3 | "window(s)" tooltip | todo | |
| F-36 | 3 | P3 | About only in tray; CONTRIBUTING wrong | todo | |
| F-37 | 4 | P2 | Window titles written to the log | verified | Harness: pinit.log has 'Pinned a window of dummywin.exe' and not the window title |
| F-38 | 4 | P2 | Site invisible without JavaScript | verified | Browser: with the js class removed every .reveal computes to opacity 1; class is now set in head before first paint |
| F-39 | 4 | P2 | Site does not mention SmartScreen | verified | Browser: SmartScreen note renders under the hero CTA and in the download band; portable link returns 302 to the asset (direct installer link deferred, see report) |
| F-40 | 4 | P3 | Autoplay video without controls | todo | |
| F-41 | 4 | P3 | Failed save is ignored | todo | |
| F-42 | 4 | P3 | CI permissions / pinning / checksums | todo | |
| F-43 | 4 | P3 | Docs drift (size, version, restore promise) | todo | |

## Missing must-haves

| ID | Summary | Status | How verified / why blocked |
|----|---------|--------|----------------------------|
| M-01 | Visible "this is pinned" indicator | in-progress | Outline on pin/unpin done (F-02); locate-from-list and tray list still to come |
| M-02 | Pending restore for apps opened later (same work as F-12) | verified | Harness: 4 saved pins, 1 open at start → 1 pinned at saved opacity, 3 waiting rows; file keeps all 4 after an unrelated pin; awaited window pinned and outlined within 0.8 s of opening; late-titled window pinned once the title matches; another window of the same app ignored; forget/quit/session-end all checked |
| M-03 | Code-signed installer | todo | |
| M-04 | "Unpin all" | todo | |
| M-05 | Tray menu lists pinned windows + "Pin a window…" | todo | |
| M-06 | Notifications on/off | verified | Harness: setting defaults to off, checkbox unchecked, ticking it writes show_notifications=true |
| M-07 | Reset shortcuts to defaults | verified | Harness: Restore Defaults fills in the default set and clears the error; nothing applied until OK |
| M-08 | Click-through for pinned windows | todo | |
| M-09 | "Check for updates" link | todo | |
| M-10 | Dark theme | todo | |
| M-11 | winget / Scoop package | todo | |

## Cleanup

| ID | Summary | Status | How verified / why blocked |
|----|---------|--------|----------------------------|
| C-01 | Remove `MainWindow::setShortcutConfig` | todo | |
| C-02 | Remove `m_emptyLabel`, `m_shortcutsLabel` | todo | |
| C-03 | Remove `winpin::opacityPercent` | todo | |
| C-04 | Remove `PinManager::pinnedCount` | todo | |
| C-05 | Unused / missing includes | todo | |
| C-06 | Unused link libraries and `CMAKE_AUTOUIC` | todo | |
| C-07 | Unreachable `icon-128.png` fallback | todo | |
| C-08 | `resources/logo.svg` unreferenced | todo | |
| C-09 | Stale remote branches | todo | |
| C-10 | Stale / wrong comments | todo | |
| C-11 | One window-handle type, one cast helper | todo | |
| C-12 | Shared app-data directory helper | todo | |
| C-13 | Chip-row builder duplicated three times | verified | Build + harness screenshots: cheat-sheet and empty-state chips render from the shared helper |
| C-14 | Hotkey-result messages duplicated | verified | main.cpp has one applyShortcuts path for startup and edits; harness shortcuts scenario |
| C-15 | Un-pin sequence duplicated | verified | Build + harness: unpin and quit paths both restore windows (F-05/F-09 scenarios) |
| C-16 | `"Unknown"` sentinel strings | verified | Build + harness enumerate/list scenarios; no "Unknown" literal left in src |
| C-17 | Inline row colours outside the stylesheet | todo | |
| C-18 | Tests for persistence and restore matching | todo | |

## Optimisation

| ID | Summary | Status | How verified / why blocked |
|----|---------|--------|----------------------------|
| O-01 | Drop Qt Network (mutex + window message for single instance) | done |  |
| O-02 | Trim unneeded Qt plugins from the bundle | todo | |
| O-03 | Embed the small icon instead of the 141 KB one | todo | |
| O-04 | In-place list updates (same work as F-20) | verified | Same change and check as F-20 |
| O-05 | Cache settings in memory instead of re-reading the file | todo | |
| O-06 | Smaller README demo GIF | todo | |
| O-07 | Compress `og-image.png` | todo | |
| O-08 | Drop the Google Fonts request on the site | todo | |
