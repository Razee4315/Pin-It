#pragma once
//
// SessionWatcher — tells PinIt when Windows starts to log off / shut down /
// restart, and when that is called off again.
//
// It owns a hidden top-level window of its own, so it works even when PinIt
// sits in the tray and has never shown a window (which is how it starts at
// login). Windows sends every top-level window WM_QUERYENDSESSION when a
// session end begins, and WM_ENDSESSION with wParam == FALSE if it is
// cancelled (another app blocked it, or the user changed their mind).
//
#include <QObject>

class SessionWatcher : public QObject
{
    Q_OBJECT
public:
    explicit SessionWatcher(QObject *parent = nullptr);
    ~SessionWatcher() override;

signals:
    void sessionEnding();
    void sessionEndCancelled();

private:
    void *m_window = nullptr;   // HWND
};
