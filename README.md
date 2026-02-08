<p align="center">
  <img src="public/logo.svg" width="80" alt="PinIt Logo">
  <h1 align="center">PinIt</h1>
</p>

A minimal, distraction-free "Always on Top" utility for Windows built with Tauri v2.

## Why PinIt?

As a developer moving between Linux and Windows, I always missed the native ability to keep any window on top. While Linux desktop environments often have this built-in, Windows options were limited.

The most common solution, Microsoft PowerToys, comes bundled with dozens of other utilities I didn't need. I wanted a lightweight, singular purpose tool that does one thing and does it well—without the bloat. PinIt was born from the desire for a clean, simple, and resource-efficient alternative that feels like a native part of the system.

## Features

- **Global Hotkey** - Pin any window instantly with `Win+Ctrl+T`
- **Transparency Control** - Adjust window opacity with `Win+Ctrl+=` and `Win+Ctrl+-`
- **Minimal Interface** - Unobtrusive UI that stays out of your way
- **System Tray Integration** - Runs quietly in the background
- **Two Themes** - Clean Paper (Light) and Dark themes
- **Native Performance** - Built with Rust and Tauri for minimal resource usage

## Installation

Download the latest release from the [Releases](https://github.com/StartVision/PinIt/releases) page.

### Available Formats

- **Windows**: `.msi` installer or `.exe` portable

## Development

### Prerequisites

- [Node.js](https://nodejs.org/) (v18+)
- [Bun](https://bun.sh/) (recommended) or npm
- [Rust](https://www.rust-lang.org/tools/install)

### Setup

```bash
# Clone the repository
git clone https://github.com/StartVision/Pin-It.git
cd Pin-It

# Install dependencies
bun install

# Run in development mode
bun run tauri dev

# Build for production
bun run tauri build
```

## Keyboard Shortcuts

| Action | Shortcut |
|--------|----------|
| Toggle Pin | `Win` + `Ctrl` + `T` |
| Increase Opacity | `Win` + `Ctrl` + `=` |
| Decrease Opacity | `Win` + `Ctrl` + `-` |

## Tech Stack

- **Frontend**: React, TypeScript, Vite
- **Backend**: Rust, Tauri v2
- **Build**: Bun

## License

This project is **source available** with restricted commercial use:
- **Personal use** - Free to use, copy, and modify
- **Commercial use** - Requires written permission from the author

See the [LICENSE](LICENSE) file for full details.

## Author

**Saqlain Abbas**
Email: saqlainrazee@gmail.com

GitHub: [@Razee4315](https://github.com/Razee4315)
