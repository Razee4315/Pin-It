# Changelog

All notable changes to PinIt are documented here.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [2.2.0]

### Fixed
- **Pins survive a Windows restart again.** 2.1.1 cleared the saved pins on
  every exit, which also wiped them on shutdown/restart — disabling the
  advertised "pins come back after a restart" feature. PinIt now keeps pins
  when Windows is logging off/restarting and only forgets them on a deliberate
  quit (and a shutdown that gets cancelled no longer confuses the two).
- **Saved pins wait for their window.** After a restart PinIt usually starts
  before the apps it had pinned. Instead of giving up — and then erasing those
  pins — it now lists them as "Waiting" and pins each window as soon as it is
  opened again. A waiting pin can be dropped from the list.
- Restoring no longer pins "some window of the same app": a saved pin is only
  applied to a window with the same app *and* title.
- The pin hotkey no longer pins the desktop, the taskbar, Start or PinIt
  itself, and the window picker no longer lists them (or hidden Store-app
  windows).
- A long window title no longer pushes the unpin button out of the list.
- Unpinning no longer takes away an app's own always-on-top (e.g. Task
  Manager's) or resets the transparency of apps that manage their own.
- Double-clicking the tray icon no longer opens the window and hides it again;
  clicking it while the window is buried brings the window forward.
- The "Start PinIt with Windows" checkbox now reflects what Windows will
  actually do (the Run registry entry), including when the installer set it.
- Window titles in the list follow the window as it is retitled.
- After a crash, re-pinning and then unpinning a window now fully restores it.
- Closing the window when no system tray is available now quits PinIt instead
  of leaving it running invisibly with no way to exit.
- A corrupt `pinned.json` is backed up to `pinned.json.corrupt` instead of
  being silently overwritten; a failed save is written to the log.
- Window titles are no longer written to `pinit.log`.

### Added
- **On-screen feedback**: a brief outline around the window when it is pinned
  or unpinned, and when the pointer rests on its row in the list.
- **Click-through**: let mouse clicks pass through a pinned window (per
  window, next to the opacity slider).
- **Dark theme** that follows the Windows colour mode.
- **Tray menu** lists the pinned windows (click to unpin), plus "Pin a
  window…", "Unpin all" and "Check for updates…".
- "Unpin all" and "About" in the window.
- The window picker has a search box, window icons and proper empty states.
- Shortcuts can use F1–F24, arrows, Space and punctuation keys; the editor has
  "Restore Defaults" and only accepts a set Windows has actually registered,
  saying which shortcut another app already owns.
- A standing warning in the window and tray tooltip while a shortcut is
  unavailable.
- Setting: "Show a notification when pinning" (off by default — the outline
  replaces the notification).

### Changed
- Each pinned window is now a single compact row, kept in the order you pinned
  them; the window is resizable in height and gives the extra room to the list.
- Holding an opacity hotkey now keeps fading; pressing one on an unpinned
  window says why nothing happened.
- Global shortcuts must include Win, Ctrl or Alt (Shift alone would capture
  ordinary typing).
- Messages appear inside the window while you are looking at it, as a tray
  notification otherwise.
- The tray "Quit" item says how many windows it will unpin.
- The pin confirmation sound is now a soft "tick" instead of the system beep.
- Better contrast, visible keyboard focus, sensible tab order and screen-reader
  names throughout.
- A second launch now brings the running PinIt to the front.
- Smaller download: Qt Network and unused Qt plugins are no longer bundled.
- The installer detects a running PinIt and always removes the autostart entry
  on uninstall. Release assets come with `SHA256SUMS.txt`.
- Building from source now needs Qt 6.5 or newer.

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
