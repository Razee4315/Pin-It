#include "pinrow.h"
#include "winpin.h"
#include "elidedlabel.h"

#include <QColor>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

namespace {

// Deterministic avatar colour for a process name (ported from the original
// PinIt frontend) so each pinned app gets a stable little badge. The shades
// are light enough for the dark initial to stay readable on every one.
QColor avatarColor(const QString &name)
{
    static const char *kColors[] = {
        "#ef9a9a", "#f48fb1", "#ce93d8", "#b39ddb", "#9fa8da",
        "#90caf9", "#81d4fa", "#80deea", "#80cbc4", "#a5d6a7",
        "#c5e1a5", "#ffe082", "#ffcc80", "#ffab91", "#bcaaa4",
    };
    constexpr int count = int(sizeof(kColors) / sizeof(kColors[0]));
    quint32 hash = 0;
    for (const QChar ch : name)
        hash = ch.unicode() + (hash << 5) - hash;   // wraps mod 2^32 (well-defined)
    return QColor(QString::fromLatin1(kColors[hash % count]));
}

// First letter of the process name (sans .exe) for the avatar badge.
QString avatarInitial(const QString &name)
{
    QString n = name;
    if (n.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive))
        n.chop(4);
    return n.isEmpty() ? QStringLiteral("?") : QString(n.at(0).toUpper());
}

// Coloured badge with the process initial. Purely decorative: the process
// name is spelled out right next to it.
QLabel *makeAvatar(const QString &processName)
{
    auto *avatar = new QLabel(avatarInitial(processName));
    avatar->setProperty("role", "avatar");
    avatar->setFixedSize(28, 28);
    avatar->setAlignment(Qt::AlignCenter);
    // Only the per-app colour is set here; the rest comes from the theme.
    avatar->setStyleSheet(
        QStringLiteral("background: %1;").arg(avatarColor(processName).name()));
    return avatar;
}

} // namespace

QString displayProcess(const QString &processName)
{
    return processName.isEmpty() ? QCoreApplication::translate("PinRow", "(unknown app)")
                                 : processName;
}

QString displayTitle(const QString &title)
{
    if (title.isEmpty())
        return QCoreApplication::translate("PinRow", "(untitled window)");

    const int slash = title.lastIndexOf(QLatin1Char('\\'));
    if (slash >= 0 && slash < title.size() - 1)
        return title.mid(slash + 1);
    return title;
}

PendingRow::PendingRow(const persistence::SavedPin &pin, QWidget *parent)
    : QFrame(parent)
{
    setProperty("role", "card");

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(10, 6, 8, 6);
    row->setSpacing(8);

    row->addWidget(makeAvatar(pin.processName));

    auto *info = new QVBoxLayout;
    info->setSpacing(0);
    auto *name = new ElidedLabel(displayTitle(pin.title));
    name->setProperty("role", "muted");
    name->setToolTip(pin.title);
    auto *proc = new ElidedLabel(displayProcess(pin.processName));
    proc->setProperty("role", "muted");
    info->addWidget(name);
    info->addWidget(proc);
    row->addLayout(info, 1);

    auto *waiting = new QLabel(tr("Waiting for window"));
    waiting->setProperty("role", "muted");
    waiting->setToolTip(tr("PinIt will pin this window again as soon as it is opened."));
    row->addWidget(waiting);

    auto *forgetBtn = new QPushButton(QString::fromUtf8("\xE2\x9C\x95"));   // ✕
    forgetBtn->setObjectName(QStringLiteral("unpin"));
    forgetBtn->setFixedSize(24, 24);
    forgetBtn->setToolTip(tr("Stop waiting for this window"));
    forgetBtn->setAccessibleName(tr("Stop waiting for %1").arg(displayTitle(pin.title)));
    forgetBtn->setCursor(Qt::PointingHandCursor);
    connect(forgetBtn, &QPushButton::clicked, this, &PendingRow::forgetRequested);
    row->addWidget(forgetBtn);
    m_forget = forgetBtn;
}

