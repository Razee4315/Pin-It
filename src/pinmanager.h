#pragma once
//
// PinManager — tracks pinned windows and drives the Win32 layer.
//
// Owns the equivalent of the Rust app's global PinState plus the persistence
// and re-enforcement behaviour. UI and tray observe it via signals.
//
#include <QObject>
#include <QHash>
#include <QString>
#include <QVector>
#include <cstdint>

class QTimer;

struct PinnedWindow {
    intptr_t hwnd = 0;
    QString  title;
    QString  processName;
    int      opacity = 100;   // percent
};

class PinManager : public QObject
{
    Q_OBJECT
public:
    explicit PinManager(QObject *parent = nullptr);

    // High-level actions (hwnd as intptr_t for Qt-friendliness).
    // announce=false suppresses the pin chime + tray balloon (used when
    // re-pinning a batch of saved windows at startup, which would otherwise
    // fire one sound and one notification per window).
    bool pin(intptr_t hwnd, bool announce = true);
    bool unpin(intptr_t hwnd);
    bool toggle(intptr_t hwnd);
    bool isPinned(intptr_t hwnd) const;

    // Hotkey entry points — operate on whatever window is focused.
    void toggleForeground();
    void adjustForegroundOpacity(int deltaPercent);

    bool setOpacity(intptr_t hwnd, int percent);

    QVector<PinnedWindow> pinnedWindows() const;
    int pinnedCount() const { return m_pinned.size(); }

    // Restore pins saved from a previous session (called once at startup).
    void restoreSaved();

    // On exit: undo always-on-top + opacity on every pinned foreign window so
    // they aren't left stuck topmost/translucent. After a manual quit the pins
    // are then forgotten (clear memory + pinned.json) so a manual relaunch
    // starts clean; after a session end (logoff/shutdown/restart) the saved
    // pins are kept so they're re-pinned on the next login.
    void restoreAllWindows();

    // Called when Windows signals a logoff/shutdown/restart (see commitDataRequest
    // in main). Makes the next restoreAllWindows() keep the saved pins so the
    // advertised "pins come back after a restart" behaviour works.
    void markSessionEnding() { m_sessionEnding = true; }

signals:
    void pinsChanged();
    void pinToggled(bool isPinned, const QString &title, const QString &process);
    void opacityChanged(intptr_t hwnd, int percent);
    void errorOccurred(const QString &message);

private slots:
    void reenforce();          // periodic: re-apply topmost, drop dead windows

private:
    void persist() const;
    void schedulePersist();    // coalesce rapid writes (opacity slider drags)
    void updateTimer();        // run the re-enforce timer only while pins exist

    QHash<intptr_t, PinnedWindow> m_pinned;
    QTimer *m_timer = nullptr;
    QTimer *m_persistTimer = nullptr;  // single-shot debounce for persist()
    bool    m_sessionEnding = false;   // true once Windows is logging off/shutting down
};
