<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/wordmark-dark.png">
    <img src="docs/assets/wordmark-light.png" width="360" alt="PinIt — pin any window always on top">
  </picture>
</p>

<p align="center">
  <b>Keep any window always on top on Windows 11 & 10 — instantly, with a global hotkey.</b>
</p>

<p align="center">
  <a href="https://github.com/Razee4315/Pin-It/releases/latest"><img src="https://img.shields.io/github/v/release/Razee4315/Pin-It?style=flat-square" alt="Latest release"></a>
  <a href="https://github.com/Razee4315/Pin-It/releases"><img src="https://img.shields.io/github/downloads/Razee4315/Pin-It/total?style=flat-square" alt="Downloads"></a>
  <a href="https://github.com/Razee4315/Pin-It/releases/latest"><img src="https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078D6?style=flat-square" alt="Platform"></a>
  <a href="https://www.qt.io"><img src="https://img.shields.io/badge/built%20with-C%2B%2B%20%26%20Qt%206-41CD52?style=flat-square" alt="Built with C++ and Qt 6"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-Apache--2.0-green?style=flat-square" alt="License: Apache 2.0"></a>
</p>

Press `Win+Ctrl+T` and the focused window stays on top of everything else. Slide its opacity down to see through it. Restart your PC and PinIt re-pins it automatically. A single-purpose alternative to installing a whole utility suite — written in native C++ with Qt, talking directly to the Windows API.

<p align="center">
  <img src="docs/assets/pin-it-demo.gif" width="720" alt="PinIt in action: pinning a Notepad window always on top with Win+Ctrl+T, then fading its opacity to see through it">
  <br>
  <em>Pin a window, fade it, and keep working — all from one hotkey</em>
</p>

## Download

