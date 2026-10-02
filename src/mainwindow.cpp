#include "mainwindow.h"
#include "pinmanager.h"
#include "winpin.h"
#include "shortcuts.h"
#include "shortcutsdialog.h"
#include "pinrow.h"
#include "elidedlabel.h"
#include "pinflash.h"
#include "windowpicker.h"
#include "autostart.h"

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
#include <QCloseEvent>
#include <QPixmap>
#include <QIcon>
#include <QCoreApplication>
#include <QMessageBox>
#include <QSet>
#include <QTimer>
#include <QAccessible>
#include <QDesktopServices>
#include <QUrl>

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

// A shortcut as chips: [ Win ] + [ Ctrl ] + [ T ].
void addKeyChips(QHBoxLayout *row, const QStringList &keys)
{
    for (int i = 0; i < keys.size(); ++i) {
        if (i > 0)
            row->addWidget(plusLabel());
        row->addWidget(keyChip(keys[i]));
    }
}

// Empty a layout, deleting its widgets and nested layouts.
void clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QLayout *child = item->layout())
            clearLayout(child);
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
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

    // Fixed width, free height: the layout is a single column, so only the
    // pinned list benefits from more room — and it takes all the extra height
    // the user gives the window. No maximize button.
    constexpr int kWidth = 380;
    constexpr int kMinHeight = 500;
    constexpr int kDefaultHeight = 600;
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
                   | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
    setFixedWidth(kWidth);
    setMinimumHeight(kMinHeight);
    resize(kWidth, kDefaultHeight);

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
    connect(m_manager, &PinManager::clickThroughChanged, this,
            [this](intptr_t hwnd, bool enabled) {
                if (PinRow *row = m_rows.value(hwnd))
                    row->setClickThrough(enabled);
            });
    connect(m_manager, &PinManager::opacityChanged, this, [this](intptr_t hwnd, int percent) {
        if (PinRow *row = m_rows.value(hwnd))
            row->setOpacity(percent);
    });
    // A saved pin found its window (which the user has just opened): outline
    // it so the silent re-pin doesn't go unnoticed.
    connect(m_manager, &PinManager::pinRestored, this,
            [](intptr_t hwnd) { pinflash::show(hwnd, pinflash::Kind::Pinned); });
    connect(m_manager, &PinManager::pinToggled, this,
            [this](intptr_t hwnd, bool pinned, const QString &title) {
                if (pinned && m_settings.enableSound)
                    winpin::playPinSound();
                // The outline around the window itself is the primary feedback:
                // instant, and exactly where the user is looking.
                pinflash::show(hwnd, pinned ? pinflash::Kind::Pinned
                                            : pinflash::Kind::Unpinned);
                if (m_settings.showNotifications)
                    notify(pinned ? tr("Pinned: %1").arg(displayTitle(title))
                                  : tr("Unpinned: %1").arg(displayTitle(title)));
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
    // Lives in the header rather than on a row of its own, which leaves that
    // height to the pinned list.
    auto *editShortcuts = new QPushButton(tr("Edit shortcuts…"));
    connect(editShortcuts, &QPushButton::clicked, this, &MainWindow::openShortcutsDialog);
    header->addWidget(editShortcuts, 0, Qt::AlignVCenter);
    m_editShortcuts = editShortcuts;
    root->addLayout(header);

    // --- Pin button ----------------------------------------------------------
    auto *addBtn = new QPushButton(tr("+   Pin a window…"));
    addBtn->setObjectName(QStringLiteral("primary"));
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addWindowDialog);
    root->addWidget(addBtn);
    m_addButton = addBtn;

    // --- SHORTCUTS -----------------------------------------------------------
    auto *scLabel = new QLabel(tr("SHORTCUTS"));
    scLabel->setProperty("role", "section");
    root->addWidget(scLabel);

    auto *scCard = makeCard();
    // Never squeezed: when space is short it is the list that scrolls.
    scCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *scv = new QVBoxLayout(scCard);
    scv->setContentsMargins(12, 8, 12, 8);
    scv->setSpacing(6);
    m_shortcutsLayout = scv;
    fillShortcutRows(scv);
    root->addWidget(scCard);

    m_hotkeyWarning = new QLabel;
    m_hotkeyWarning->setProperty("role", "warning");
    m_hotkeyWarning->setWordWrap(true);
    m_hotkeyWarning->hide();
    root->addWidget(m_hotkeyWarning);

    // --- PINNED (n) ----------------------------------------------------------
    m_pinnedHeader = new QLabel(tr("PINNED (0)"));
    m_pinnedHeader->setProperty("role", "section");
    root->addWidget(m_pinnedHeader);

    auto *scroll = new QScrollArea(central);
    m_scroll = scroll;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Not a tab stop itself: its rows are, and focusing one scrolls it into view.
    scroll->setFocusPolicy(Qt::NoFocus);
    scroll->setMinimumHeight(108);   // two rows, even with the hotkey warning showing
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
    m_emptyHint = new QHBoxLayout;
    fillEmptyHint();
    ec->addLayout(m_emptyHint);
    m_listLayout->insertWidget(0, m_emptyCard);   // lives in the list region

    // --- Settings (compact, at the bottom) -----------------------------------
    m_soundBox = new QCheckBox(tr("Play a sound when pinning"));
    m_soundBox->setChecked(m_settings.enableSound);
    connect(m_soundBox, &QCheckBox::toggled, this, [this](bool on) {
        m_settings.enableSound = on;
        persistence::saveSettings(m_settings);
    });
    root->addWidget(m_soundBox);

    m_notifyBox = new QCheckBox(tr("Show a notification when pinning"));
    m_notifyBox->setChecked(m_settings.showNotifications);
    connect(m_notifyBox, &QCheckBox::toggled, this, [this](bool on) {
        m_settings.showNotifications = on;
        persistence::saveSettings(m_settings);
    });
    root->addWidget(m_notifyBox);

    m_autostartBox = new QCheckBox(tr("Start PinIt with Windows"));
    // The Run key is the truth (the installer can set it too); the copy in
    // the settings file is only kept in step for older versions.
    autostart::repairPath();
    m_autostartBox->setChecked(autostart::isEnabled());
    connect(m_autostartBox, &QCheckBox::toggled, this, [this](bool on) {
        autostart::setEnabled(on);
        m_settings.startWithWindows = on;
        persistence::saveSettings(m_settings);
    });
    root->addWidget(m_autostartBox);

    // --- Footer -------------------------------------------------------------
    auto *footer = new QHBoxLayout;
    m_unpinAll = new QPushButton(tr("Unpin all"));
    m_unpinAll->setObjectName(QStringLiteral("link"));
    connect(m_unpinAll, &QPushButton::clicked, this, &MainWindow::unpinAll);
    footer->addWidget(m_unpinAll);
    footer->addStretch();
    // Also reachable without the tray (version number for bug reports).
    m_aboutButton = new QPushButton(tr("About"));
    m_aboutButton->setObjectName(QStringLiteral("link"));
    connect(m_aboutButton, &QPushButton::clicked, this, &MainWindow::showAbout);
    footer->addWidget(m_aboutButton);
    root->addLayout(footer);

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
                : n == 1 ? tr("PinIt — 1 window pinned")
                         : tr("PinIt — %1 windows pinned").arg(n);
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

void MainWindow::fillShortcutRows(QVBoxLayout *scv)
{
    clearLayout(scv);

    const persistence::ShortcutConfig &sc = m_settings.shortcuts;

    // One row: the shortcut's chips, optionally "/ [alternative key]", then
    // what it does on the right.
    auto addRow = [scv](const QStringList &keys, const QString &desc,
                        const QString &alternativeKey = QString()) {
        auto *row = new QHBoxLayout;
        row->setSpacing(6);
        addKeyChips(row, keys);
        if (!alternativeKey.isEmpty()) {
            row->addWidget(plusLabel(QStringLiteral("/")));
            row->addWidget(keyChip(alternativeKey));
        }
        // The description takes what is left and shortens itself if a long
        // shortcut (three modifiers) leaves little room.
        auto *d = new ElidedLabel(desc);
        d->setProperty("role", "desc");
        d->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        d->setToolTip(desc);
        row->addWidget(d, 1);
        scv->addLayout(row);
    };

    addRow(shortcuts::displayTokens(sc.togglePin), tr("Pin / unpin window"));

    // The two opacity shortcuts share one row ("Win + Ctrl + = / -") only when
    // they really differ in nothing but the key; otherwise each gets its own
    // row so the modifiers shown are the ones that work.
    const QStringList up = shortcuts::displayTokens(sc.opacityUp);
    const QStringList down = shortcuts::displayTokens(sc.opacityDown);
    const bool sameModifiers = !up.isEmpty() && !down.isEmpty()
        && up.mid(0, up.size() - 1) == down.mid(0, down.size() - 1);
    if (sameModifiers) {
        addRow(up, tr("Adjust opacity"), down.last());
    } else {
        addRow(up, tr("Increase opacity"));
        addRow(down, tr("Decrease opacity"));
    }

    addRow(shortcuts::displayTokens(sc.toggleWindow), tr("Show / hide PinIt"));
}

void MainWindow::fillEmptyHint()
{
    clearLayout(m_emptyHint);
    m_emptyHint->addStretch();
    auto *use = new QLabel(tr("Use"));
    use->setProperty("role", "muted");
    m_emptyHint->addWidget(use);
    addKeyChips(m_emptyHint, shortcuts::displayTokens(m_settings.shortcuts.togglePin));
    m_emptyHint->addStretch();
}

void MainWindow::openShortcutsDialog()
{
    // The dialog only closes with OK once Windows has accepted the new set.
    // If it is refused, the set that was working is put straight back.
    const auto tryShortcuts = [this](const persistence::ShortcutConfig &candidate) {
        if (!m_applyShortcuts)
            return QStringList();
        const QStringList refused = m_applyShortcuts(candidate);
        if (!refused.isEmpty())
            m_applyShortcuts(m_settings.shortcuts);
        return refused;
    };

    ShortcutsDialog dlg(m_settings.shortcuts, tryShortcuts, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    // Only a set that is live gets saved.
    m_settings.shortcuts = dlg.config();
    persistence::saveSettings(m_settings);
    if (m_shortcutsLayout)
        fillShortcutRows(m_shortcutsLayout);
    fillEmptyHint();   // the "Use [Win]+[Ctrl]+[T]" hint shows the pin shortcut too
    setHotkeyProblems({});
    notify(tr("Shortcuts updated."));
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
        connect(row, &PinRow::clickThroughRequested, this, [this, hwnd, row](bool enabled) {
            // If Windows refuses, put the button back to the real state.
            if (!m_manager->setClickThrough(hwnd, enabled))
                row->setClickThrough(!enabled);
        });
        connect(row, &PinRow::unpinRequested, this,
                [this, hwnd]() { m_manager->unpin(hwnd); });
        connect(row, &PinRow::locateRequested, this,
                [hwnd]() { pinflash::show(hwnd, pinflash::Kind::Pinned); });
        m_rows.insert(hwnd, row);
        // After the last live row, ahead of the waiting rows and the stretch.
        m_listLayout->insertWidget(m_listLayout->count() - 1 - m_pendingRows.size(), row);
    }

    // Waiting rows are static, so simply rebuild them.
    for (PendingRow *row : std::as_const(m_pendingRows)) {
        m_listLayout->removeWidget(row);
        row->hide();
        row->deleteLater();
    }
    m_pendingRows.clear();
    const QVector<persistence::SavedPin> pending = m_manager->pendingPins();
    for (int i = 0; i < pending.size(); ++i) {
        auto *row = new PendingRow(pending[i]);
        connect(row, &PendingRow::forgetRequested, this,
                [this, i]() { m_manager->forgetPending(i); });
        m_pendingRows.push_back(row);
        m_listLayout->insertWidget(m_listLayout->count() - 1, row);   // before the stretch
    }

    if (m_emptyCard)
        m_emptyCard->setVisible(pinned.isEmpty() && pending.isEmpty());
    if (m_pinnedHeader) {
        m_pinnedHeader->setText(pending.isEmpty()
            ? tr("PINNED (%1)").arg(pinned.size())
            : tr("PINNED (%1)  ·  WAITING (%2)").arg(pinned.size()).arg(pending.size()));
    }

    m_unpinAll->setEnabled(!pinned.isEmpty() || !pending.isEmpty());

    updateTrayToolTip();
    updateTabOrder();
}

void MainWindow::unpinAll()
{
    // Outline each window as it is let go, like a single unpin does.
    const QVector<PinnedWindow> windows = m_manager->pinnedWindows();
    for (const PinnedWindow &w : windows)
        pinflash::show(w.hwnd, pinflash::Kind::Unpinned);

    const int count = m_manager->unpinAll();
    notify(count == 1 ? tr("Unpinned 1 window.") : tr("Unpinned %1 windows.").arg(count));
}

void MainWindow::updateTabOrder()
{
    // Rows are created long after the rest of the window, which would put
    // them at the very end of the tab chain. Keep Tab moving top to bottom.
    QList<QWidget *> chain = {m_editShortcuts, m_addButton};
    const QVector<PinnedWindow> pinned = m_manager->pinnedWindows();
    for (const PinnedWindow &w : pinned) {
        if (const PinRow *row = m_rows.value(w.hwnd))
            chain += row->focusChain();
    }
    for (const PendingRow *row : std::as_const(m_pendingRows))
        chain += row->focusChain();
    chain += {m_soundBox, m_notifyBox, m_autostartBox, m_unpinAll, m_aboutButton};

    for (qsizetype i = 1; i < chain.size(); ++i)
        QWidget::setTabOrder(chain[i - 1], chain[i]);
}

void MainWindow::addWindowDialog()
{
    QVector<winpin::PinnableWindow> candidates;
    for (const winpin::PinnableWindow &w : winpin::enumerateWindows()) {
        if (!m_manager->isPinned(w.hwnd))
            candidates.push_back(w);
    }

    WindowPicker picker(candidates, this);
    if (picker.exec() != QDialog::Accepted)
        return;
    if (const intptr_t hwnd = picker.selectedWindow())
        m_manager->pin(hwnd);
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
        "<p><a href=\"%6/releases/latest\">%8</a></p>"
        "<p style='color:gray'>%7</p>")
        .arg(QStringLiteral(PINIT_PRODUCT),
             QStringLiteral(PINIT_VERSION_STR),
             tr("Keep any window always on top — with a global hotkey."),
             QStringLiteral(QT_VERSION_STR),
             QStringLiteral(PINIT_COMPANY),
             QStringLiteral(PINIT_URL),
             QStringLiteral(PINIT_COPYRIGHT),
             tr("Check for a newer version")));
    box.exec();
}

