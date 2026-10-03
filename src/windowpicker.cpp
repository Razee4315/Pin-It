#include "windowpicker.h"
#include "pinrow.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include <windows.h>

namespace {

// The icon the window shows in its title bar / the taskbar, if it has one.
QIcon iconOfWindow(intptr_t hwnd)
{
    const HWND h = reinterpret_cast<HWND>(hwnd);

    // Ask the window first (with a short timeout, and never waiting on a hung
    // app), then fall back to the icon registered for its window class.
    constexpr UINT kTimeoutMs = 50;
    DWORD_PTR icon = 0;
    for (const WPARAM which : {WPARAM(ICON_SMALL2), WPARAM(ICON_SMALL), WPARAM(ICON_BIG)}) {
        if (SendMessageTimeoutW(h, WM_GETICON, which, 0, SMTO_ABORTIFHUNG, kTimeoutMs, &icon)
            && icon != 0)
            break;
        icon = 0;
    }
    if (icon == 0)
        icon = GetClassLongPtrW(h, GCLP_HICONSM);
    if (icon == 0)
        icon = GetClassLongPtrW(h, GCLP_HICON);
    if (icon == 0)
        return QIcon();

    return QIcon(QPixmap::fromImage(QImage::fromHICON(reinterpret_cast<HICON>(icon))));
}

} // namespace

WindowPicker::WindowPicker(const QVector<winpin::PinnableWindow> &windows, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Pin a window"));
    resize(400, 440);

    auto *layout = new QVBoxLayout(this);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search open windows…"));
    m_search->setAccessibleName(tr("Search open windows"));
    m_search->setClearButtonEnabled(true);
    m_search->installEventFilter(this);
    layout->addWidget(m_search);

    m_list = new QListWidget(this);
    m_list->setAccessibleName(tr("Open windows"));
    m_list->setIconSize(QSize(20, 20));
    for (const winpin::PinnableWindow &w : windows) {
        auto *item = new QListWidgetItem(
            iconOfWindow(w.hwnd),
            QStringLiteral("%1   —   %2").arg(displayTitle(w.title), displayProcess(w.processName)),
            m_list);
        item->setToolTip(w.title);
        item->setData(Qt::UserRole, QVariant::fromValue<qlonglong>(w.hwnd));
    }
    layout->addWidget(m_list, 1);

    // Shown in place of the list when there is nothing to choose from.
    m_emptyLabel = new QLabel(this);
    m_emptyLabel->setProperty("role", "muted");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    layout->addWidget(m_emptyLabel, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_pinButton = buttons->addButton(tr("Pin"), QDialogButtonBox::AcceptRole);
    m_pinButton->setDefault(true);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
    connect(m_list, &QListWidget::currentRowChanged, this, &WindowPicker::updateState);
    connect(m_search, &QLineEdit::textChanged, this, &WindowPicker::applyFilter);

    applyFilter(QString());
    m_search->setFocus();
}

intptr_t WindowPicker::selectedWindow() const
{
    if (result() != QDialog::Accepted)
        return 0;
    const QListWidgetItem *item = m_list->currentItem();
    if (!item || item->isHidden())
        return 0;
    return static_cast<intptr_t>(item->data(Qt::UserRole).toLongLong());
}

void WindowPicker::applyFilter(const QString &text)
{
    const QString needle = text.trimmed();
    QListWidgetItem *firstVisible = nullptr;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        const bool match = needle.isEmpty()
            || item->text().contains(needle, Qt::CaseInsensitive)
            || item->toolTip().contains(needle, Qt::CaseInsensitive);
        item->setHidden(!match);
        if (match && !firstVisible)
            firstVisible = item;
    }

    // Keep a visible row selected so Enter always pins what the user sees.
    const QListWidgetItem *current = m_list->currentItem();
    if (!current || current->isHidden())
        m_list->setCurrentItem(firstVisible);

    const bool anyVisible = firstVisible != nullptr;
    m_list->setVisible(anyVisible);
    m_emptyLabel->setVisible(!anyVisible);
    if (!anyVisible) {
        m_emptyLabel->setText(m_list->count() == 0
            ? tr("There are no other windows open to pin.")
            : tr("No open window matches “%1”.").arg(needle));
    }
    updateState();
}

void WindowPicker::updateState()
{
    const QListWidgetItem *current = m_list->currentItem();
    m_pinButton->setEnabled(current && !current->isHidden());
}

bool WindowPicker::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_search && event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Up || key == Qt::Key_Down || key == Qt::Key_PageUp
            || key == Qt::Key_PageDown) {
            QCoreApplication::sendEvent(m_list, event);
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}
