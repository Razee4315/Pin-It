# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

PinIt is a Windows-only "Always on Top" utility built with Tauri v2. It lets users pin any window to stay on top via global hotkeys, with opacity control and persistence across restarts. The frontend is React+TypeScript, the backend is Rust using the Windows API directly.

## Build & Development Commands

```bash
bun install                # Install frontend dependencies
bun run tauri dev          # Start development (Vite dev server + Rust backend)
bun run tauri build        # Production build (MSI + NSIS installers)
bun run build              # Frontend only: TypeScript check + Vite build
cargo check                # Rust type check (run from src-tauri/)
```

CI runs `bun run build` (frontend type check) and `cargo check` (Rust check) on Windows. There are no test suites configured.

## Architecture

### Frontend → Backend Communication

The app uses Tauri's IPC invoke system. Frontend calls are defined in `src/commands.ts` using `invoke()`, which maps to Rust functions annotated with `#[tauri::command]` in `src-tauri/src/commands.rs`. Types shared across the boundary are in `src/types.ts` (TypeScript) and the Rust structs use `serde::Serialize`.

### Rust Backend Modules (`src-tauri/src/`)

- **`lib.rs`** — App initialization: plugin registration, event hooks, global shortcuts, tray icon setup, persistence restore on startup, state save on exit
- **`commands.rs`** — All Tauri IPC command handlers (toggle pin, opacity, focus, auto-start, sound settings, tray notice)
- **`persistence.rs`** — Save/restore pinned window state + user settings to `%LOCALAPPDATA%/PinIt/pinned.json`. `UserSettings` stores sound/tray notice prefs. Persistence matches by process name + title on restore.
- **`autostart.rs`** — Windows Registry manipulation for auto-start (`HKCU\...\Run`)
- **`always_on_top/`** — Core module:
  - `pin_manager.rs` — Window pinning via `SetWindowPos` with `HWND_TOPMOST`, process name extraction
  - `state.rs` — Global `Lazy<RwLock<HashMap>>` storing `PinnedWindow` structs
  - `hotkey.rs` — Global shortcut registration (Win+Ctrl+T/=/-/P) and handler logic, emits events to frontend
  - `event_hook.rs` — `SetWinEventHook` callbacks that re-enforce topmost on window events (critical for Win11 stability). Stores `AppHandle` in a static to emit `window-destroyed` events from C callbacks.
  - `transparency.rs` — `SetLayeredWindowAttributes` for per-window opacity control
  - `error.rs` — `PinError` enum with `thiserror`

### Frontend (`src/`)

Single-component app in `App.tsx`. Uses React hooks for state. Listens to Tauri events (`pin-toggled`, `opacity-changed`, `window-destroyed`, `pin-error`) emitted from the Rust backend to stay in sync.

### Key Design Decisions

- **Window close is intercepted** — the main window hides instead of closing (`lib.rs` `on_window_event`), keeping the app in the system tray
- **Win11 topmost re-enforcement** — Windows 11's DWM compositor can strip the topmost flag; `event_hook.rs` re-applies it on window events like `LOCATIONCHANGE` and `MOVESIZEEND`
- **Global state is a module-level static** — `PINNED_WINDOWS` in `state.rs` is a `Lazy<RwLock<HashMap<isize, PinnedWindow>>>`, accessed from hotkey handlers, commands, and event hooks across threads
- **Opacity stored as 0-255 internally** — The UI shows percentages (20-100%), conversion happens at the boundary
- **Event-driven UI sync** — `event_hook.rs` stores a global `AppHandle` to emit events from C callbacks; the frontend listens for `window-destroyed`, `pin-error`, etc.
- **Dark mode** — CSS `prefers-color-scheme: dark` media query auto-switches theme variables

## Version Bumping

Version is specified in three places that must stay in sync:
- `package.json` → `version`
- `src-tauri/Cargo.toml` → `version`
- `src-tauri/tauri.conf.json` → `version`

The release workflow auto-creates a GitHub release when a new version tag is pushed to main.
