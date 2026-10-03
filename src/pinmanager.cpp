#include "pinmanager.h"
#include "winpin.h"
#include "persistence.h"
#include "pinmatch.h"

#include <QTimer>
#include <QtGlobal>

#include <utility>

PinManager::PinManager(QObject *parent)
    : QObject(parent)
{
    // Windows 11's compositor occasionally strips the topmost flag. We put it
    // back — and sweep out windows that have since closed — whenever another
    // window comes to the front (the moment it matters), with a slow timer as
    // a safety net. Both only run while at least one window is pinned (see
    // updateTimer) so an idle PinIt uses zero CPU.
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

PinManager::~PinManager()
{
    winpin::stopWatchingForeground();
}

void PinManager::schedulePersist()
{
    m_persistTimer->start();   // (re)start; a write fires once the burst settles
}

void PinManager::updateTimer()
{
    if (m_pinned.isEmpty() && m_pending.isEmpty()) {
        m_timer->stop();
        winpin::stopWatchingForeground();
    } else if (!m_timer->isActive()) {
        m_timer->start();
        winpin::watchForeground(
            [](void *self) { static_cast<PinManager *>(self)->reenforce(); }, this);
    }
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

    if (!winpin::isValidWindow(hwnd)) {
        if (announce)
            emit errorOccurred(tr("That window no longer exists."));
        return false;
    }

    if (!winpin::isPinnable(hwnd)) {
        // The desktop, the taskbar, Start… or PinIt itself.
        if (announce)
            emit errorOccurred(tr("That window can't be pinned."));
        return false;
    }

    const QString title = winpin::windowTitle(hwnd);
    const QString proc  = winpin::processName(hwnd);
    // Some apps keep themselves on top (Task Manager's "Always on top", media
    // players). Remember that so unpinning doesn't take it away from them.
    const bool wasTopmost = winpin::isTopmost(hwnd);

    if (!winpin::applyTopmost(hwnd) || !winpin::isTopmost(hwnd)) {
        // UIPI silently blocks SetWindowPos on elevated windows; verifying the
        // style actually took is how we detect that (same as the Rust port).
        if (announce) {
            qWarning("Pin failed for %s (likely elevated/UIPI)", qUtf8Printable(proc));
            emit errorOccurred(tr("Can't pin %1 — it may be running as administrator.")
                                   .arg(proc.isEmpty() ? tr("this window") : proc));
        }
        return false;
    }

    PinnedWindow w;
    w.hwnd = hwnd;
    w.title = title;
    w.processName = proc;
    w.opacity = 100;
    w.wasLayered = winpin::isLayered(hwnd);   // remember its original style
    w.wasTopmost = wasTopmost;
    w.wasClickThrough = winpin::isClickThrough(hwnd);
    w.clickThrough = w.wasClickThrough;
    m_pinned.push_back(w);

    // This window is no longer being waited for (whether a pending pin was
    // just applied to it or the user pinned it by hand first).
    for (qsizetype i = 0; i < m_pending.size(); ++i) {
        if (pinmatch::matches(m_pending[i], proc, title)) {
            m_pending.removeAt(i);
            break;
        }
    }

    persist();
    updateTimer();
    // Process name only — window titles can hold document names, URLs or
    // message subjects, and users are asked to attach this log to bug reports.
    qInfo("Pinned a window of %s", qUtf8Printable(proc));
    if (announce)
        emit pinToggled(hwnd, true, title);
    emit pinsChanged();
    return true;
}

bool PinManager::release(const PinnedWindow &window)
{
    if (!winpin::isValidWindow(window.hwnd))
        return false;

    // Only undo opacity if we actually changed it — otherwise we'd reset an
    // app that manages its own transparency. keepLayered preserves its style.
    if (window.clickThrough && !window.wasClickThrough)
        winpin::setClickThrough(window.hwnd, false);
    if (window.opacityChanged)
        winpin::restoreOpacity(window.hwnd, window.wasLayered);
    if (!window.wasTopmost)
        winpin::removeTopmost(window.hwnd);
    return true;
}

bool PinManager::unpin(intptr_t hwnd)
{
    const PinnedWindow *found = find(hwnd);
    if (!found)
        return false;

    const PinnedWindow window = *found;   // copy: the entry is removed below
    release(window);

    m_pinned.removeIf([hwnd](const PinnedWindow &w) { return w.hwnd == hwnd; });
    persist();
    updateTimer();
    emit pinToggled(hwnd, false, window.title);
    emit pinsChanged();
    return true;
}

int PinManager::unpinAll()
{
    const int count = m_pinned.size();
    if (count == 0 && m_pending.isEmpty())
        return 0;

    for (const PinnedWindow &w : std::as_const(m_pinned))
        release(w);
    m_pinned.clear();
    m_pending.clear();
    persist();
    updateTimer();
    emit pinsChanged();
    return count;
}

bool PinManager::toggle(intptr_t hwnd)
{
    return isPinned(hwnd) ? unpin(hwnd) : pin(hwnd);
}

void PinManager::toggleForeground()
{
    const intptr_t fg = winpin::foregroundWindow();
    if (!fg) {
        emit errorOccurred(tr("No window to pin — click a window first."));
        return;
    }
    toggle(fg);
}

void PinManager::adjustForegroundOpacity(int deltaPercent)
{
    const intptr_t hwnd = winpin::foregroundWindow();
    if (!hwnd)
        return;
    const PinnedWindow *w = find(hwnd);
    if (!w) {
        // Only pinned windows can be faded; say so instead of doing nothing.
        emit errorOccurred(tr("Pin this window first to change its opacity."));
        return;
    }
    setOpacity(hwnd, w->opacity + deltaPercent);
}

bool PinManager::setOpacity(intptr_t hwnd, int percent)
{
    PinnedWindow *w = find(hwnd);
    if (!w)
        return false;

    if (percent < winpin::kMinOpacity) percent = winpin::kMinOpacity;
    if (percent > winpin::kMaxOpacity) percent = winpin::kMaxOpacity;

    if (!winpin::setOpacityPercent(hwnd, percent))
        return false;

    w->opacity = percent;
    w->opacityChanged = true;   // remember so unpin/exit undoes it
    schedulePersist();   // debounced — slider drags fire this dozens of times
    emit opacityChanged(hwnd, percent);
    return true;
}

bool PinManager::setClickThrough(intptr_t hwnd, bool enabled)
{
    PinnedWindow *w = find(hwnd);
    if (!w)
        return false;

    // Windows only honours click-through on layered windows, and a layered
    // window needs an alpha set to be drawn at all — which is exactly what
    // setting the opacity does.
    if (enabled && !winpin::isLayered(hwnd) && !setOpacity(hwnd, w->opacity))
        return false;
    if (!winpin::setClickThrough(hwnd, enabled))
        return false;

    w->clickThrough = enabled;
    schedulePersist();
    emit clickThroughChanged(hwnd, enabled);
    return true;
}

void PinManager::reenforce()
{
    // Windows is shutting down and closing the other apps. Dropping their
    // pins now, as if the user had closed those windows, would lose exactly
    // what must survive the restart.
    if (m_sessionEnding)
        return;

    const qsizetype before = m_pinned.size();
    m_pinned.removeIf([](const PinnedWindow &w) { return !winpin::isValidWindow(w.hwnd); });

    for (PinnedWindow &w : m_pinned) {
        if (!winpin::isTopmost(w.hwnd))
            winpin::applyTopmost(w.hwnd);

        // Browsers and editors retitle their window all the time. Keep the
        // list — and the title saved for the next restore — current.
        const QString title = winpin::windowTitle(w.hwnd);
        if (title != w.title && !title.isEmpty()) {
            w.title = title;
            schedulePersist();
            emit titleChanged(w.hwnd, title);
        }
    }

    if (m_pinned.size() != before) {
        persist();
        updateTimer();
        emit pinsChanged();
    }

    restorePending();
}

void PinManager::restorePending()
{
    if (m_pending.isEmpty())
        return;

    // Only the window in front is checked: a window that has just opened is
    // the foreground window, and looking at one window costs next to nothing
    // (no enumeration of every window on each tick).
    const intptr_t hwnd = winpin::foregroundWindow();
    if (!hwnd || isPinned(hwnd) || !winpin::isPinnable(hwnd))
        return;

    const QString title = winpin::windowTitle(hwnd);
    const QString proc = winpin::processName(hwnd);
    for (const persistence::SavedPin &pending : std::as_const(m_pending)) {
        if (!pinmatch::matches(pending, proc, title))
            continue;
        const persistence::SavedPin saved = pending;   // pin() removes the entry
        if (applySaved(hwnd, saved))
            emit pinRestored(hwnd);
        return;
    }
}

bool PinManager::applySaved(intptr_t hwnd, const persistence::SavedPin &saved)
{
    if (!pin(hwnd, /*announce=*/false))
        return false;

    // After a crash the window is still topmost / layered / click-through from
    // our previous run, which pin() has just recorded as the app's own doing.
    // The saved flags know better. (After a normal restart both agree.)
    if (PinnedWindow *w = find(hwnd)) {
        w->wasLayered = w->wasLayered && saved.wasLayered;
        w->wasTopmost = w->wasTopmost && saved.wasTopmost;
        w->wasClickThrough = w->wasClickThrough && saved.wasClickThrough;
    }
    const int percent = winpin::alphaToPercent(saved.opacity);
    if (percent < 100)
        setOpacity(hwnd, percent);
    if (saved.clickThrough)
        setClickThrough(hwnd, true);
    return true;
}

void PinManager::forgetPending(int index)
{
    if (index < 0 || index >= m_pending.size())
        return;
    m_pending.removeAt(index);
    persist();
    updateTimer();
    emit pinsChanged();
}

void PinManager::restoreAllWindows()
{
    int restored = 0;
    for (const PinnedWindow &w : m_pinned) {
        if (release(w))
            ++restored;
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
    m_pending.clear();
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
    pins.reserve(m_pinned.size() + m_pending.size());
    for (const auto &w : m_pinned) {
        persistence::SavedPin sp;
        sp.processName = w.processName;
        sp.title       = w.title;
        sp.opacity     = winpin::percentToAlpha(w.opacity);
        sp.clickThrough = w.clickThrough;
        sp.wasLayered  = w.wasLayered;
        sp.wasTopmost  = w.wasTopmost;
        sp.wasClickThrough = w.wasClickThrough;
        pins.push_back(sp);
    }
    // Pins still waiting for their window stay saved until the user quits or
    // forgets them — dropping them here is what used to erase every pin whose
    // app wasn't open yet at login.
    pins += m_pending;
    persistence::savePins(pins);
}

void PinManager::restoreSaved()
{
    const persistence::SavedState state = persistence::load();
    if (state.pins.isEmpty())
        return;

    // Everything starts out pending; pin() takes an entry off that list when
    // its window is pinned. What is left waits for its window to appear.
    m_pending = state.pins;

    const QVector<winpin::PinnableWindow> live = winpin::enumerateWindows();
    for (const persistence::SavedPin &saved : state.pins) {
        for (const auto &w : live) {
            if (!isPinned(w.hwnd) && pinmatch::matches(saved, w.processName, w.title)) {
                applySaved(w.hwnd, saved);
                break;
            }
        }
    }

    updateTimer();
    emit pinsChanged();
}
