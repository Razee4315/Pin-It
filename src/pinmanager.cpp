#include "pinmanager.h"
#include "winpin.h"
#include "persistence.h"
#include "pinmatch.h"

#include <QTimer>
#include <QSet>
#include <QtGlobal>

namespace {
inline void *H(intptr_t h) { return reinterpret_cast<void *>(h); }
} // namespace

PinManager::PinManager(QObject *parent)
    : QObject(parent)
{
    // Windows 11's compositor occasionally strips the topmost flag. Rather
    // than wiring a SetWinEventHook callback, we re-assert it on a timer and
    // sweep out windows that have since closed. Cheap and robust.
    // The timer only runs while at least one window is pinned (see updateTimer)
    // so an idle PinIt uses zero CPU.
    m_timer = new QTimer(this);
    m_timer->setInterval(2000);
    connect(m_timer, &QTimer::timeout, this, &PinManager::reenforce);

    // Opacity changes arrive in bursts while a slider is dragged. Rather than
    // rewriting pinned.json on every step, coalesce them: the actual write
    // happens 600 ms after the last change.
    m_persistTimer = new QTimer(this);
    m_persistTimer->setSingleShot(true);
    m_persistTimer->setInterval(600);
    connect(m_persistTimer, &QTimer::timeout, this, [this]() { persist(); });
}

void PinManager::schedulePersist()
{
    m_persistTimer->start();   // (re)start; a write fires once the burst settles
}

void PinManager::updateTimer()
{
    if (m_pinned.isEmpty())
        m_timer->stop();
    else if (!m_timer->isActive())
        m_timer->start();
}

PinnedWindow *PinManager::find(intptr_t hwnd)
{
    for (PinnedWindow &w : m_pinned) {
        if (w.hwnd == hwnd)
            return &w;
    }
    return nullptr;
}

const PinnedWindow *PinManager::find(intptr_t hwnd) const
{
    for (const PinnedWindow &w : m_pinned) {
        if (w.hwnd == hwnd)
            return &w;
    }
    return nullptr;
}

bool PinManager::isPinned(intptr_t hwnd) const
{
    return find(hwnd) != nullptr;
}

bool PinManager::pin(intptr_t hwnd, bool announce)
{
    if (isPinned(hwnd))
        return true;

    if (!winpin::isValidWindow(H(hwnd))) {
        emit errorOccurred(tr("That window no longer exists."));
        return false;
    }

    if (!winpin::isPinnable(H(hwnd))) {
        // The desktop, the taskbar, Start… or PinIt itself.
        emit errorOccurred(tr("That window can't be pinned."));
        return false;
    }

    const QString title = winpin::windowTitle(H(hwnd));
    const QString proc  = winpin::processName(H(hwnd));

    if (!winpin::applyTopmost(H(hwnd)) || !winpin::isTopmost(H(hwnd))) {
        // UIPI silently blocks SetWindowPos on elevated windows; verifying the
        // style actually took is how we detect that (same as the Rust port).
        qWarning("Pin failed for %s (likely elevated/UIPI)", qUtf8Printable(proc));
        emit errorOccurred(tr("Can't pin %1 — it may be running as administrator.")
                               .arg(proc));
        return false;
    }

    PinnedWindow w;
    w.hwnd = hwnd;
    w.title = title;
    w.processName = proc;
    w.opacity = 100;
    w.wasLayered = winpin::isLayered(H(hwnd));   // remember its original style
    m_pinned.push_back(w);

    persist();
    updateTimer();
    qInfo("Pinned %s (%s)", qUtf8Printable(title), qUtf8Printable(proc));
    if (announce)
        emit pinToggled(true, title, proc);
    emit pinsChanged();
    return true;
}

bool PinManager::unpin(intptr_t hwnd)
{
    QString title, proc;
    bool opacityChanged = false, wasLayered = false;
    if (const PinnedWindow *w = find(hwnd)) {
        title = w->title;
        proc  = w->processName;
        opacityChanged = w->opacityChanged;
        wasLayered = w->wasLayered;
    }

    if (winpin::isValidWindow(H(hwnd))) {
        // Only undo opacity if we actually changed it — otherwise we'd reset an
        // app that manages its own transparency. keepLayered preserves its style.
        if (opacityChanged)
            winpin::restoreOpacity(H(hwnd), wasLayered);
        winpin::removeTopmost(H(hwnd));
    }

    m_pinned.removeIf([hwnd](const PinnedWindow &w) { return w.hwnd == hwnd; });
    persist();
    updateTimer();
    emit pinToggled(false, title, proc);
    emit pinsChanged();
    return true;
}

