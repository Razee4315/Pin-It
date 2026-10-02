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
| F-11 | 2 | P0 | Restart fix is unreleased | blocked | Prepared: version 2.2.0, changelog, README and site metadata; the P1 restore/list fixes it depended on are in. Publishing means pushing a v2.2.0 tag, which creates a public release — needs your go-ahead after you merge |
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
| F-27 | 3 | P2 | Autostart checkbox ignores the registry | verified | Unit test autostartFollowsTheRegistry (scratch key) + harness: checkbox follows the Run key, an entry whose exe exists is untouched, a stale path is repaired; real Run key confirmed unchanged |
| F-28 | 3 | P3 | Empty-state hint chips go stale | verified | Harness: after rebinding, the empty-state hint shows the new pin shortcut (screenshot checked) |
| F-29 | 3 | P3 | Opacity cheat-sheet row assumes shared modifiers | verified | Harness: opacity shortcuts with different modifiers render as Increase/Decrease rows; same modifiers keep the combined row; long shortcuts elide the description |
| F-30 | 3 | P3 | Unsupported keys shown as "A" | verified | Unit tests (named/function keys, every offered key round-trips, unknown key preserved) + harness: F13–F17 register and display; NumpadAdd is shown as written, reported unavailable, not rewritten |
| F-31 | 3 | P3 | Shortcuts dialog accessibility | verified | Harness: all 16 checkboxes and 4 key lists carry '<action>: <part>' accessible names; errors are an inline label with an accessibility alert, no message boxes |
| F-32 | 3 | P3 | Quit forgets pins with no hint | verified | Harness: tray Quit reads 'Quit' / 'Quit and unpin 1 window' / 'Quit and unpin 2 windows' |
| F-33 | 3 | P3 | Uninstaller leaves Run key; no AppMutex | blocked | Implemented (AppMutex + unconditional uninsdeletevalue; app holds the mutex — harness-checked). Inno Setup is not installed here, so the script could not be compiled or an uninstall run; CI compiles it on push. Needs one manual install/uninstall |
| F-34 | 3 | P3 | Second launch only flashes the taskbar | verified | Harness: after a second launch the PinIt window was the foreground window (AllowSetForegroundWindow from the second process) |
| F-35 | 3 | P3 | "window(s)" tooltip | verified | Harness: tooltip reads 'no windows pinned' / '1 window pinned' / '2 windows pinned' |
| F-36 | 3 | P3 | About only in tray; CONTRIBUTING wrong | verified | Harness: About button in the window footer opens the About box; CONTRIBUTING and the bug template now point at it |
| F-37 | 4 | P2 | Window titles written to the log | verified | Harness: pinit.log has 'Pinned a window of dummywin.exe' and not the window title |
| F-38 | 4 | P2 | Site invisible without JavaScript | verified | Browser: with the js class removed every .reveal computes to opacity 1; class is now set in head before first paint |
| F-39 | 4 | P2 | Site does not mention SmartScreen | verified | Browser: SmartScreen note renders under the hero CTA and in the download band; portable link returns 302 to the asset (direct installer link deferred, see report) |
| F-40 | 4 | P3 | Autoplay video without controls | verified | Browser: video has controls and no autoplay attribute; script starts it only when reduced motion is not requested |
| F-41 | 4 | P3 | Failed save is ignored | verified | Harness: with pinned.json made unwritable, the log gets 'Could not save …pinned.json: <reason>'; corrupt-file backup still works |
| F-42 | 4 | P3 | CI permissions / pinning / checksums | blocked | Implemented: read-only token by default, write only in a tag-gated release job; actions pinned to commit SHAs; SHA256SUMS.txt and a stable-named installer published. YAML parses, but the workflow can only be proven by a CI run, which needs the branch pushed. clang-format check deliberately not added (see report) |
| F-43 | 4 | P3 | Docs drift (size, version, restore promise) | verified | Read back: llms.txt size and restart wording, site 'It remembers' copy and README FAQ now describe the waiting-pin behaviour; softwareVersion is bumped with F-11 |

## Missing must-haves

| ID | Summary | Status | How verified / why blocked |
|----|---------|--------|----------------------------|
| M-01 | Visible "this is pinned" indicator | verified | Harness: outline on pin/unpin/restore (F-02, F-12); resting the pointer on a row outlines its window after 350 ms, a passing pointer does not; tray menu lists pinned windows (M-05) |
| M-02 | Pending restore for apps opened later (same work as F-12) | verified | Harness: 4 saved pins, 1 open at start → 1 pinned at saved opacity, 3 waiting rows; file keeps all 4 after an unrelated pin; awaited window pinned and outlined within 0.8 s of opening; late-titled window pinned once the title matches; another window of the same app ignored; forget/quit/session-end all checked |
| M-03 | Code-signed installer | todo | |
| M-04 | "Unpin all" | verified | Harness: Unpin all (window button and tray item) releases every window incl. opacity, clears waiting pins and the saved file, then disables itself |
| M-05 | Tray menu lists pinned windows + "Pin a window…" | verified | Harness: tray menu lists each pinned window (ampersands escaped); choosing one unpins only that window; 'Pin a window…' present |
| M-06 | Notifications on/off | verified | Harness: setting defaults to off, checkbox unchecked, ticking it writes show_notifications=true |
| M-07 | Reset shortcuts to defaults | verified | Harness: Restore Defaults fills in the default set and clears the error; nothing applied until OK |
| M-08 | Click-through for pinned windows | verified | Harness: toggle sets WS_EX_TRANSPARENT and WindowFromPoint no longer hits the window; off/unpin restore a normal clickable unlayered window; saved; undone correctly after a simulated crash |
| M-09 | "Check for updates" link | verified | Harness: tray has 'Check for updates…'; About box links to /releases/latest. Opening the browser itself was not triggered |
| M-10 | Dark theme | verified | Harness: every screen rendered in both variants and reviewed (main, picker, shortcuts, About, tray menu); rendered contrast >= 4.5 for all text pairs in both; a colour-scheme change at run time re-themes without restart. Real Windows setting toggle not exercised |
| M-11 | winget / Scoop package | todo | |

