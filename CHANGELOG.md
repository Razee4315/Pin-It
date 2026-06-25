# Changelog

All notable changes to PinIt are documented here.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

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

[2.1.1]: https://github.com/Razee4315/Pin-It/releases/tag/v2.1.1
[2.1.0]: https://github.com/Razee4315/Pin-It/releases/tag/v2.1.0
[2.0.0]: https://github.com/Razee4315/Pin-It/releases/tag/v2.0.0
