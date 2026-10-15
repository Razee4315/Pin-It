# PinIt in Rust (Slint) — work in progress

A pure-Rust port of PinIt: no Qt, no webview, no JavaScript. The UI is
[Slint](https://slint.dev) and every Windows call goes through the
[`windows`](https://crates.io/crates/windows) crate. It reads and writes the
same `pinned.json` as the Qt build, so pins and settings carry over.

Slint draws with its software renderer. For one small window it looks the
same as OpenGL and keeps the GPU driver out of the process. Measured on
Windows 10 with the window open:

| | Rust + Slint | Qt 6 build (2.2.0) |
|---|---|---|
| Files shipped | 1 exe, 9.5 MB | 22 files, 35.8 MB |
| Private memory | about 6 MB | about 11 MB |
| Working set | about 22 MB | about 34 MB |

## Build and run

```bash
cargo run              # debug
cargo build --release  # single PinIt exe in target/release
cargo test
```

To run next to an installed PinIt without touching it, set `PINIT_DEV=1`.
The build then uses `%LOCALAPPDATA%\PinIt-dev`, its own single-instance lock,
and its own Run-key value. The default shortcuts will be reported as
unavailable while the installed copy holds them; pick spare ones in
**Shortcuts** (for example `Ctrl+Alt+Shift+F13`).

## Layout

| File | What it does |
|---|---|
| `ui/theme.slint` | Colours, type and motion for light and dark mode |
| `ui/controls.slint` | Branded button, check box, slider, key caps, sheet |
| `ui/app.slint` | The window, the window picker, the shortcut editor, About |
| `src/main.rs` | Wires the UI, the pins, the tray and the hotkeys together |
| `src/pins.rs` | Which windows are pinned; re-applying topmost; restoring saved pins |
| `src/win.rs` | Every Win32 call on other apps' windows |
| `src/host.rs` | Hidden helper window: tray icon and menu, hotkeys, shutdown, second launch |
| `src/flash.rs` | The outline drawn around a window when it is pinned |
| `src/store.rs` | `pinned.json` |
| `src/shortcuts.rs` | Shortcut strings such as `super+ctrl+KeyT` |
| `src/autostart.rs` | The HKCU Run key |

## Not done yet

- Log file (`pinit.log`)
- Installer and CI (the Inno Setup script still packages the Qt build)
- Mica backdrop on Windows 11
- The maximize button: the window has a fixed width, but Windows still shows it
