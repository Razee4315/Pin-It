# PinIt (C++ / Qt)

Keep any window **always on top** on Windows 10 & 11 — instantly, with a global hotkey.

Press `Win + Ctrl + T` and the focused window stays on top of everything else. Fade its
opacity to see through it. Pins survive restarts. This is a native **C++ / Qt 6** port of the
original [Tauri-based PinIt](https://github.com/Razee4315/Pin-It).

## Features

- **Global hotkey pinning** — `Win + Ctrl + T` pins/unpins the focused window.
- **Per-window transparency** — `Win + Ctrl + =` / `Win + Ctrl + -`, or a slider in the app.
- **Pins survive restarts** — re-pins your windows (and opacity) when you log back in.
- **Windows 11 topmost re-enforcement** — re-applies always-on-top if the compositor drops it.
- **System tray app** — closes to the tray; optional start-with-Windows.
- **Tiny & native** — talks directly to the Win32 API.

## Keyboard shortcuts

| Action | Default |
|--------|---------|
| Pin / unpin focused window | `Win + Ctrl + T` |
| Increase opacity | `Win + Ctrl + =` |
| Decrease opacity | `Win + Ctrl + -` |
| Show / hide PinIt | `Win + Ctrl + P` |

## Building from source

Requires **Qt 6** (Widgets) and a C++17 compiler (MinGW or MSVC) + CMake.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<path-to-Qt>
cmake --build build
```

The executable is `build/PinIt.exe`. To run it standalone, bundle the Qt runtime with
`windeployqt`.

## Packaging

```sh
# 1. Bundle the Qt runtime
windeployqt --release --no-translations dist/PinIt/PinIt.exe
# 2. Build the installer (needs Inno Setup 6)
ISCC installer/PinIt.iss      # -> release/PinIt_<version>_x64-setup.exe
```

## CI

`.github/workflows/build.yml` builds the app, bundles Qt, and produces the installer +
portable ZIP on every push. Pushing a `v*` tag publishes them to a GitHub Release.

## License

Apache-2.0 — see the original project for details.

## Project layout

```
src/            C++ sources (Win32 layer, pin manager, hotkeys, UI)
resources/      Logo/icon assets, Qt .qrc, Windows .rc
installer/      Inno Setup script
.github/        GitHub Actions build workflow
```
