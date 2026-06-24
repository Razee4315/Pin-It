#pragma once
//
// GlobalHotkeyManager — registers system-wide hotkeys via RegisterHotKey and
// turns the resulting WM_HOTKEY messages into Qt signals.
//
// Shortcut strings use the Tauri syntax stored in pinned.json
// (e.g. "super+ctrl+KeyT") so configuration stays file-compatible.
//
#include <QObject>
#include <QAbstractNativeEventFilter>

#include "persistence.h"

class GlobalHotkeyManager : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit GlobalHotkeyManager(QObject *parent = nullptr);
    ~GlobalHotkeyManager() override;

    // Register the four PinIt shortcuts. Returns false only if none could be
    // registered; partial failures are reported via failedActions().
    bool registerAll(const persistence::ShortcutConfig &config);
    void unregisterAll();

    QStringList failedActions() const { return m_failed; }

    bool nativeEventFilter(const QByteArray &eventType, void *message,
                           qintptr *result) override;

signals:
    void togglePin();
    void opacityUp();
    void opacityDown();
    void toggleWindow();

private:
    bool registerOne(int id, const QString &shortcut);

    QStringList m_failed;
    bool        m_anyRegistered = false;
};