void MainWindow::buildTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_tray = new QSystemTrayIcon(appIcon(), this);

    // Filled each time it opens, so it always reflects the current pins.
    m_trayMenu = new QMenu(this);
    connect(m_trayMenu, &QMenu::aboutToShow, this, &MainWindow::fillTrayMenu);
    fillTrayMenu();

    m_tray->setContextMenu(m_trayMenu);
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

void MainWindow::fillTrayMenu()
{
    m_trayMenu->clear();
    const int pinned = m_manager->pinnedCount();

    m_trayMenu->addAction(tr("Show PinIt"), this, &MainWindow::showFromTray);
    m_trayMenu->addAction(tr("Pin a window…"), this, &MainWindow::addWindowDialog);
    m_trayMenu->addSeparator();

    // What is pinned right now, each one click away from being unpinned —
    // so the tray alone is enough to see and manage pins.
    const QVector<PinnedWindow> windows = m_manager->pinnedWindows();
    for (const PinnedWindow &w : windows) {
        const intptr_t hwnd = w.hwnd;
        // Keep the menu narrow, and stop "&" in a title becoming a mnemonic.
        QString title = m_trayMenu->fontMetrics().elidedText(displayTitle(w.title),
                                                             Qt::ElideRight, 260);
        title.replace(QLatin1Char('&'), QLatin1String("&&"));
        m_trayMenu->addAction(tr("Unpin: %1").arg(title), this,
                              [this, hwnd]() { m_manager->unpin(hwnd); });
    }
    if (!windows.isEmpty()) {
        m_trayMenu->addAction(tr("Unpin all"), this, &MainWindow::unpinAll);
        m_trayMenu->addSeparator();
    }

    // PinIt never goes online by itself; this just opens the releases page
    // in the browser.
    m_trayMenu->addAction(tr("Check for updates…"), this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral(PINIT_URL "/releases/latest")));
    });
    m_trayMenu->addAction(tr("About PinIt"), this, &MainWindow::showAbout);
    m_trayMenu->addSeparator();

    // Quitting un-pins everything and forgets the pins; say so up front
    // rather than surprising the user afterwards.
    const QString quitText = pinned == 0 ? tr("Quit")
                           : pinned == 1 ? tr("Quit and unpin 1 window")
                                         : tr("Quit and unpin %1 windows").arg(pinned);
    m_trayMenu->addAction(quitText, qApp, &QApplication::quit);
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
