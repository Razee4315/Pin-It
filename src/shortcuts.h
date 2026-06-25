#pragma once
//
// shortcuts — parse a Tauri-style shortcut string ("super+ctrl+KeyT") into
// Win32 RegisterHotKey modifiers + virtual-key code. Kept separate from the
// hotkey manager so it can be unit-tested without the rest of the app.
//
#include <QString>

namespace shortcuts {

// Parse `s` into `mods` (MOD_* flags) and `vk` (virtual-key code).
// Returns false if the string has no key or an unrecognised token.
bool parse(const QString &s, unsigned &mods, unsigned &vk);

} // namespace shortcuts
