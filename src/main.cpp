//
// PinIt — keep any window always on top (Win+Ctrl+T), C++/Qt port.
//
// Wires the pieces together:
//   GlobalHotkeyManager  -> system-wide hotkeys (WM_HOTKEY)
//   PinManager           -> Win32 always-on-top + opacity + persistence
//   MainWindow           -> UI + system tray
//
#include <QApplication>
#include <QSharedMemory>
#include <QMessageBox>
#include <QIcon>
#include <QSystemTrayIcon>

#include "pinmanager.h"
#include "globalhotkey.h"
#include "mainwindow.h"
#include "persistence.h"

// Warm "paper" theme — ported from the original PinIt CSS variables.
static const char *kStyleSheet = R"qss(
QWidget#central { background: #f8f6f2; }
QDialog { background: #f8f6f2; }
QLabel { color: #2a2622; font-family: "Segoe UI"; }

QLabel[role="title"]   { font-size: 17px; font-weight: 700; color: #2a2622; }
QLabel[role="section"] { font-size: 11px; font-weight: 700; color: #6b6760;
                         letter-spacing: 1px; }
QLabel[role="desc"]    { color: #6b6760; font-size: 12px; }
QLabel[role="muted"]   { color: #6b6760; font-size: 12px; }

QLabel[role="key"] {
    background: #f0ede6; border: 1px solid rgba(0,0,0,0.12);
    border-radius: 5px; padding: 3px 9px;
    color: #2a2622; font-weight: 700; font-size: 11px;
}
QLabel[role="plus"] { color: #9a948a; font-size: 12px; }

QFrame[role="card"] {
    background: #ffffff; border: 1px solid rgba(0,0,0,0.08);
    border-radius: 12px;
}

QPushButton {
    background: #ffffff; border: 1px solid rgba(0,0,0,0.12);
    border-radius: 8px; padding: 7px 14px; color: #2a2622; font-size: 12px;
}
QPushButton:hover { background: #f0ede6; }

QPushButton#primary {
    background: #c49464; border: none; color: #ffffff; font-weight: 700;
    padding: 9px 14px;
}
QPushButton#primary:hover { background: #b6855a; }

QCheckBox { color: #5a564e; font-size: 12px; spacing: 7px; }

QSlider::groove:horizontal { height: 4px; background: #e6e2da; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #c49464; border-radius: 2px; }
QSlider::handle:horizontal {
    background: #ffffff; border: 1px solid #c49464; width: 14px; height: 14px;
    margin: -6px 0; border-radius: 7px;
}
QScrollArea { background: transparent; border: none; }
)qss";

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("PinIt"));
    QCoreApplication::setOrganizationName(QStringLiteral("PinIt"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icon.png")));
    app.setStyleSheet(QString::fromUtf8(kStyleSheet));

    // Single-instance guard: if PinIt is already running, just exit.
    QSharedMemory guard(QStringLiteral("PinIt_SingleInstance_v1"));
    if (!guard.create(1)) {
        return 0;
    }

    // Keep running when the window closes to the tray.
    app.setQuitOnLastWindowClosed(false);

    const persistence::UserSettings settings = persistence::loadSettings();

    PinManager manager;
    MainWindow window(&manager);

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

    if (!hotkeys.registerAll(settings.shortcuts)) {
        window.notify(QObject::tr(
            "Could not register global hotkeys — another app may be using them."));
    } else if (!hotkeys.failedActions().isEmpty()) {
        window.notify(QObject::tr("Some hotkeys are unavailable: %1")
                          .arg(hotkeys.failedActions().join(QStringLiteral(", "))));
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
