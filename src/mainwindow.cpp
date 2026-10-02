#include "mainwindow.h"
#include "pinmanager.h"
#include "winpin.h"
#include "shortcuts.h"
#include "shortcutsdialog.h"
#include "pinrow.h"
#include "pinflash.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QScrollArea>
#include <QFrame>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QDialog>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QCloseEvent>
#include <QPixmap>
#include <QIcon>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QMessageBox>
#include <QSet>
#include <QTimer>
#include <QAccessible>

#include "version.h"

namespace {

QIcon appIcon()
{
    QIcon ic(QStringLiteral(":/icon.png"));
    return ic.isNull() ? QIcon(QStringLiteral(":/icon-128.png")) : ic;
}

// A single keyboard-key chip, e.g. [ Win ].
QLabel *keyChip(const QString &text)
{
    auto *l = new QLabel(text);
    l->setProperty("role", "key");
    l->setAlignment(Qt::AlignCenter);
    return l;
}

QLabel *plusLabel(const QString &text = QStringLiteral("+"))
{
    auto *l = new QLabel(text);
    l->setProperty("role", "plus");
    return l;
}

QFrame *makeCard()
{
    auto *card = new QFrame;
    card->setProperty("role", "card");
    return card;
}

} // namespace

MainWindow::MainWindow(PinManager *manager, QWidget *parent)
    : QMainWindow(parent)
    , m_manager(manager)
{
    setWindowTitle(QStringLiteral("PinIt"));
    setWindowIcon(appIcon());

    // Fixed-size window: drop the maximize button and lock the dimensions.
    setWindowFlags(Qt::Window | Qt::MSWindowsFixedSizeDialogHint
                   | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
                   | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
    setFixedSize(360, 470);

    m_settings = persistence::loadSettings();

    buildUi();
    buildTray();
    syncList();

    connect(m_manager, &PinManager::pinsChanged, this, &MainWindow::syncList);
    connect(m_manager, &PinManager::errorOccurred, this, &MainWindow::notify);
    connect(m_manager, &PinManager::titleChanged, this,
            [this](intptr_t hwnd, const QString &title) {
                if (PinRow *row = m_rows.value(hwnd))
                    row->setTitle(title);
            });
    connect(m_manager, &PinManager::opacityChanged, this, [this](intptr_t hwnd, int percent) {
        if (PinRow *row = m_rows.value(hwnd))
            row->setOpacity(percent);
    });
    connect(m_manager, &PinManager::pinToggled, this,
            [this](intptr_t hwnd, bool pinned, const QString &title) {
                if (pinned && m_settings.enableSound)
                    winpin::beep();
                // The outline around the window itself is the primary feedback:
                // instant, and exactly where the user is looking.
                pinflash::show(hwnd, pinned ? pinflash::Kind::Pinned
                                            : pinflash::Kind::Unpinned);
                notify(pinned ? tr("Pinned: %1").arg(title)
                              : tr("Unpinned: %1").arg(title));
            });
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("central"));
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(9);

    // --- Header: logo + name -------------------------------------------------
    auto *header = new QHBoxLayout;
    header->setSpacing(9);
    auto *logo = new QLabel;
    logo->setPixmap(appIcon().pixmap(24, 24));
    header->addWidget(logo);

    auto *titleBox = new QVBoxLayout;
    titleBox->setSpacing(0);
    auto *title = new QLabel(QStringLiteral("PinIt"));
    title->setProperty("role", "title");
    auto *tagline = new QLabel(tr("Keep any window always on top"));
    tagline->setProperty("role", "muted");
    titleBox->addWidget(title);
    titleBox->addWidget(tagline);
    header->addLayout(titleBox);
    header->addStretch();
    root->addLayout(header);

    // --- Pin button ----------------------------------------------------------
    auto *addBtn = new QPushButton(tr("+   Pin a window…"));
    addBtn->setObjectName(QStringLiteral("primary"));
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addWindowDialog);
    root->addWidget(addBtn);

    // --- SHORTCUTS -----------------------------------------------------------
    auto *scLabel = new QLabel(tr("SHORTCUTS"));
    scLabel->setProperty("role", "section");
    root->addWidget(scLabel);

    auto *scCard = makeCard();
    auto *scv = new QVBoxLayout(scCard);
    scv->setContentsMargins(12, 10, 12, 10);
    scv->setSpacing(9);
    m_shortcutsLayout = scv;
    fillShortcutRows(scv);
    root->addWidget(scCard);

    m_hotkeyWarning = new QLabel;
    m_hotkeyWarning->setProperty("role", "warning");
    m_hotkeyWarning->setWordWrap(true);
    m_hotkeyWarning->hide();
    root->addWidget(m_hotkeyWarning);

    auto *editShortcuts = new QPushButton(tr("Edit shortcuts…"));
    connect(editShortcuts, &QPushButton::clicked, this, &MainWindow::openShortcutsDialog);
    root->addWidget(editShortcuts, 0, Qt::AlignLeft);

    // --- PINNED (n) ----------------------------------------------------------
    m_pinnedHeader = new QLabel(tr("PINNED (0)"));
    m_pinnedHeader->setProperty("role", "section");
    root->addWidget(m_pinnedHeader);

    auto *scroll = new QScrollArea(central);
    m_scroll = scroll;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *listContainer = new QWidget(scroll);
    m_listLayout = new QVBoxLayout(listContainer);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(8);
    m_listLayout->addStretch();
    scroll->setWidget(listContainer);
    root->addWidget(scroll, 1);

    // Empty-state card (shown when nothing is pinned).
    m_emptyCard = makeCard();
    auto *ec = new QVBoxLayout(m_emptyCard);
    ec->setContentsMargins(14, 16, 14, 16);
    ec->setSpacing(8);
    auto *emptyText = new QLabel(tr("No windows pinned"));
    emptyText->setProperty("role", "muted");
    emptyText->setAlignment(Qt::AlignCenter);
    ec->addWidget(emptyText);
    auto *hintRow = new QHBoxLayout;
    hintRow->addStretch();
    auto *use = new QLabel(tr("Use"));
    use->setProperty("role", "muted");
    hintRow->addWidget(use);
    const QStringList toggleKeys = shortcuts::displayTokens(m_settings.shortcuts.togglePin);
    for (int i = 0; i < toggleKeys.size(); ++i) {
        if (i > 0)
            hintRow->addWidget(plusLabel());
        hintRow->addWidget(keyChip(toggleKeys[i]));
    }
    hintRow->addStretch();
    ec->addLayout(hintRow);
    m_listLayout->insertWidget(0, m_emptyCard);   // lives in the list region

    // --- Settings (compact, at the bottom) -----------------------------------
    m_soundBox = new QCheckBox(tr("Play a sound when pinning"));
    m_soundBox->setChecked(m_settings.enableSound);
    connect(m_soundBox, &QCheckBox::toggled, this, [this](bool on) {
        m_settings.enableSound = on;
        persistence::saveSettings(m_settings);
    });
    root->addWidget(m_soundBox);

    m_autostartBox = new QCheckBox(tr("Start PinIt with Windows"));
    m_autostartBox->setChecked(m_settings.startWithWindows);
    connect(m_autostartBox, &QCheckBox::toggled, this, [this](bool on) {
        m_settings.startWithWindows = on;
        applyAutostart(on);
        persistence::saveSettings(m_settings);
    });
    root->addWidget(m_autostartBox);

    // In-window message. Not in a layout: it floats over the bottom of the
    // list so showing it never moves anything.
    m_status = new QLabel(central);
    m_status->setProperty("role", "status");
    m_status->setWordWrap(true);
    m_status->setAlignment(Qt::AlignCenter);
    m_status->hide();
    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(true);
    m_statusTimer->setInterval(4000);
    connect(m_statusTimer, &QTimer::timeout, m_status, &QWidget::hide);

    setCentralWidget(central);
}

