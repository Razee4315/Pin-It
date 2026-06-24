#pragma once
//
// persistence — load/save PinIt's state to %LOCALAPPDATA%\PinIt\pinned.json.
//
// This is the SAME file and JSON schema the Tauri version used, so an existing
// install's pins and settings carry straight over to this C++ build.
//
#include <QString>
#include <QHash>
#include <QVector>

namespace persistence {

// One saved pin. opacity is stored as 8-bit alpha (0-255) to match the
// on-disk format written by the Rust app.
struct SavedPin {
    QString processName;
    QString title;
    int     opacity = 255;   // alpha
};

// Configurable global shortcuts, stored in Tauri's string syntax
// (e.g. "super+ctrl+KeyT") so the file stays compatible.
struct ShortcutConfig {
    QString togglePin    = QStringLiteral("super+ctrl+KeyT");
    QString opacityUp    = QStringLiteral("super+ctrl+Equal");
    QString opacityDown  = QStringLiteral("super+ctrl+Minus");
    QString toggleWindow = QStringLiteral("super+ctrl+KeyP");
};

struct UserSettings {
    bool           enableSound      = true;
    bool           hasSeenTrayNotice = false;
    bool           startWithWindows = false;
    ShortcutConfig shortcuts;
};

// Restored pin request: process + title to match against live windows.
struct SavedState {
    QVector<SavedPin> pins;
    UserSettings      settings;
};

SavedState load();
void       save(const SavedState &state);

UserSettings   loadSettings();
void           saveSettings(const UserSettings &settings);

// Replace just the pin list, preserving settings.
void savePins(const QVector<SavedPin> &pins);

} // namespace persistence
