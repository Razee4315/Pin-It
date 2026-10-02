#pragma once
//
// persistence — load/save PinIt's state to %LOCALAPPDATA%\PinIt\pinned.json.
//
// This is the SAME file and JSON schema the Tauri version used, so an existing
// install's pins and settings carry straight over to this C++ build.
//
#include <QString>
#include <QVector>

namespace persistence {

// One saved pin. opacity is stored as 8-bit alpha (0-255) to match the
// on-disk format written by the Rust app.
struct SavedPin {
    QString processName;
    QString title;
    int     opacity = 255;   // alpha
    bool    clickThrough = false;
    // What the window looked like before PinIt first touched it. Saved so that
    // re-pinning after a crash (when the window still carries our changes)
    // doesn't mistake them for the app's own. Default true = "unknown, trust
    // what the window looks like now", which is right for older files.
    bool    wasLayered = true;
    bool    wasTopmost = true;
    bool    wasClickThrough = true;
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
    // System notification on every pin/unpin. Off by default: the outline
    // around the window already confirms it, and notifications pile up in the
    // Notification Center. Errors are always shown.
    bool           showNotifications = false;
    bool           hasSeenTrayNotice = false;
    bool           startWithWindows = false;
    ShortcutConfig shortcuts;
};

// Restored pin request: process + title to match against live windows.
struct SavedState {
    QVector<SavedPin> pins;
    UserSettings      settings;
};

// Where PinIt keeps its files (pinned.json, pinit.log): %LOCALAPPDATA%\PinIt.
QString dataDir();

SavedState load();
void       save(const SavedState &state);

UserSettings   loadSettings();
void           saveSettings(const UserSettings &settings);

// Replace just the pin list, preserving settings.
void savePins(const QVector<SavedPin> &pins);

// The file is read once and kept in memory; every save updates that copy and
// rewrites the file. Forget the copy so the next load() reads the file again
// (after the file was changed from outside — in practice, by tests).
void dropCache();

} // namespace persistence