## Cleanup

| ID | Summary | Status | How verified / why blocked |
|----|---------|--------|----------------------------|
| C-01 | Remove `MainWindow::setShortcutConfig` | verified | No references left (grep); build and tests pass |
| C-02 | Remove `m_emptyLabel`, `m_shortcutsLabel` | verified | No references left (grep); build and tests pass |
| C-03 | Remove `winpin::opacityPercent` | verified | No references left (grep); build and tests pass |
| C-04 | Remove `PinManager::pinnedCount` | verified | No longer dead: pinnedCount() is now used by the tray tooltip and menu (F-32/F-35), so it stays |
| C-05 | Unused / missing includes | verified | Build with -Wall -Wextra clean; tests pass |
| C-06 | Unused link libraries and `CMAKE_AUTOUIC` | verified | Clean reconfigure, link and tests pass; objdump shows no ADVAPI32 import |
| C-07 | Unreachable `icon-128.png` fallback | verified | PinIt.exe 753,576 → 610,572 bytes; icon renders in header, tray, About (screenshots) |
| C-08 | `resources/logo.svg` unreferenced | todo | |
| C-09 | Stale remote branches | todo | |
| C-10 | Stale / wrong comments | verified | Build passes; installer and CMake comments corrected in their own commits (F-33, O-01) |
| C-11 | One window-handle type, one cast helper | verified | Build clean; no void* handles or casts outside winpin.cpp/windowpicker.cpp; tier1, restore and click scenarios pass |
| C-12 | Shared app-data directory helper | verified | Unit test persistenceRoundTrips checks dataDir(); log and settings land in the same scratch folder in the harness and real-exe run |
| C-13 | Chip-row builder duplicated three times | verified | Build + harness screenshots: cheat-sheet and empty-state chips render from the shared helper |
| C-14 | Hotkey-result messages duplicated | verified | main.cpp has one applyShortcuts path for startup and edits; harness shortcuts scenario |
| C-15 | Un-pin sequence duplicated | verified | Build + harness: unpin and quit paths both restore windows (F-05/F-09 scenarios) |
| C-16 | `"Unknown"` sentinel strings | verified | Build + harness enumerate/list scenarios; no "Unknown" literal left in src |
| C-17 | Inline row colours outside the stylesheet | verified | Rendered in both themes: pin title still semi-bold, About text readable; only the per-app badge colour is set inline (it is data, not theme) |
| C-18 | Tests for persistence and restore matching | verified | 3 new unit tests pass (19 total) |

## Optimisation

| ID | Summary | Status | How verified / why blocked |
|----|---------|--------|----------------------------|
| O-01 | Drop Qt Network (mutex + window message for single instance) | verified | Harness: second process finds the primary, one show request, hidden and minimised windows are shown and take the foreground; PinIt.exe no longer imports Qt6Network.dll (objdump) |
| O-02 | Trim unneeded Qt plugins from the bundle | verified | Deployed locally with the old and new flags: 38.68 MB / 20 files → 34.35 MB / 8 files. From the trimmed folder (no Qt on PATH) the UI renders pixel-identical, picker icons load, and the real PinIt.exe starts, stays up and hands over a second launch. CI flag change itself runs on the next CI build |
| O-03 | Embed the small icon instead of the 141 KB one | verified | PinIt.exe 753,576 → 610,572 bytes; icon renders in header, tray, About (screenshots) |
| O-04 | In-place list updates (same work as F-20) | verified | Same change and check as F-20 |
| O-05 | Cache settings in memory instead of re-reading the file | verified | Unit tests (file contents checked after dropCache) + harness restore/crash/save/tray/shortcuts/autostart scenarios |
| O-06 | Smaller README demo GIF | verified | Re-encoded from the MP4 at 720 px / 10 fps / 128 colours: 6,510,997 → 3,449,566 bytes (−47%); frame checked, README shows it at 720 px. Note: the recording itself still shows the old UI (see report) |
| O-07 | Compress `og-image.png` | verified | Palette-quantised (256 colours): 519,051 → 354,759 bytes (−32%), still 1200×630; image reviewed |
| O-08 | Drop the Google Fonts request on the site | verified | Browser: no font link left, zero external requests on load, body uses the system UI font; page reviewed |
