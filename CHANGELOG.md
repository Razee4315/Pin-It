# Changelog

All notable changes to PinIt are documented here.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [2.2.0]

### Upgrade notes
- Pins now survive a restart even when PinIt starts before your apps: saved
  pins whose window isn't open yet show as **Waiting** and are pinned again as
  soon as that window opens with the same title. (2.1.1 lost all pins on every
  exit, including shutdown.)
- Pin/unpin **notifications are off by default**; a brief outline around the
  window confirms the action instead. Turn notifications back on with
  "Show a notification when pinning".
- Every global shortcut must now include **Win, Ctrl or Alt**. A shortcut saved
  with only Shift is reported as unavailable — pick new keys in Edit shortcuts.
- PinIt follows the Windows light/dark setting and uses Qt's Fusion style, so
  check boxes and lists look slightly different.
- `pinned.json` gains new keys (`show_notifications`, and per pin
  `click_through`, `was_layered`, `was_topmost`, `was_click_through`). Older
  files load unchanged; no migration step.
- Building from source now needs **Qt 6.5 or newer**. Qt Network is no longer
  used. No new environment variables.

### Features
- On-screen outline around a window when it is pinned, unpinned, re-pinned
  after a restart, or when the pointer rests on its row in the list
  (F-02, M-01).
- Saved pins wait for their window after a restart instead of being dropped
  (F-12, M-02).
- Click-through: let mouse clicks pass through a pinned window (M-08).
- Dark theme that follows the Windows colour mode (M-10).
- Tray menu lists the pinned windows (click to unpin), plus "Pin a window…",
  "Unpin all" and "Check for updates…" (M-05, M-04, M-09).
- "Unpin all" and "About" buttons in the window (M-04, F-36).
- Window picker with search, window icons and empty states; it only lists real
  application windows (F-22, F-21).
- Shortcut editor: F1–F24, arrows, Space and punctuation keys; Restore
  Defaults; only accepts a set that Windows actually registered and says which
  shortcut another app owns (F-30, M-07, F-26).
- Standing warning in the window and tray tooltip while a shortcut is
  unavailable (F-03).
- Optional pin notifications, off by default (M-06).
- Window grows in height to show more pins (F-15).

### Fixes
- Pins survive a Windows restart again, including when the other apps close
  before PinIt; a cancelled shutdown no longer leaves PinIt thinking the
  session is ending (F-09).
- Restoring only re-pins a window whose app and title both match (F-13).
- The pin hotkey no longer pins the desktop, taskbar, Start or PinIt itself
  (F-01).
- A long window title no longer pushes the unpin button out of the list
  (F-14).
- Unpinning keeps an app's own always-on-top (F-05); a window re-pinned after
  a crash is fully restored on unpin (F-24).
- Tray: double-click no longer opens and hides the window; a click brings a
  buried window forward; Quit says how many windows it unpins (F-04, F-07,
  F-32).
- The list follows window title changes, keeps pin order, keeps the slider in
  sync with the opacity hotkeys and updates rows in place (F-06, F-19, F-16,
  F-20).
- Opacity hotkeys repeat while held, and explain themselves on an unpinned
  window (F-18, F-17).
- Shortcuts must include Win, Ctrl or Alt; a hand-edited key the editor
  doesn't know is kept, not replaced by "A" (F-25, F-30).
- "Start PinIt with Windows" reflects the real Run registry entry and repairs
  a moved portable copy (F-27).
- The cheat-sheet and empty-state hint stay correct after rebinding (F-28,
  F-29).
- Messages appear inside the window when it is in front or when there is no
  tray (F-10).
- A second launch brings the running PinIt to the front (F-34).
- The installer detects a running PinIt and always removes the autostart entry
  on uninstall (F-33).
- A failed save is written to the log; a corrupt `pinned.json` is backed up
  (F-41).
- Website: content visible without JavaScript, SmartScreen guidance, video
  controls and no autoplay under reduced motion, accurate restart wording and
  fresh screenshots (F-38, F-39, F-40, F-43).
- Tooltip reads "1 window pinned" (F-35).

