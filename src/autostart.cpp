#include "autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
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
    if (!run.contains(kValueName))
        return false;

    // The stored command is "<exe>" --minimized; take the quoted path.
    const QString stored = run.value(kValueName).toString();
    const QString target = stored.section(QLatin1Char('"'), 1, 1);
    if (target.isEmpty() || QFile::exists(target))
        return false;

    run.setValue(kValueName, command());
    return true;
}

void setRegistryKeyForTesting(const QString &key)
{
    registryKey() = key;
}

} // namespace autostart