void MainWindow::setHotkeyProblems(const QStringList &failedActions)
{
    m_hotkeyProblems = failedActions;
    m_hotkeyWarning->setVisible(!failedActions.isEmpty());
    if (!failedActions.isEmpty()) {
        m_hotkeyWarning->setText(
            tr("Not working: %1. Another app is probably using the same keys — "
               "pick different ones with “Edit shortcuts…”.")
                .arg(failedActions.join(QStringLiteral(", "))));
    }
    updateTrayToolTip();
}

void MainWindow::updateTrayToolTip()
{
    if (!m_tray)
        return;
    const int n = m_manager->pinnedCount();
    QString tip = n == 0 ? tr("PinIt — no windows pinned")
                         : tr("PinIt — %n window(s) pinned", "", n);
    if (!m_hotkeyProblems.isEmpty())
        tip += QLatin1Char('\n') + tr("Shortcut not working: %1")
                                       .arg(m_hotkeyProblems.join(QStringLiteral(", ")));
    m_tray->setToolTip(tip);
}

void MainWindow::showStatus(const QString &message)
{
    m_status->setText(message);
    placeStatus();
    m_status->show();
    m_status->raise();
    m_statusTimer->start();

    // Screen readers don't notice a label appearing; announce it.
    QAccessibleEvent alert(m_status, QAccessible::Alert);
    QAccessible::updateAccessibility(&alert);
}

