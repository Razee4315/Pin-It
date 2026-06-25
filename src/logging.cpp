#include "logging.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QTextStream>

namespace {

QString g_logPath;
QMutex  g_mutex;

const char *levelTag(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:    return "D";
    case QtInfoMsg:     return "I";
    case QtWarningMsg:  return "W";
    case QtCriticalMsg: return "E";
    case QtFatalMsg:    return "F";
    }
    return "?";
}

void messageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    QMutexLocker lock(&g_mutex);
    QFile f(g_logPath);
    if (!f.open(QIODevice::Append | QIODevice::Text))
        return;
    QTextStream(&f) << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")
                    << " [" << levelTag(type) << "] " << msg << '\n';
}

} // namespace

namespace logging {

void init()
{
    QString base = qEnvironmentVariable("LOCALAPPDATA");
    if (base.isEmpty())
        base = QDir::homePath();
    const QString dir = QDir(base).filePath(QStringLiteral("PinIt"));
    QDir().mkpath(dir);
    g_logPath = QDir(dir).filePath(QStringLiteral("pinit.log"));

    // Simple rotation: once the log passes ~512 KB, keep one .old copy.
    constexpr qint64 kMaxBytes = 512 * 1024;
    if (QFileInfo(g_logPath).size() > kMaxBytes) {
        QFile::remove(g_logPath + ".old");
        QFile::rename(g_logPath, g_logPath + ".old");
    }

    qInstallMessageHandler(messageHandler);
}

} // namespace logging