bool PinManager::toggle(intptr_t hwnd)
{
    return isPinned(hwnd) ? unpin(hwnd) : pin(hwnd);
}

void PinManager::toggleForeground()
{
    void *fg = winpin::foregroundWindow();
    if (!fg) {
        emit errorOccurred(tr("No window to pin — click a window first."));
        return;
    }
    toggle(reinterpret_cast<intptr_t>(fg));
}

void PinManager::adjustForegroundOpacity(int deltaPercent)
{
    void *fg = winpin::foregroundWindow();
    if (!fg)
        return;
    const intptr_t hwnd = reinterpret_cast<intptr_t>(fg);
    const PinnedWindow *w = find(hwnd);
    if (!w)
        return;   // only adjust opacity of pinned windows
    setOpacity(hwnd, w->opacity + deltaPercent);
}

bool PinManager::setOpacity(intptr_t hwnd, int percent)
{
    PinnedWindow *w = find(hwnd);
    if (!w)
        return false;

    if (percent < winpin::kMinOpacity) percent = winpin::kMinOpacity;
    if (percent > winpin::kMaxOpacity) percent = winpin::kMaxOpacity;

    if (!winpin::setOpacityPercent(H(hwnd), percent))
        return false;

    w->opacity = percent;
    w->opacityChanged = true;   // remember so unpin/exit undoes it
    schedulePersist();   // debounced — slider drags fire this dozens of times
    emit opacityChanged(hwnd, percent);
    return true;
}

void PinManager::reenforce()
{
    const qsizetype before = m_pinned.size();
    m_pinned.removeIf([](const PinnedWindow &w) { return !winpin::isValidWindow(H(w.hwnd)); });

    for (const PinnedWindow &w : m_pinned) {
        if (!winpin::isTopmost(H(w.hwnd)))
            winpin::applyTopmost(H(w.hwnd));
    }

    if (m_pinned.size() != before) {
        persist();
        updateTimer();
        emit pinsChanged();
    }
}

void PinManager::restoreAllWindows()
{
    int restored = 0;
    for (const PinnedWindow &w : m_pinned) {
        if (winpin::isValidWindow(H(w.hwnd))) {
            if (w.opacityChanged)
                winpin::restoreOpacity(H(w.hwnd), w.wasLayered);
            winpin::removeTopmost(H(w.hwnd));
            ++restored;
        }
    }

    if (m_sessionEnding) {
        // Windows is logging off / shutting down / restarting. Leave the saved
        // pin list intact so the windows are re-pinned on the next login — the
        // behaviour the website and README advertise. (We still un-topmost the
        // live windows above, harmlessly, in case the session end is aborted.)
        persist();   // flush any debounced opacity change so it survives the reboot
        qInfo("Session ending: restored %d window(s), keeping pins for next login",
              restored);
        return;
    }

    // Manual quit: forget the pins so a manual relaunch starts clean. Drop them
    // from memory, stop the re-enforce timer, and clear pinned.json (settings
    // are preserved because persist() only rewrites the pin list). Closing to
    // the tray never reaches here — this runs only on a real quit (aboutToQuit).
    m_pinned.clear();
    persist();
    updateTimer();
    qInfo("Restored and cleared %d pinned window(s) on manual quit", restored);
}

void PinManager::persist() const
{
    // Cancel any debounced write — this immediate persist supersedes it.
    if (m_persistTimer)
        m_persistTimer->stop();

    QVector<persistence::SavedPin> pins;
    pins.reserve(m_pinned.size());
    for (const auto &w : m_pinned) {
        persistence::SavedPin sp;
        sp.processName = w.processName;
        sp.title       = w.title;
        sp.opacity     = winpin::percentToAlpha(w.opacity);
        pins.push_back(sp);
    }
    persistence::savePins(pins);
}

void PinManager::restoreSaved()
{
    const persistence::SavedState state = persistence::load();
    if (state.pins.isEmpty())
        return;

    const QVector<winpin::PinnableWindow> live = winpin::enumerateWindows();
    QSet<intptr_t> used;

    for (const persistence::SavedPin &saved : state.pins) {
        intptr_t match = 0;
        for (const auto &w : live) {
            if (!used.contains(w.hwnd) && pinmatch::matches(saved, w.processName, w.title)) {
                match = w.hwnd;
                break;
            }
        }

        if (match != 0 && pin(match, /*announce=*/false)) {
            used.insert(match);
            const int percent = winpin::alphaToPercent(saved.opacity);
            if (percent < 100)
                setOpacity(match, percent);
        }
    }
}
