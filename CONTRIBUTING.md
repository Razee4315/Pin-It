# Contributing to PinIt

Thanks for your interest in improving PinIt! ❤️ All contributions — bug reports,
ideas, docs, and code — are welcome.

This project is governed by our [Code of Conduct](CODE_OF_CONDUCT.md). By
participating, you agree to uphold it. Report unacceptable behaviour to
<saqlainrazee@gmail.com>.

## Reporting bugs / requesting features

Search the [existing issues](https://github.com/Razee4315/Pin-It/issues) first.
If nothing matches, open a new one using the issue templates. For bugs, please
include your Windows version, PinIt version (Help → About), steps to reproduce,
and — if possible — the log at `%LOCALAPPDATA%\PinIt\pinit.log`.

## Development setup

**Prerequisites**

- [Qt 6](https://www.qt.io/download-open-source) (Widgets) with a C++17 compiler
  (MinGW or MSVC)
- [CMake](https://cmake.org/) 3.21+

**Build & test**

```bash
git clone https://github.com/Razee4315/Pin-It.git
cd Pin-It
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<path-to-Qt>
cmake --build build
ctest --test-dir build --output-on-failure
```

The app is `build/PinIt.exe`. See the README for packaging (windeployqt + Inno Setup).

## Code style & conventions

- Formatting is defined by [`.clang-format`](.clang-format) — run
  `clang-format -i` on files you touch.
- Builds run with `-Wall -Wextra`; keep your changes warning-clean.
- Add or update tests in `tests/` for any logic change; CI runs them on every push.
- Keep the architecture's separation: Win32 in `winpin`, state/logic in
  `PinManager`, UI in `MainWindow`. No business logic in widgets.
- Match the surrounding code's naming and style.

## Pull requests

1. Branch off `main`.
2. Keep PRs focused; write a clear description of the change and why.
3. Make sure the build is clean and tests pass before pushing.
