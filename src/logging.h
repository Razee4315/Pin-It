#pragma once
//
// logging — route Qt's qDebug/qInfo/qWarning/qCritical to a log file at
// %LOCALAPPDATA%\PinIt\pinit.log so user-reported issues can be diagnosed.
// Lightweight: a single appended text file with simple size-based rotation.
//
namespace logging {

// Install the file message handler. Call once, early in main().
void init();

} // namespace logging
