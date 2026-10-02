//
// PinIt — keep any window always on top (Win+Ctrl+T), C++/Qt port.
//
// Wires the pieces together:
//   GlobalHotkeyManager  -> system-wide hotkeys (WM_HOTKEY)
//   PinManager           -> Win32 always-on-top + opacity + persistence
//   MainWindow           -> UI + system tray
//
#include <QApplication>
#include <QMessageBox>
#include <QIcon>
#include <QSystemTrayIcon>
#include <QSessionManager>

#include "pinmanager.h"
#include "globalhotkey.h"
#include "sessionwatcher.h"
#include "singleinstance.h"
#include "mainwindow.h"
#include "persistence.h"
#include "logging.h"
#include "theme.h"
#include "version.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("PinIt"));
    QCoreApplication::setOrganizationName(QStringLiteral("PinIt"));
    QApplication::setApplicationVersion(QStringLiteral(PINIT_VERSION_STR));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icon.png")));
    theme::followSystem(app);

    logging::init();
    qInfo("PinIt %s starting", PINIT_VERSION_STR);

    // Single instance: if PinIt is already running, ask it to show its window
    // and exit — instead of dying silently. (The name is also the installer's
    // AppMutex; keep the two in step.)
    SingleInstance instance(QStringLiteral("PinIt_SingleInstance_v2"));
    if (!instance.isPrimary()) {
        instance.askPrimaryToShow();
        qInfo("Another instance is running; asked it to show");
        return 0;
    }

    // Keep running when the window closes to the tray.
    app.setQuitOnLastWindowClosed(false);

    PinManager manager;
    MainWindow window(&manager);

    // On quit, un-pin/un-fade any windows we touched so nothing is left stuck
    // always-on-top or translucent.
    QObject::connect(&app, &QApplication::aboutToQuit, &manager,
                     &PinManager::restoreAllWindows);

    // Distinguish a manual quit from Windows logging off / shutting down. On a
    // session end we keep the saved pins so they're re-pinned next login; on a
    // manual quit we forget them. commitDataRequest fires before aboutToQuit.
    QObject::connect(&app, &QGuiApplication::commitDataRequest, &manager,
                     [&manager](QSessionManager &) { manager.markSessionEnding(); });
    // ...and notice when that shutdown is cancelled, so the flag doesn't stick
    // and make a later manual quit keep the pins.
    SessionWatcher sessionWatcher;
    app.installNativeEventFilter(&sessionWatcher);
    QObject::connect(&sessionWatcher, &SessionWatcher::sessionEndCancelled, &manager,
                     &PinManager::clearSessionEnding);

    // A later launch means "show the window".
    QObject::connect(&instance, &SingleInstance::showRequested, &window,
                     &MainWindow::showFromTray);

    GlobalHotkeyManager hotkeys;
    app.installNativeEventFilter(&hotkeys);

    QObject::connect(&hotkeys, &GlobalHotkeyManager::togglePin,
                     &manager, &PinManager::toggleForeground);
    QObject::connect(&hotkeys, &GlobalHotkeyManager::opacityUp,
                     &manager, [&manager]() { manager.adjustForegroundOpacity(5); });
    QObject::connect(&hotkeys, &GlobalHotkeyManager::opacityDown,
                     &manager, [&manager]() { manager.adjustForegroundOpacity(-5); });
    QObject::connect(&hotkeys, &GlobalHotkeyManager::toggleWindow,
                     &window, &MainWindow::toggleVisibility);

    // The one way hotkeys get (re)registered — at startup and whenever the
    // Shortcuts dialog tries a new set. Returns the actions that failed.
    const auto applyShortcuts = [&hotkeys](const persistence::ShortcutConfig &c) {
        hotkeys.registerAll(c);
        return hotkeys.failedActions();
    };
    window.setShortcutApplier(applyShortcuts);

    const QStringList failedHotkeys = applyShortcuts(window.shortcutConfig());
    window.setHotkeyProblems(failedHotkeys);
    if (!failedHotkeys.isEmpty()) {
        qWarning("Hotkeys unavailable: %s",
                 qUtf8Printable(failedHotkeys.join(QStringLiteral(", "))));
        window.notify(QObject::tr("Some hotkeys are unavailable: %1")
                          .arg(failedHotkeys.join(QStringLiteral(", "))));
    }

    // Re-pin whatever was pinned last session.
    manager.restoreSaved();

    // When launched at login with --minimized, start silently in the tray
    // instead of popping the window. Fall back to showing it if there's no tray.
    const bool startMinimized =
        QCoreApplication::arguments().contains(QStringLiteral("--minimized"));
    if (!startMinimized || !QSystemTrayIcon::isSystemTrayAvailable())
        window.show();

    return app.exec();
}
