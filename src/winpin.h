#pragma once
//
// winpin — thin C++ wrappers around the Win32 calls PinIt needs.
//
// This is the direct port of the Rust `always_on_top` module: always-on-top
// via SetWindowPos(HWND_TOPMOST), per-window opacity via
// SetLayeredWindowAttributes, and window enumeration via EnumWindows.
//
// HWNDs are passed around as void* so this header doesn't drag <windows.h>
// into the rest of the app. The .cpp casts them back to HWND.
//
#include <QRect>
#include <QString>
#include <QVector>
#include <cstdint>

namespace winpin {

// Opacity is expressed to the UI as a percentage and clamped to this range
// (matching the Rust port — fully transparent windows would be unusable).
constexpr int kMinOpacity = 20;
constexpr int kMaxOpacity = 100;

// A top-level window the user could pin (used by the "add window" picker).
struct PinnableWindow {
    intptr_t hwnd = 0;
    QString  title;
    QString  processName;
};

// --- Window metadata ------------------------------------------------------
QString windowTitle(void *hwnd);     // empty if the window has no title
QString processName(void *hwnd);     // e.g. "notepad.exe"; empty if it can't be read
void   *foregroundWindow();          // nullptr if none
bool    isValidWindow(void *hwnd);
bool    isTopmost(void *hwnd);
bool    isLayered(void *hwnd);        // window already has WS_EX_LAYERED
QString className(void *hwnd);        // Win32 window class, e.g. "Notepad"
// The window's visible frame in physical screen pixels (without the invisible
// resize border Windows 10/11 add around it). Empty if it can't be read.
QRect   frameRect(void *hwnd);

// --- What may be pinned ---------------------------------------------------
// True for the window classes that make up the Windows shell itself (desktop,
// taskbar, Start, Task View…). Pinning those makes no sense and can cover
// everything else, so they are never offered or accepted.
bool isShellClass(const QString &className);
// A real, foreign application window: valid, not part of the shell and not one
// of PinIt's own windows.
bool isPinnable(void *hwnd);

// --- Always-on-top --------------------------------------------------------
bool applyTopmost(void *hwnd);       // HWND_TOPMOST
bool removeTopmost(void *hwnd);      // HWND_NOTOPMOST

// --- Transparency ---------------------------------------------------------
// percent is clamped to [kMinOpacity, kMaxOpacity].
bool setOpacityPercent(void *hwnd, int percent);
int  opacityPercent(void *hwnd);     // 100 if the window isn't layered
// Back to fully opaque. Only removes WS_EX_LAYERED when keepLayered is false;
// pass true when the window had the style before PinIt touched it, so we don't
// strip a style the app relies on for its own transparency.
bool restoreOpacity(void *hwnd, bool keepLayered = false);

// --- Click-through --------------------------------------------------------
// A click-through window ignores the mouse: clicks land on whatever is behind
// it (WS_EX_TRANSPARENT). Useful with a faded window kept over one's work.
bool isClickThrough(void *hwnd);
bool setClickThrough(void *hwnd, bool enabled);

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
