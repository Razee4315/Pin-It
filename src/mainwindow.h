#pragma once
//
// MainWindow — the PinIt UI: list of pinned windows with opacity sliders,
// an "add window" picker, settings, and the system-tray integration.
//
#include <QElapsedTimer>
#include <QMainWindow>

#include <functional>
#include <QHash>

#include "persistence.h"

class PinManager;
class PinRow;
class PendingRow;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QWidget;
class QSystemTrayIcon;
class QMenu;
class QCheckBox;
class QLabel;
class QScrollArea;
class QTimer;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(PinManager *manager, QWidget *parent = nullptr);

    // The settings MainWindow loaded at construction (so main() doesn't have to
    // read the file a second time just to register the initial hotkeys).
    persistence::ShortcutConfig shortcutConfig() const { return m_settings.shortcuts; }

    // Names of the actions whose hotkey could not be registered (empty = all
    // fine). Shown as a standing warning in the window and the tray tooltip —
    // a one-off notification at login is too easy to miss.
    void setHotkeyProblems(const QStringList &failedActions);

    // How a shortcut set is made live (registered with Windows). Returns the
    // names of the actions that could not be registered; empty = all active.
    using ShortcutApplier = std::function<QStringList(const persistence::ShortcutConfig &)>;
    void setShortcutApplier(ShortcutApplier applier) { m_applyShortcuts = std::move(applier); }

public slots:
    void toggleVisibility();      // bound to the Show/Hide hotkey
    void showFromTray();
    // Transient message: shown inside the window while the user is looking at
    // it (or when there is no tray), as a tray notification otherwise.
    void notify(const QString &message);

protected:
    void closeEvent(QCloseEvent *event) override;   // hide to tray
    void changeEvent(QEvent *event) override;       // tracks when focus was lost
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void syncList();              // bring the rows in line with the pin list
    void addWindowDialog();
    void showAbout();
    void openShortcutsDialog();
    void unpinAll();

private:
    void buildUi();
    void buildTray();
    void fillShortcutRows(QVBoxLayout *scv);   // (re)builds the SHORTCUTS chips
    void fillEmptyHint();                      // (re)builds the empty-state hotkey hint
    void showStatus(const QString &message);   // in-window message, fades by itself
    void placeStatus();
    void updateTrayToolTip();
    void fillTrayMenu();
    void updateTabOrder();     // top to bottom, including the list rows

    PinManager      *m_manager = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu           *m_trayMenu = nullptr;
    QScrollArea     *m_scroll = nullptr;
    QLabel          *m_status = nullptr;        // floats over the bottom of the list
    QTimer          *m_statusTimer = nullptr;
    QVBoxLayout     *m_listLayout = nullptr;
    QHash<intptr_t, PinRow *> m_rows;   // one live row per pinned window
    QList<PendingRow *> m_pendingRows;  // saved pins still waiting for their window
    QPushButton     *m_editShortcuts = nullptr;
    QPushButton     *m_addButton = nullptr;
    QPushButton     *m_unpinAll = nullptr;
    QPushButton     *m_aboutButton = nullptr;
    QLabel          *m_pinnedHeader = nullptr;
    QLabel          *m_hotkeyWarning = nullptr;
    QStringList      m_hotkeyProblems;
    QWidget         *m_emptyCard = nullptr;
    QVBoxLayout     *m_shortcutsLayout = nullptr;
    QHBoxLayout     *m_emptyHint = nullptr;
    QCheckBox       *m_soundBox = nullptr;
    QCheckBox       *m_notifyBox = nullptr;
    QCheckBox       *m_autostartBox = nullptr;

    // Running since the window last lost focus (see toggleVisibility).
    QElapsedTimer m_sinceDeactivated;

    ShortcutApplier m_applyShortcuts;

    persistence::UserSettings m_settings;
};
