#pragma once
//
// SessionWatcher — notices when Windows calls off a logoff / shutdown.
//
// Qt reports the *start* of a session end (QGuiApplication::commitDataRequest)
// but not its cancellation. Windows announces that with WM_ENDSESSION carrying
// wParam == FALSE, which this native event filter turns into a signal.
//
#include <QObject>
#include <QAbstractNativeEventFilter>

class SessionWatcher : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit SessionWatcher(QObject *parent = nullptr);

    bool nativeEventFilter(const QByteArray &eventType, void *message,
                           qintptr *result) override;

signals:
    void sessionEndCancelled();
};
