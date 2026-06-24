#include "persistence.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>

namespace {

QString savePath()
{
    // dirs::data_local_dir() in the Rust app == %LOCALAPPDATA%.
    QString base = qEnvironmentVariable("LOCALAPPDATA");
    if (base.isEmpty())
        base = QDir::homePath();
    return QDir(base).filePath(QStringLiteral("PinIt/pinned.json"));
}

persistence::ShortcutConfig readShortcuts(const QJsonObject &o)
{
    persistence::ShortcutConfig c;
    if (o.contains("toggle_pin"))    c.togglePin    = o.value("toggle_pin").toString();
    if (o.contains("opacity_up"))    c.opacityUp    = o.value("opacity_up").toString();
    if (o.contains("opacity_down"))  c.opacityDown  = o.value("opacity_down").toString();
    if (o.contains("toggle_window")) c.toggleWindow = o.value("toggle_window").toString();
    return c;
}

QJsonObject writeShortcuts(const persistence::ShortcutConfig &c)
{
    QJsonObject o;
    o["toggle_pin"]    = c.togglePin;
    o["opacity_up"]    = c.opacityUp;
    o["opacity_down"]  = c.opacityDown;
    o["toggle_window"] = c.toggleWindow;
    return o;
}

persistence::UserSettings readSettings(const QJsonObject &o)
{
    persistence::UserSettings s;
    s.enableSound       = o.value("enable_sound").toBool(true);
    s.hasSeenTrayNotice = o.value("has_seen_tray_notice").toBool(false);
    s.startWithWindows  = o.value("start_with_windows").toBool(false);
    s.shortcuts         = readShortcuts(o.value("shortcuts").toObject());
    return s;
}

QJsonObject writeSettings(const persistence::UserSettings &s)
{
    QJsonObject o;
    o["enable_sound"]         = s.enableSound;
    o["has_seen_tray_notice"] = s.hasSeenTrayNotice;
    o["start_with_windows"]   = s.startWithWindows;
    o["shortcuts"]            = writeShortcuts(s.shortcuts);
    return o;
}

} // namespace

namespace persistence {

SavedState load()
{
    SavedState state;

    QFile f(savePath());
    if (!f.open(QIODevice::ReadOnly))
        return state;   // defaults

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return state;

    const QJsonObject root = doc.object();

    // pins: object keyed by "process:hwnd" -> { process_name, title, opacity }
    const QJsonObject pins = root.value("pins").toObject();
    for (auto it = pins.begin(); it != pins.end(); ++it) {
        const QJsonObject p = it.value().toObject();
        SavedPin sp;
        sp.processName = p.value("process_name").toString();
        sp.title       = p.value("title").toString();
        sp.opacity     = p.value("opacity").toInt(255);
        if (!sp.processName.isEmpty())
            state.pins.push_back(sp);
    }

    state.settings = readSettings(root.value("settings").toObject());
    return state;
}

void save(const SavedState &state)
{
    const QString path = savePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject pins;
    for (int i = 0; i < state.pins.size(); ++i) {
        const SavedPin &sp = state.pins[i];
        QJsonObject p;
        p["process_name"] = sp.processName;
        p["title"]        = sp.title;
        p["opacity"]      = sp.opacity;
        // Key matches the Rust format: "<process>:<index>" keeps it unique.
        pins[QStringLiteral("%1:%2").arg(sp.processName).arg(i)] = p;
    }

    QJsonObject root;
    root["pins"]     = pins;
    root["settings"] = writeSettings(state.settings);

    // QSaveFile writes to a temp file then atomically renames — same crash
    // safety the Rust version got from its tmp+rename dance.
    QSaveFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        f.commit();
    }
}

UserSettings loadSettings()
{
    return load().settings;
}

void saveSettings(const UserSettings &settings)
{
    SavedState state = load();
    state.settings = settings;
    save(state);
}

void savePins(const QVector<SavedPin> &pins)
{
    SavedState state = load();   // preserve settings
    state.pins = pins;
    save(state);
}

} // namespace persistence
