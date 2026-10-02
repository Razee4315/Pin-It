# Audit implementation progress

Branch: `audit/full-implementation`. Status values: todo / in-progress / done / verified / blocked.

An item is `verified` only once the build is warning-clean, the unit tests pass,
and the affected flow has been exercised (the "How verified" column says how).

## Findings

| ID | Tier | Sev | Summary | Status | How verified / why blocked |
|----|------|-----|---------|--------|----------------------------|
| F-01 | 1 | P1 | Hotkey pins shell windows / PinIt itself | verified |  |
| F-02 | 1 | P1 | Toast on every pin/unpin, no setting | todo | |
| F-03 | 1 | P2 | Hotkey registration failure is only a toast | done |  |
| F-04 | 1 | P2 | Tray double-click opens then hides | verified | Harness: tray Trigger shows the window, the following DoubleClick leaves it shown |
| F-05 | 1 | P2 | Unpin strips an app's own always-on-top | done |  |
| F-06 | 1 | P2 | Titles never refreshed | done |  |
| F-07 | 1 | P3 | Buried window is hidden instead of raised | done |  |
| F-08 | 1 | P3 | 2 s polling only | todo | |
| F-09 | 1 | P3 | Session-ending flag sticks after cancelled shutdown | done |  |
| F-10 | 1 | P3 | Messages vanish without a tray | done |  |
| F-11 | 2 | P0 | Restart fix is unreleased | todo | |
| F-12 | 2 | P1 | Restore is one-shot and erases unmatched pins | todo | |
| F-13 | 2 | P1 | Restore fallback pins the wrong window | verified |  |
| F-14 | 2 | P1 | Pinned row clips its unpin button | verified |  |
| F-15 | 2 | P2 | Pinned list is only 98 px tall | todo | |
| F-16 | 2 | P2 | Slider not synced with opacity hotkeys | verified | Harness: setOpacity(60) outside the UI moves the row slider and label to 60%; dragging the slider still sets opacity |
| F-17 | 2 | P2 | Opacity hotkey silent on unpinned window | verified | Harness: adjustForegroundOpacity on an unpinned window emits 'Pin this window first…' |
| F-18 | 2 | P2 | Opacity hotkeys do not repeat | verified | Harness with injected held keys: pin hotkey fired once, opacity hotkey fired 4 times |
| F-19 | 2 | P2 | List order is hash order | verified | Harness: 6 pins listed in pin order; order kept after an unpin |
| F-20 | 2 | P2 | List rebuilt from scratch on every change | verified | Harness: first row object identical before/after 4 more pins and an unpin; removed row disappears |
| F-21 | 2 | P2 | Picker lists shell / cloaked windows | todo | |
| F-22 | 2 | P2 | Picker: no search, icons, empty state; dead-end OK | todo | |
| F-23 | 2 | P2 | Accessible names and contrast | todo | |
| F-24 | 2 | P3 | Layered style left behind after crash re-pin | todo | |
| F-25 | 3 | P2 | Shift-only shortcuts accepted | verified | Unit test shortcutNeedsWinCtrlOrAlt + harness: Shift+A from config is refused at registration |
| F-26 | 3 | P2 | Shortcuts saved before registration is tested | todo | |
| F-27 | 3 | P2 | Autostart checkbox ignores the registry | todo | |
| F-28 | 3 | P3 | Empty-state hint chips go stale | todo | |
| F-29 | 3 | P3 | Opacity cheat-sheet row assumes shared modifiers | todo | |
| F-30 | 3 | P3 | Unsupported keys shown as "A" | todo | |
| F-31 | 3 | P3 | Shortcuts dialog accessibility | todo | |
| F-32 | 3 | P3 | Quit forgets pins with no hint | todo | |
| F-33 | 3 | P3 | Uninstaller leaves Run key; no AppMutex | todo | |
| F-34 | 3 | P3 | Second launch only flashes the taskbar | todo | |
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
| M-01 | Visible "this is pinned" indicator | todo | |
| M-02 | Pending restore for apps opened later (same work as F-12) | todo | |
| M-03 | Code-signed installer | todo | |
| M-04 | "Unpin all" | todo | |
| M-05 | Tray menu lists pinned windows + "Pin a window…" | todo | |
| M-06 | Notifications on/off | todo | |
| M-07 | Reset shortcuts to defaults | todo | |
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
| C-13 | Chip-row builder duplicated three times | todo | |
| C-14 | Hotkey-result messages duplicated | todo | |
| C-15 | Un-pin sequence duplicated | done |  |
| C-16 | `"Unknown"` sentinel strings | todo | |
| C-17 | Inline row colours outside the stylesheet | todo | |
| C-18 | Tests for persistence and restore matching | todo | |

## Optimisation

| ID | Summary | Status | How verified / why blocked |
|----|---------|--------|----------------------------|
| O-01 | Drop Qt Network (mutex + window message for single instance) | todo | |
| O-02 | Trim unneeded Qt plugins from the bundle | todo | |
| O-03 | Embed the small icon instead of the 141 KB one | todo | |
| O-04 | In-place list updates (same work as F-20) | verified | Same change and check as F-20 |
| O-05 | Cache settings in memory instead of re-reading the file | todo | |
| O-06 | Smaller README demo GIF | todo | |
| O-07 | Compress `og-image.png` | todo | |
| O-08 | Drop the Google Fonts request on the site | todo | |