QList<QWidget *> PendingRow::focusChain() const
{
    return {m_forget};
}

PinRow::PinRow(const PinnedWindow &window, QWidget *parent)
    : QFrame(parent)
{
    setProperty("role", "card");

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(10, 6, 8, 6);
    row->setSpacing(8);

    row->addWidget(makeAvatar(window.processName));

    // Title + process name stacked tightly; takes the leftover width.
    auto *info = new QVBoxLayout;
    info->setSpacing(0);
    auto *name = new ElidedLabel(displayTitle(window.title));
    name->setStyleSheet(QStringLiteral("font-weight: 600;"));
    name->setToolTip(window.title);   // full title on hover
    m_title = name;
    auto *proc = new ElidedLabel(displayProcess(window.processName));
    proc->setProperty("role", "muted");
    info->addWidget(name);
    info->addWidget(proc);
    row->addLayout(info, 1);

    // Opacity slider + percentage.
    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setRange(winpin::kMinOpacity, winpin::kMaxOpacity);
    m_slider->setValue(window.opacity);
    m_slider->setFixedWidth(76);
    // The round handle is pulled out over the thin groove (margin:-6px in
    // the QSS); without enough vertical room it gets clipped at the top.
    m_slider->setMinimumHeight(20);
    row->addWidget(m_slider);

    m_percent = new QLabel(QStringLiteral("%1%").arg(window.opacity));
    m_percent->setProperty("role", "muted");
    m_percent->setMinimumWidth(30);
    m_percent->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(m_percent);

    connect(m_slider, &QSlider::valueChanged, this, [this](int v) {
        m_percent->setText(QStringLiteral("%1%").arg(v));
        emit opacityRequested(v);
    });

    // Compact unpin button (full label still available as a tooltip).
    auto *unpinBtn = new QPushButton(QString::fromUtf8("\xE2\x9C\x95"));   // ✕
    unpinBtn->setObjectName(QStringLiteral("unpin"));
    unpinBtn->setFixedSize(24, 24);
    unpinBtn->setToolTip(tr("Unpin this window"));
    unpinBtn->setCursor(Qt::PointingHandCursor);
    connect(unpinBtn, &QPushButton::clicked, this, &PinRow::unpinRequested);
    row->addWidget(unpinBtn);
    m_unpin = unpinBtn;

    nameControls(window.title);

    // Hovering a row outlines its window — but only once the pointer rests
    // there, so sweeping the mouse across the list doesn't flash every window.
    m_hoverTimer = new QTimer(this);
    m_hoverTimer->setSingleShot(true);
    m_hoverTimer->setInterval(350);
    connect(m_hoverTimer, &QTimer::timeout, this, &PinRow::locateRequested);
}

void PinRow::enterEvent(QEnterEvent *event)
{
    m_hoverTimer->start();
    QFrame::enterEvent(event);
}

void PinRow::leaveEvent(QEvent *event)
{
    m_hoverTimer->stop();
    QFrame::leaveEvent(event);
}

QList<QWidget *> PinRow::focusChain() const
{
    return {m_slider, m_unpin};
}

void PinRow::nameControls(const QString &title)
{
    const QString shown = displayTitle(title);
    m_slider->setAccessibleName(tr("Opacity of %1").arg(shown));
    m_unpin->setAccessibleName(tr("Unpin %1").arg(shown));
}

void PinRow::setTitle(const QString &title)
{
    static_cast<ElidedLabel *>(m_title)->setFullText(displayTitle(title));
    m_title->setToolTip(title);
    nameControls(title);
}

void PinRow::setOpacity(int percent)
{
    const QSignalBlocker blocker(m_slider);
    m_slider->setValue(percent);
    m_percent->setText(QStringLiteral("%1%").arg(percent));
}