void MainWindow::placeStatus()
{
    if (!m_status || !m_scroll)
        return;
    constexpr int kInset = 6;
    const QRect area = m_scroll->geometry();
    const int width = area.width() - 2 * kInset;
    const int height = m_status->heightForWidth(width);
    m_status->setGeometry(area.left() + kInset, area.bottom() - kInset - height + 1,
                          width, height);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    placeStatus();
}

void MainWindow::setShortcutConfig(const persistence::ShortcutConfig &cfg)
{
    m_settings.shortcuts = cfg;
    if (m_shortcutsLayout)
        fillShortcutRows(m_shortcutsLayout);
}

void MainWindow::fillShortcutRows(QVBoxLayout *scv)
{
    // Clear any existing rows (each row is a nested QHBoxLayout of chips).
    while (QLayoutItem *item = scv->takeAt(0)) {
        if (QLayout *child = item->layout()) {
            while (QLayoutItem *ci = child->takeAt(0)) {
                if (ci->widget())
                    ci->widget()->deleteLater();
                delete ci;
            }
        }
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    const persistence::ShortcutConfig &sc = m_settings.shortcuts;

    auto addRow = [&](const QStringList &keys, const QString &desc) {
        auto *row = new QHBoxLayout;
        row->setSpacing(6);
        for (int i = 0; i < keys.size(); ++i) {
            if (i > 0)
                row->addWidget(plusLabel());
            row->addWidget(keyChip(keys[i]));
        }
        row->addStretch();
        auto *d = new QLabel(desc);
        d->setProperty("role", "desc");
        row->addWidget(d);
        scv->addLayout(row);
    };

    addRow(shortcuts::displayTokens(sc.togglePin), tr("Pin / unpin window"));

    {   // Opacity row shows both +/- keys sharing the same modifiers.
        const QStringList up = shortcuts::displayTokens(sc.opacityUp);
        const QStringList down = shortcuts::displayTokens(sc.opacityDown);
        auto *row = new QHBoxLayout;
        row->setSpacing(6);
        for (int i = 0; i < up.size(); ++i) {
            const bool isKey = (i == up.size() - 1);
            if (i > 0)
                row->addWidget(plusLabel());
            if (isKey) {
                row->addWidget(keyChip(up[i]));
                row->addWidget(plusLabel(QStringLiteral("/")));
                row->addWidget(keyChip(down.isEmpty() ? QStringLiteral("-") : down.last()));
            } else {
                row->addWidget(keyChip(up[i]));
            }
        }
        row->addStretch();
        auto *d = new QLabel(tr("Adjust opacity"));
        d->setProperty("role", "desc");
        row->addWidget(d);
        scv->addLayout(row);
    }

    addRow(shortcuts::displayTokens(sc.toggleWindow), tr("Show / hide PinIt"));
}

void MainWindow::openShortcutsDialog()
{
    ShortcutsDialog dlg(m_settings.shortcuts, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    m_settings.shortcuts = dlg.config();
    persistence::saveSettings(m_settings);
    if (m_shortcutsLayout)
        fillShortcutRows(m_shortcutsLayout);
    emit shortcutsChanged(m_settings.shortcuts);
}

void MainWindow::syncList()
{
    const QVector<PinnedWindow> pinned = m_manager->pinnedWindows();

    // Drop the rows of windows that are no longer pinned.
    QSet<intptr_t> live;
    for (const PinnedWindow &w : pinned)
        live.insert(w.hwnd);
    for (auto it = m_rows.begin(); it != m_rows.end();) {
        if (live.contains(it.key())) {
            ++it;
        } else {
            m_listLayout->removeWidget(it.value());
            it.value()->hide();
            it.value()->deleteLater();
            it = m_rows.erase(it);
        }
    }

    // Add rows for new pins. Existing rows are left alone, so a slider that is
    // being dragged (or has keyboard focus) is not torn down under the user.
    for (const PinnedWindow &w : pinned) {
        if (m_rows.contains(w.hwnd))
            continue;
        const intptr_t hwnd = w.hwnd;
        auto *row = new PinRow(w);
        connect(row, &PinRow::opacityRequested, this,
                [this, hwnd](int percent) { m_manager->setOpacity(hwnd, percent); });
        connect(row, &PinRow::unpinRequested, this,
                [this, hwnd]() { m_manager->unpin(hwnd); });
        m_rows.insert(hwnd, row);
        m_listLayout->insertWidget(m_listLayout->count() - 1, row);   // before the stretch
    }

    if (m_emptyCard)
        m_emptyCard->setVisible(pinned.isEmpty());
    if (m_pinnedHeader)
        m_pinnedHeader->setText(tr("PINNED (%1)").arg(pinned.size()));

    updateTrayToolTip();
}

void MainWindow::addWindowDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Pin a window"));
    dlg.setWindowIcon(appIcon());
    dlg.resize(400, 440);
    auto *l = new QVBoxLayout(&dlg);
    auto *prompt = new QLabel(tr("Choose a window to keep on top:"), &dlg);
    l->addWidget(prompt);

    auto *list = new QListWidget(&dlg);
    const QString self = windowTitle();
    for (const winpin::PinnableWindow &w : winpin::enumerateWindows()) {
        if (w.title.isEmpty() || w.title == QStringLiteral("Unknown"))
            continue;
        if (w.title == self)
            continue;
        if (m_manager->isPinned(w.hwnd))
            continue;
        auto *item = new QListWidgetItem(
            QStringLiteral("%1   —   %2").arg(displayTitle(w.title), w.processName), list);
        item->setToolTip(w.title);
        item->setData(Qt::UserRole, QVariant::fromValue<qlonglong>(w.hwnd));
    }
    l->addWidget(list, 1);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    l->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, &dlg, &QDialog::accept);

    if (dlg.exec() == QDialog::Accepted) {
        if (QListWidgetItem *sel = list->currentItem()) {
            const intptr_t hwnd =
                static_cast<intptr_t>(sel->data(Qt::UserRole).toLongLong());
            m_manager->pin(hwnd);
        }
    }
}

