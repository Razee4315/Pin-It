#pragma once
//
// shortcuts — parse a Tauri-style shortcut string ("super+ctrl+KeyT") into
// Win32 RegisterHotKey modifiers + virtual-key code. Kept separate from the
// hotkey manager so it can be unit-tested without the rest of the app.
//
#include <QString>
#include <QStringList>

namespace shortcuts {

// Parse `s` into `mods` (MOD_* flags) and `vk` (virtual-key code).
// Returns false if the string has no key or an unrecognised token.
bool parse(const QString &s, unsigned &mods, unsigned &vk);

// Turn a Tauri-style shortcut ("super+ctrl+KeyT") into display tokens for the
// UI, e.g. ["Win", "Ctrl", "T"]. Used by the main window and the editor dialog.
QStringList displayTokens(const QString &s);

// Build a Tauri-style shortcut string from modifier flags + a key label
// ("T", "5", "=", "-"). Inverse of displayTokens/parse for the editor dialog.
QString build(bool win, bool ctrl, bool alt, bool shift, const QString &key);

} // namespace shortcuts
