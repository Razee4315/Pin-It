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
QString windowTitle(void *hwnd);
QString processName(void *hwnd);
void   *foregroundWindow();          // nullptr if none
bool    isValidWindow(void *hwnd);
bool    isTopmost(void *hwnd);
bool    isLayered(void *hwnd);        // window already has WS_EX_LAYERED

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

// Percent <-> 8-bit alpha, rounded so the round-trip is lossless (no drift).
int percentToAlpha(int percent);
int alphaToPercent(int alpha);

// --- Enumeration ----------------------------------------------------------
// Every visible, non-tool top-level window.
QVector<PinnableWindow> enumerateWindows();

// --- Misc -----------------------------------------------------------------
// Play the system default notification sound (used for the pin chime).
void beep();

} // namespace winpin
