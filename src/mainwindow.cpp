#include "mainwindow.h"
#include "pinmanager.h"
#include "winpin.h"
#include "shortcuts.h"
#include "shortcutsdialog.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
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
#include <QColor>
#include <QCursor>

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

// Console apps (PowerShell, cmd) set their window title to a full path.
// Show just the final component so the list stays readable.
QString displayTitle(const QString &title)
{
    const int slash = title.lastIndexOf(QLatin1Char('\\'));
    if (slash >= 0 && slash < title.size() - 1)
        return title.mid(slash + 1);
    return title;
}

// Deterministic avatar colour for a process name (ported from the original
// PinIt frontend) so each pinned app gets a stable little badge.
QColor avatarColor(const QString &name)
{
    static const char *kColors[] = {
        "#e57373", "#f06292", "#ba68c8", "#9575cd", "#7986cb",
        "#64b5f6", "#4fc3f7", "#4dd0e1", "#4db6ac", "#81c784",
        "#aed581", "#ffd54f", "#ffb74d", "#ff8a65", "#a1887f",
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
    rebuildList();

    connect(m_manager, &PinManager::pinsChanged, this, &MainWindow::rebuildList);
    connect(m_manager, &PinManager::errorOccurred, this, &MainWindow::notify);
    connect(m_manager, &PinManager::pinToggled, this,
            [this](bool pinned, const QString &title, const QString &) {
                if (pinned && m_settings.enableSound)
                    winpin::beep();
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

    auto *editShortcuts = new QPushButton(tr("Edit shortcuts…"));
    connect(editShortcuts, &QPushButton::clicked, this, &MainWindow::openShortcutsDialog);
    root->addWidget(editShortcuts, 0, Qt::AlignLeft);

    // --- PINNED (n) ----------------------------------------------------------
    m_pinnedHeader = new QLabel(tr("PINNED (0)"));
    m_pinnedHeader->setProperty("role", "section");
    root->addWidget(m_pinnedHeader);

    auto *scroll = new QScrollArea(central);
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

    setCentralWidget(central);
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

void MainWindow::rebuildList()
{
    // Remove previously-built pin cards, keeping the empty card and the stretch.
    for (int i = m_listLayout->count() - 1; i >= 0; --i) {
        QWidget *w = m_listLayout->itemAt(i)->widget();
        if (!w || w == m_emptyCard)
            continue;
        delete m_listLayout->takeAt(i);
        w->deleteLater();
    }

    const QVector<PinnedWindow> pinned = m_manager->pinnedWindows();
    if (m_emptyCard)
        m_emptyCard->setVisible(pinned.isEmpty());
    if (m_pinnedHeader)
        m_pinnedHeader->setText(tr("PINNED (%1)").arg(pinned.size()));

    for (const PinnedWindow &w : pinned) {
        const intptr_t hwnd = w.hwnd;

        // One compact row per pin: [avatar] [title / process] [slider] [%] [x]
        auto *card = makeCard();
        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(10, 6, 8, 6);
        row->setSpacing(8);

        // Coloured badge with the process initial.
        auto *avatar = new QLabel(avatarInitial(w.processName));
        avatar->setFixedSize(28, 28);
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setStyleSheet(QStringLiteral(
            "background:%1; border-radius:6px; color:white;"
            "font-weight:700; font-size:12px;").arg(avatarColor(w.processName).name()));
        row->addWidget(avatar);

        // Title + process name stacked tightly; takes the leftover width.
        auto *info = new QVBoxLayout;
        info->setSpacing(0);
        auto *name = new QLabel;
        name->setStyleSheet(QStringLiteral("font-weight: 600;"));
        // Elide so a long title never widens the card or forces a scrollbar.
        name->setText(name->fontMetrics().elidedText(
            displayTitle(w.title), Qt::ElideRight, 150));
        name->setToolTip(w.title);   // full title on hover
        auto *proc = new QLabel(w.processName);
        proc->setProperty("role", "muted");
        info->addWidget(name);
        info->addWidget(proc);
        row->addLayout(info, 1);

        // Opacity slider + percentage.
        auto *slider = new QSlider(Qt::Horizontal);
        slider->setRange(winpin::kMinOpacity, winpin::kMaxOpacity);
        slider->setValue(w.opacity);
        slider->setFixedWidth(76);
        // The round handle is pulled out over the thin groove (margin:-6px in
        // the QSS); without enough vertical room it gets clipped at the top.
        slider->setMinimumHeight(20);
        row->addWidget(slider);

        auto *pct = new QLabel(QStringLiteral("%1%").arg(w.opacity));
        pct->setProperty("role", "muted");
        pct->setMinimumWidth(30);
        pct->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(pct);

        connect(slider, &QSlider::valueChanged, this, [this, hwnd, pct](int v) {
            pct->setText(QStringLiteral("%1%").arg(v));
            m_manager->setOpacity(hwnd, v);
        });

        // Compact unpin button (full label still available as a tooltip).
        auto *unpinBtn = new QPushButton(QString::fromUtf8("\xE2\x9C\x95"));   // ✕
        unpinBtn->setObjectName(QStringLiteral("unpin"));
        unpinBtn->setFixedSize(24, 24);
        unpinBtn->setToolTip(tr("Unpin this window"));
        unpinBtn->setCursor(Qt::PointingHandCursor);
        connect(unpinBtn, &QPushButton::clicked, this,
                [this, hwnd]() { m_manager->unpin(hwnd); });
        row->addWidget(unpinBtn);

        m_listLayout->insertWidget(m_listLayout->count() - 1, card);
    }

    if (m_tray) {
        const int n = pinned.size();
        m_tray->setToolTip(n == 0 ? tr("PinIt — no windows pinned")
                                  : tr("PinIt — %n window(s) pinned", "", n));
    }
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
                if (reason == QSystemTrayIcon::Trigger ||
                    reason == QSystemTrayIcon::DoubleClick)
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
    if (isVisible() && !isMinimized())
        hide();
    else
        showFromTray();
}

void MainWindow::showFromTray()
{
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::notify(const QString &message)
{
    if (m_tray && m_tray->isVisible())
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