### Performance
- Smaller download: Qt Network, Qt Svg and unused Qt plugins are no longer
  bundled (about 4 MB less unpacked) (O-01, O-02).
- Smaller executable icon resource (O-03); settings file read once instead of
  before every save (O-05).
- Pins are re-asserted the moment another window comes to the front (F-08).
- Website: no Google Fonts request; README GIF and social image roughly halved
  and a third smaller (O-08, O-06, O-07).

### Accessibility
- Text contrast at least 4.5:1 in both themes, visible keyboard focus, a
  top-to-bottom tab order, and screen-reader names for every row control and
  shortcut setting (F-23, F-31).
- In-window messages and dialog errors are announced to screen readers (F-10,
  F-31).

### Security and privacy
- Window titles are no longer written to `pinit.log` (F-37).
- CI: read-only token by default, write access only in the tag-gated release
  job, third-party actions pinned to commit SHAs; releases include
  `SHA256SUMS.txt` and a stable-named `PinIt-setup-x64.exe` (F-42).

### Cleanup
- Dead code and unused includes/libraries removed; one window-handle type;
  shared helpers for the data folder, shortcut chips and the unpin path; no
  sentinel strings; colours only in the theme (C-01 – C-07, C-10 – C-17).
- New unit tests for saved-pin matching, shell windows, shortcut keys,
  autostart and `pinned.json` (19 tests, up from 7) (C-18).
- Earlier, unreleased fixes from PRs #14–#16: compact one-line pinned rows, a
  soft tick sound, debounced opacity saves, corrupt-file backup, quitting when
  no tray is available, silent restore at startup.

Full audit tracker: [AUDIT_PROGRESS.md](AUDIT_PROGRESS.md).

## [2.1.1]

### Changed
- Quitting PinIt now **forgets all pins** — on exit, every pinned window is
  un-topmosted and reset to full opacity, and the saved pin list is cleared so
  nothing is re-pinned on the next launch. (Closing to the tray still keeps
  pins live.) This reverses the "re-pin on next launch" behaviour from 2.1.0.

## [2.1.0]

### Added
- **Edit Shortcuts dialog** — rebind all global hotkeys from the UI (modifier
  checkboxes + key), with conflict and duplicate validation. No more hand-editing
  `pinned.json`.

### Fixed
- Pinned windows are now **restored on exit** — quitting PinIt no longer leaves
  other apps' windows stuck always-on-top or translucent. (Saved pins are kept,
  so they're re-pinned on the next launch.)

## [2.0.0]

Complete rewrite of PinIt as a native **C++ / Qt 6** application (the previous
Rust + Tauri implementation is archived on the `legacy-tauri` branch).

### Added
- Native C++/Qt 6 Widgets app talking directly to the Win32 API.
- Global-hotkey pinning, per-window opacity, system tray, pin persistence, and
  Windows 11 topmost re-enforcement (feature parity with the Tauri version).
- About dialog (version, author, links) from the tray menu.
- Executable file metadata (version/company/description) via `VERSIONINFO`.
- Pin confirmation sound (system beep), toggleable in settings.
- File logging to `%LOCALAPPDATA%\PinIt\pinit.log` for diagnostics.
- Unit tests (opacity conversion, shortcut parsing) run in CI.
- CMake build, `windeployqt` bundling, Inno Setup installer, and a GitHub
  Actions pipeline that builds the installer + portable ZIP on every push and
  publishes a Release on version tags.

### Changed
- Autostart now launches hidden in the tray (`--minimized`) instead of opening
  the window on every login.
- A second launch now focuses the running instance instead of exiting silently.
- The re-enforce timer only runs while windows are pinned (zero idle CPU).
- Version is single-sourced from CMake into the app, the installer, and the exe
  metadata.

[2.2.0]: https://github.com/Razee4315/Pin-It/releases/tag/v2.2.0
[2.1.1]: https://github.com/Razee4315/Pin-It/releases/tag/v2.1.1
[2.1.0]: https://github.com/Razee4315/Pin-It/releases/tag/v2.1.0
[2.0.0]: https://github.com/Razee4315/Pin-It/releases/tag/v2.0.0
