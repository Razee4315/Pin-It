#pragma once
//
// MainWindow — the PinIt UI: list of pinned windows with opacity sliders,
// an "add window" picker, settings, and the system-tray integration.
//
#include <QMainWindow>

#include "persistence.h"

class PinManager;
class QVBoxLayout;
class QWidget;
class QSystemTrayIcon;
class QCheckBox;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(PinManager *manager, QWidget *parent = nullptr);

    void setShortcutConfig(const persistence::ShortcutConfig &cfg);

    // The settings MainWindow loaded at construction (so main() doesn't have to
    // read the file a second time just to register the initial hotkeys).
    persistence::ShortcutConfig shortcutConfig() const { return m_settings.shortcuts; }

signals:
    void shortcutsChanged(const persistence::ShortcutConfig &cfg);

public slots:
    void toggleVisibility();      // bound to the Show/Hide hotkey
    void showFromTray();
    void notify(const QString &message);   // transient tray balloon

protected:
    void closeEvent(QCloseEvent *event) override;   // hide to tray

private slots:
    void rebuildList();
    void addWindowDialog();
    void showAbout();
    void openShortcutsDialog();

private:
    void buildUi();
    void buildTray();
    void applyAutostart(bool enabled);
    void fillShortcutRows(QVBoxLayout *scv);   // (re)builds the SHORTCUTS chips

    PinManager      *m_manager = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QVBoxLayout     *m_listLayout = nullptr;
    QLabel          *m_emptyLabel = nullptr;
    QLabel          *m_pinnedHeader = nullptr;
    QWidget         *m_emptyCard = nullptr;
    QVBoxLayout     *m_shortcutsLayout = nullptr;
    QCheckBox       *m_soundBox = nullptr;
    QCheckBox       *m_autostartBox = nullptr;
    QLabel          *m_shortcutsLabel = nullptr;

    persistence::UserSettings m_settings;
};
