#include "pinmanager.h"
#include "winpin.h"
#include "persistence.h"

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
}

void PinManager::updateTimer()
{
    if (m_pinned.isEmpty())
        m_timer->stop();
    else if (!m_timer->isActive())
        m_timer->start();
}

bool PinManager::isPinned(intptr_t hwnd) const
{
    return m_pinned.contains(hwnd);
}

bool PinManager::pin(intptr_t hwnd)
{
    if (m_pinned.contains(hwnd))
        return true;

    if (!winpin::isValidWindow(H(hwnd))) {
        emit errorOccurred(tr("That window no longer exists."));
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
    m_pinned.insert(hwnd, w);

    persist();
    updateTimer();
    qInfo("Pinned %s (%s)", qUtf8Printable(title), qUtf8Printable(proc));
    emit pinToggled(true, title, proc);
    emit pinsChanged();
    return true;
}

bool PinManager::unpin(intptr_t hwnd)
{
    auto it = m_pinned.find(hwnd);
    QString title, proc;
    if (it != m_pinned.end()) {
        title = it->title;
        proc  = it->processName;
    }

    if (winpin::isValidWindow(H(hwnd))) {
        winpin::restoreOpacity(H(hwnd));
        winpin::removeTopmost(H(hwnd));
    }

    m_pinned.remove(hwnd);
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
    if (!m_pinned.contains(hwnd))
        return;   // only adjust opacity of pinned windows
    setOpacity(hwnd, m_pinned[hwnd].opacity + deltaPercent);
}

bool PinManager::setOpacity(intptr_t hwnd, int percent)
{
    auto it = m_pinned.find(hwnd);
    if (it == m_pinned.end())
        return false;

    if (percent < winpin::kMinOpacity) percent = winpin::kMinOpacity;
    if (percent > winpin::kMaxOpacity) percent = winpin::kMaxOpacity;

    if (!winpin::setOpacityPercent(H(hwnd), percent))
        return false;

    it->opacity = percent;
    persist();
    emit opacityChanged(hwnd, percent);
    return true;
}

QVector<PinnedWindow> PinManager::pinnedWindows() const
{
    QVector<PinnedWindow> out;
    out.reserve(m_pinned.size());
    for (const auto &w : m_pinned)
        out.push_back(w);
    return out;
}

void PinManager::reenforce()
{
    QVector<intptr_t> stale;
    for (auto it = m_pinned.begin(); it != m_pinned.end(); ++it) {
        if (!winpin::isValidWindow(H(it.key()))) {
            stale.push_back(it.key());
            continue;
        }
        if (!winpin::isTopmost(H(it.key())))
            winpin::applyTopmost(H(it.key()));
    }

    if (!stale.isEmpty()) {
        for (intptr_t h : stale)
            m_pinned.remove(h);
        persist();
        updateTimer();
        emit pinsChanged();
    }
}

void PinManager::restoreAllWindows()
{
    int restored = 0;
    for (auto it = m_pinned.begin(); it != m_pinned.end(); ++it) {
        if (winpin::isValidWindow(H(it.key()))) {
            winpin::restoreOpacity(H(it.key()));
            winpin::removeTopmost(H(it.key()));
            ++restored;
        }
    }
    // Intentionally keep m_pinned / pinned.json intact so the next launch
    // re-pins these windows.
    qInfo("Restored %d window(s) on exit", restored);
}

void PinManager::persist() const
{
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
        // Prefer an exact process+title match, else first unused window of
        // the same process — mirrors the Rust restore() heuristic.
        intptr_t match = 0;
        for (const auto &w : live) {
            if (w.processName != saved.processName || used.contains(w.hwnd))
                continue;
            if (!saved.title.isEmpty() && w.title == saved.title) {
                match = w.hwnd;
                break;
            }
            if (match == 0)
                match = w.hwnd;   // fallback candidate, keep scanning for exact
        }

        if (match != 0 && pin(match)) {
            used.insert(match);
            const int percent = winpin::alphaToPercent(saved.opacity);
            if (percent < 100)
                setOpacity(match, percent);
        }
    }
}