void MainWindow::showAbout()
{
    QMessageBox box(this);
    box.setWindowTitle(tr("About PinIt"));
    box.setIconPixmap(appIcon().pixmap(64, 64));
    box.setTextFormat(Qt::RichText);
    box.setTextInteractionFlags(Qt::TextBrowserInteraction);   // clickable links
    box.setText(QStringLiteral(
        "<h3>%1 %2</h3>"
        "<p>%3</p>"
        "<p>Built with C++ &amp; Qt %4.</p>"
        "<p>By %5<br><a href=\"%6\">%6</a></p>"
        "<p style='color:gray'>%7</p>")
        .arg(QStringLiteral(PINIT_PRODUCT),
             QStringLiteral(PINIT_VERSION_STR),
             tr("Keep any window always on top — with a global hotkey."),
             QStringLiteral(QT_VERSION_STR),
             QStringLiteral(PINIT_COMPANY),
             QStringLiteral(PINIT_URL),
             QStringLiteral(PINIT_COPYRIGHT)));
    box.exec();
}

void MainWindow::buildTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_tray = new QSystemTrayIcon(appIcon(), this);

    auto *menu = new QMenu(this);
    QAction *showAct = menu->addAction(tr("Show PinIt"));
    connect(showAct, &QAction::triggered, this, &MainWindow::showFromTray);
    QAction *aboutAct = menu->addAction(tr("About PinIt"));
    connect(aboutAct, &QAction::triggered, this, &MainWindow::showAbout);
    menu->addSeparator();
    QAction *quitAct = menu->addAction(tr("Quit"));
    connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);

    m_tray->setContextMenu(menu);
    m_tray->setToolTip(QStringLiteral("PinIt"));
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                // Trigger only: a double-click also delivers a Trigger first,
                // so reacting to both opened the window and hid it again.
                if (reason == QSystemTrayIcon::Trigger)
                    toggleVisibility();
            });
    m_tray->show();
}

