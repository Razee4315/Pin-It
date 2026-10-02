#pragma once
//
// winpin — thin C++ wrappers around the Win32 calls PinIt needs.
//
// This is the direct port of the Rust `always_on_top` module: always-on-top
// via SetWindowPos(HWND_TOPMOST), per-window opacity via
// SetLayeredWindowAttributes, and window enumeration via EnumWindows.
//
// Windows are identified by a WindowId — the HWND as an integer — so this
// header doesn't drag <windows.h> into the rest of the app. Only winpin.cpp
// converts it back to an HWND.
//
#include <QRect>
#include <QString>
#include <QVector>
#include <cstdint>

namespace winpin {

using WindowId = intptr_t;   // 0 = no window

// Opacity is expressed to the UI as a percentage and clamped to this range
// (matching the Rust port — fully transparent windows would be unusable).
constexpr int kMinOpacity = 20;
constexpr int kMaxOpacity = 100;

// A top-level window the user could pin (used by the "add window" picker).
struct PinnableWindow {
    WindowId hwnd = 0;
    QString  title;
    QString  processName;
};

// --- Window metadata ------------------------------------------------------
QString windowTitle(WindowId hwnd);     // empty if the window has no title
QString processName(WindowId hwnd);     // e.g. "notepad.exe"; empty if it can't be read
WindowId foregroundWindow();        // 0 if none
bool    isValidWindow(WindowId hwnd);
bool    isTopmost(WindowId hwnd);
bool    isLayered(WindowId hwnd);        // window already has WS_EX_LAYERED
QString className(WindowId hwnd);        // Win32 window class, e.g. "Notepad"
// The window's visible frame in physical screen pixels (without the invisible
// resize border Windows 10/11 add around it). Empty if it can't be read.
QRect   frameRect(WindowId hwnd);

// --- What may be pinned ---------------------------------------------------
// True for the window classes that make up the Windows shell itself (desktop,
// taskbar, Start, Task View…). Pinning those makes no sense and can cover
// everything else, so they are never offered or accepted.
bool isShellClass(const QString &className);
// A real, foreign application window: valid, not part of the shell and not one
// of PinIt's own windows.
bool isPinnable(WindowId hwnd);

// --- Always-on-top --------------------------------------------------------
bool applyTopmost(WindowId hwnd);       // HWND_TOPMOST
bool removeTopmost(WindowId hwnd);      // HWND_NOTOPMOST

// --- Transparency ---------------------------------------------------------
// percent is clamped to [kMinOpacity, kMaxOpacity].
bool setOpacityPercent(WindowId hwnd, int percent);
int  opacityPercent(WindowId hwnd);     // 100 if the window isn't layered
// Back to fully opaque. Only removes WS_EX_LAYERED when keepLayered is false;
// pass true when the window had the style before PinIt touched it, so we don't
// strip a style the app relies on for its own transparency.
bool restoreOpacity(WindowId hwnd, bool keepLayered = false);

// --- Click-through --------------------------------------------------------
// A click-through window ignores the mouse: clicks land on whatever is behind
// it (WS_EX_TRANSPARENT). Useful with a faded window kept over one's work.
bool isClickThrough(WindowId hwnd);
bool setClickThrough(WindowId hwnd, bool enabled);

// Percent <-> 8-bit alpha, rounded so the round-trip is lossless (no drift).
int percentToAlpha(int percent);
int alphaToPercent(int alpha);

// --- Enumeration ----------------------------------------------------------
// Every window the user could meaningfully pin: visible, titled, pinnable (see
// isPinnable), not a tool window and not "cloaked" (Windows keeps suspended
// Store apps and windows on other virtual desktops around as visible-but-hidden).
QVector<PinnableWindow> enumerateWindows();

// --- Foreground changes ---------------------------------------------------
// Calls `callback(context)` on this thread's event loop whenever another
// application's window comes to the front. One watcher at a time; watching
// again replaces the previous one.
using ForegroundCallback = void (*)(void *context);
bool watchForeground(ForegroundCallback callback, void *context);
void stopWatchingForeground();

// --- Misc -----------------------------------------------------------------
// False when the user turned off "Animation effects" in Windows settings.
bool animationsEnabled();

// Play the system default notification sound (used for the pin chime).
void beep();

} // namespace winpin
