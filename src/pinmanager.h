#pragma once
//
// PinManager — tracks pinned windows and drives the Win32 layer.
//
// Owns the equivalent of the Rust app's global PinState plus the persistence
// and re-enforcement behaviour. UI and tray observe it via signals.
//
#include <QObject>
#include <QString>
#include <QVector>
#include <cstdint>

#include "persistence.h"

class QTimer;

struct PinnedWindow {
    intptr_t hwnd = 0;
    QString  title;
    QString  processName;
    int      opacity = 100;        // percent
    bool     wasLayered = false;   // window had WS_EX_LAYERED before we pinned it
    bool     wasTopmost = false;   // window was already always-on-top before we pinned it
    bool     opacityChanged = false;  // we changed its opacity, so undo it on unpin
    bool     clickThrough = false;    // mouse clicks pass through it
    bool     wasClickThrough = false; // ...and it already did before we pinned it
};

class PinManager : public QObject
{
    Q_OBJECT
public:
    explicit PinManager(QObject *parent = nullptr);
    ~PinManager() override;

    // High-level actions (hwnd as intptr_t for Qt-friendliness).
    // announce=false makes the pin silent — no chime, no outline, no error
    // message. Used when re-pinning saved windows, which happens without the
    // user asking and may be retried.
    bool pin(intptr_t hwnd, bool announce = true);
    bool unpin(intptr_t hwnd);
    // Unpin every window and stop waiting for the pending ones. Returns how
    // many live windows were unpinned.
    int  unpinAll();
    bool toggle(intptr_t hwnd);
    bool isPinned(intptr_t hwnd) const;

    // Hotkey entry points — operate on whatever window is focused.
    void toggleForeground();
    void adjustForegroundOpacity(int deltaPercent);

    bool setOpacity(intptr_t hwnd, int percent);
    // Let mouse clicks pass through a pinned window (or stop doing so).
    bool setClickThrough(intptr_t hwnd, bool enabled);

    // In the order the windows were pinned.
    QVector<PinnedWindow> pinnedWindows() const { return m_pinned; }
    int pinnedCount() const { return m_pinned.size(); }

    // Restore pins saved from a previous session (called once at startup).
    // Saved pins whose window isn't open yet are kept as "pending" and applied
    // when that window shows up — after a reboot PinIt usually starts before
    // the apps it had pinned.
    void restoreSaved();

    // Saved pins still waiting for their window.
    QVector<persistence::SavedPin> pendingPins() const { return m_pending; }
    void forgetPending(int index);

    // On exit: undo always-on-top + opacity on every pinned foreign window so
    // they aren't left stuck topmost/translucent. After a manual quit the pins
    // are then forgotten (clear memory + pinned.json) so a manual relaunch
    // starts clean; after a session end (logoff/shutdown/restart) the saved
    // pins are kept so they're re-pinned on the next login.
    void restoreAllWindows();

    // Called when Windows signals a logoff/shutdown/restart (see SessionWatcher).
    // From then on the pin list is frozen: the other apps are closing, and a
    // window disappearing must not be mistaken for the user closing it.
    // restoreAllWindows() then keeps the saved pins so the advertised "pins
    // come back after a restart" behaviour works.
    void markSessionEnding() { m_sessionEnding = true; }
    // The shutdown was called off (another app blocked it, or the user
    // cancelled): a later Quit is a manual quit again.
    void clearSessionEnding() { m_sessionEnding = false; }

signals:
    void pinsChanged();
    void pinToggled(intptr_t hwnd, bool isPinned, const QString &title);
    void pinRestored(intptr_t hwnd);   // a pending pin found its window
    void opacityChanged(intptr_t hwnd, int percent);
    void titleChanged(intptr_t hwnd, const QString &title);
    void clickThroughChanged(intptr_t hwnd, bool enabled);
    void errorOccurred(const QString &message);

private slots:
    // Periodic and on foreground changes: re-apply topmost, drop dead windows,
    // refresh titles, apply pending pins.
    void reenforce();

private:
    // Give a window back the way we found it. Returns false if it is gone.
    static bool release(const PinnedWindow &window);
    void persist() const;
    void schedulePersist();    // coalesce rapid writes (opacity slider drags)
    void updateTimer();        // watch (timer + foreground events) only while there is work
    bool applySaved(intptr_t hwnd, const persistence::SavedPin &saved);
    void restorePending();     // pin the foreground window if a pending pin matches it

    PinnedWindow *find(intptr_t hwnd);
    const PinnedWindow *find(intptr_t hwnd) const;

    QVector<PinnedWindow> m_pinned;    // pin order — also the order the UI lists them
    QVector<persistence::SavedPin> m_pending;   // saved pins whose window isn't open yet
    QTimer *m_timer = nullptr;
    QTimer *m_persistTimer = nullptr;  // single-shot debounce for persist()
    bool    m_sessionEnding = false;   // true once Windows is logging off/shutting down
};
