#pragma once
//
// MainWindow — the PinIt UI: list of pinned windows with opacity sliders,
// an "add window" picker, settings, and the system-tray integration.
//
#include <QElapsedTimer>
#include <QMainWindow>
#include <QHash>

#include "persistence.h"

class PinManager;
class PinRow;
class QVBoxLayout;
class QWidget;
class QSystemTrayIcon;
class QCheckBox;
class QLabel;
class QScrollArea;
class QTimer;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(PinManager *manager, QWidget *parent = nullptr);

    void setShortcutConfig(const persistence::ShortcutConfig &cfg);

    // The settings MainWindow loaded at construction (so main() doesn't have to
    // read the file a second time just to register the initial hotkeys).
    persistence::ShortcutConfig shortcutConfig() const { return m_settings.shortcuts; }

    // Names of the actions whose hotkey could not be registered (empty = all
    // fine). Shown as a standing warning in the window and the tray tooltip —
    // a one-off notification at login is too easy to miss.
    void setHotkeyProblems(const QStringList &failedActions);

signals:
    void shortcutsChanged(const persistence::ShortcutConfig &cfg);

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

private:
    void buildUi();
    void buildTray();
    void applyAutostart(bool enabled);
    void fillShortcutRows(QVBoxLayout *scv);   // (re)builds the SHORTCUTS chips
    void showStatus(const QString &message);   // in-window message, fades by itself
    void placeStatus();
    void updateTrayToolTip();

    PinManager      *m_manager = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QScrollArea     *m_scroll = nullptr;
    QLabel          *m_status = nullptr;        // floats over the bottom of the list
    QTimer          *m_statusTimer = nullptr;
    QVBoxLayout     *m_listLayout = nullptr;
    QHash<intptr_t, PinRow *> m_rows;   // one live row per pinned window
    QList<QWidget *> m_pendingRows;     // saved pins still waiting for their window
    QLabel          *m_emptyLabel = nullptr;
    QLabel          *m_pinnedHeader = nullptr;
    QLabel          *m_hotkeyWarning = nullptr;
    QStringList      m_hotkeyProblems;
    QWidget         *m_emptyCard = nullptr;
    QVBoxLayout     *m_shortcutsLayout = nullptr;
    QCheckBox       *m_soundBox = nullptr;
    QCheckBox       *m_notifyBox = nullptr;
    QCheckBox       *m_autostartBox = nullptr;
    QLabel          *m_shortcutsLabel = nullptr;

    // Running since the window last lost focus (see toggleVisibility).
    QElapsedTimer m_sinceDeactivated;

    persistence::UserSettings m_settings;
};
