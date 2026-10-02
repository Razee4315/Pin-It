#include "sessionwatcher.h"

#include <windows.h>

SessionWatcher::SessionWatcher(QObject *parent)
    : QObject(parent)
{
}

bool SessionWatcher::nativeEventFilter(const QByteArray &eventType, void *message,
                                       qintptr *result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(result);

    const MSG *msg = static_cast<const MSG *>(message);
    if (msg->message == WM_ENDSESSION && msg->wParam == FALSE)
        emit sessionEndCancelled();
    return false;   // never consume: Qt still needs to see the message
}