**[⬇ Download the latest release](https://github.com/Razee4315/Pin-It/releases/latest)** — Windows 10 & 11, free.

| File | What it is |
|------|------------|
| `PinIt_x.y.z_x64-setup.exe` | Installer (recommended) — Start Menu shortcut + uninstaller |
| `PinIt-portable-x64.zip` | Portable — unzip and run, no installation |

> **Note:** The installers are not yet code-signed, so Windows SmartScreen may show "Windows protected your PC". Click **More info → Run anyway**. PinIt is fully open source (Apache 2.0) — audit the code or build it yourself from this repository.

## Features

- **Global hotkey pinning** — `Win+Ctrl+T` pins/unpins the focused window. No clicking through menus.
- **Per-window transparency** — make any pinned window see-through with `Win+Ctrl+=` / `Win+Ctrl+-` or a slider. Great for reference docs, video calls, or notes over your work.
- **Pins survive restarts** — PinIt remembers what you pinned (and its opacity) and re-pins it when you log back in.
- **Windows 11 topmost re-enforcement** — Win11's compositor sometimes strips the always-on-top flag; PinIt re-applies it automatically.
- **System tray app** — closes to the tray and stays out of your way. Optional start-with-Windows.
- **Tiny and fast** — native C++/Qt talking directly to the Windows API. Minimal RAM, instant response.

## Keyboard Shortcuts

| Action | Default shortcut |
|--------|------------------|
| Pin / unpin focused window | `Win` + `Ctrl` + `T` |
| Increase opacity | `Win` + `Ctrl` + `=` |
| Decrease opacity | `Win` + `Ctrl` + `-` |
| Show / hide PinIt | `Win` + `Ctrl` + `P` |

## How PinIt compares

PowerToys is great when you want twenty utilities. PinIt is for when you want exactly one, done properly:

| Feature | **PinIt** | PowerToys <sub>Always On Top module</sub> | DeskPins <sub>classic freeware</sub> |
|---|:---:|:---:|:---:|
| Price | ✅ Free | ✅ Free | ✅ Free |
| Single-purpose, lightweight | ✅ **Yes** | ❌ Full utility suite | ✅ Yes |
| Global hotkey to pin | ✅ `Win+Ctrl+T` | ✅ Yes | ✅ Yes |
| True per-window transparency | ✅ **20 to 100%** | ❌ No ¹ | ❌ No |
| Pins persist across restarts | ✅ **Automatic** | ❌ Forgets on reboot | ❌ No |
| Actively maintained | ✅ Yes | ✅ Yes | ❌ Last release 2018 |

> **The short version:** if pinning is all you need, PinIt is the smallest tool that does all of it, and the only one that remembers your pins after a reboot.

¹ PowerToys' "Opacity" setting changes the highlight *border* around pinned windows, not the window content itself. True window transparency is a long-standing open feature request ([#26049](https://github.com/microsoft/PowerToys/issues/26049)).

## FAQ

### How do I keep a window always on top in Windows 11?

Windows has no built-in always-on-top button. Install PinIt, click the window you want to keep visible, and press `Win+Ctrl+T` — it stays on top of every other window until you unpin it.

### What is the keyboard shortcut to pin a window on top?

PinIt's default is `Win+Ctrl+T` to toggle pinning.

### Can I make a window transparent / see-through on Windows?

Yes — pin a window with PinIt, then press `Win+Ctrl+-` to fade it (down to 20% opacity) or `Win+Ctrl+=` to make it solid again. Each pinned window keeps its own opacity level.

### Do my pinned windows stay on top after I restart?

Yes. PinIt saves your pins (per app, with their opacity) to `%LOCALAPPDATA%\PinIt` and re-pins matching windows on the next launch — something neither PowerToys nor DeskPins does.

### Does it work with apps running as administrator?

Windows security (UIPI) prevents normal apps from modifying elevated windows. To pin a window that's running as administrator, run PinIt as administrator too.

### Is PinIt free and open source?

Yes — PinIt is completely free and open source under the [Apache 2.0 license](LICENSE). Use it, modify it, and redistribute it, including commercially.

## Building from source

### Prerequisites

- [Qt 6](https://www.qt.io/download-open-source) (Widgets) with a C++17 compiler (MinGW or MSVC)
- [CMake](https://cmake.org/) 3.21+

### Build

```bash
# Clone the repository
git clone https://github.com/Razee4315/Pin-It.git
cd Pin-It

# Configure and build (point CMAKE_PREFIX_PATH at your Qt install)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<path-to-Qt>
cmake --build build
```

The executable is `build/PinIt.exe`. To run it standalone, bundle the Qt runtime with `windeployqt`. To produce the installer, run the [Inno Setup](https://jrsoftware.org/isinfo.php) script at `installer/PinIt.iss`.

> Every push is built automatically by GitHub Actions (`.github/workflows/build.yml`), which produces the installer and a portable ZIP. Pushing a `v*` tag publishes them to a GitHub Release.

## Why PinIt?

As a developer moving between Linux and Windows, I always missed the native ability to keep any window on top. While Linux desktop environments often have this built-in, Windows options were limited.

The most common solution, Microsoft PowerToys, comes bundled with dozens of other utilities I didn't need. I wanted a lightweight, singular-purpose tool that does one thing and does it well — without the bloat. PinIt was born from the desire for a clean, simple, and resource-efficient alternative that feels like a native part of the system.

## Tech Stack

- **Language**: C++17
- **UI**: Qt 6 (Widgets), direct Windows API
- **Build**: CMake + Inno Setup

> Originally built with Rust + Tauri (v1.x). The previous implementation is preserved on the [`legacy-tauri`](https://github.com/Razee4315/Pin-It/tree/legacy-tauri) branch.

## License

PinIt is open source under the **[Apache License 2.0](LICENSE)** — free to use, modify, and redistribute, including for commercial purposes.

## Author

**Saqlain Abbas**
Email: saqlainrazee@gmail.com

GitHub: [@Razee4315](https://github.com/Razee4315)
