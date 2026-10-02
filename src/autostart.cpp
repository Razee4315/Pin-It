#include "autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

namespace {

const QString kValueName = QStringLiteral("PinIt");

QString &registryKey()
{
    static QString key = QStringLiteral(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
    return key;
}

} // namespace

namespace autostart {

QString command()
{
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    // --minimized: when launched at login, start silently in the tray instead
    // of popping the window every boot.
    return QStringLiteral("\"%1\" --minimized").arg(exe);
}

bool isEnabled()
{
    const QSettings run(registryKey(), QSettings::NativeFormat);
    return run.contains(kValueName);
}

void setEnabled(bool enabled)
{
    QSettings run(registryKey(), QSettings::NativeFormat);
    if (enabled)
        run.setValue(kValueName, command());
    else
        run.remove(kValueName);
}

bool repairPath()
{
    QSettings run(registryKey(), QSettings::NativeFormat);
    if (!run.contains(kValueName) || run.value(kValueName).toString() == command())
        return false;
    run.setValue(kValueName, command());
    return true;
}

void setRegistryKeyForTesting(const QString &key)
{
    registryKey() = key;
}

} // namespace autostart