void MainWindow::applyAutostart(bool enabled)
{
    QSettings run(QStringLiteral(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        QSettings::NativeFormat);
    if (enabled) {
        const QString exe = QDir::toNativeSeparators(
            QCoreApplication::applicationFilePath());
        // --minimized: when launched at login, start silently in the tray
        // instead of popping the window every boot.
        run.setValue(QStringLiteral("PinIt"),
                     QStringLiteral("\"%1\" --minimized").arg(exe));
    } else {
        run.remove(QStringLiteral("PinIt"));
    }
}

void MainWindow::toggleVisibility()
{
    // Hide only when the user is actually looking at the window. If it is open
    // but buried under other windows, bring it forward instead.
    //
    // Clicking the tray icon moves focus to the taskbar just before this runs,
    // so "was active a moment ago" has to count as active too.
    constexpr qint64 kJustDeactivatedMs = 400;
    const bool inFront = isActiveWindow()
        || (m_sinceDeactivated.isValid() && m_sinceDeactivated.elapsed() < kJustDeactivatedMs);

    if (isVisible() && !isMinimized() && inFront)
        hide();
    else
        showFromTray();
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange && !isActiveWindow())
        m_sinceDeactivated.start();
    QMainWindow::changeEvent(event);
}

void MainWindow::showFromTray()
{
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::notify(const QString &message)
{
    const bool hasTray = m_tray && m_tray->isVisible();
    const bool userIsLooking = isVisible() && !isMinimized() && isActiveWindow();

    // Without a tray there is nowhere else to say it, so the message would be
    // lost; and when the window is in front, a system notification is overkill.
    if (!hasTray || userIsLooking)
        showStatus(message);
    else
        m_tray->showMessage(QStringLiteral("PinIt"), message,
                            QSystemTrayIcon::Information, 2500);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_tray && m_tray->isVisible()) {
        hide();
        event->ignore();
        if (!m_settings.hasSeenTrayNotice) {
            m_settings.hasSeenTrayNotice = true;
            persistence::saveSettings(m_settings);
            m_tray->showMessage(
                QStringLiteral("PinIt"),
                tr("PinIt is still running in the tray. Right-click the icon to quit."),
                QSystemTrayIcon::Information, 3000);
        }
    } else {
        // No system tray to live in — closing the window must actually quit,
        // otherwise PinIt would keep running with no window and no tray icon
        // (quitOnLastWindowClosed is off), leaving Task Manager the only way out.
        event->accept();
        QCoreApplication::quit();
    }
}
