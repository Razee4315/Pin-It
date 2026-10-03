#pragma once
//
// SingleInstance — makes sure only one PinIt runs per user session, and lets
// a second launch ask the running one to show its window.
//
// Plain Win32: a named mutex decides who is first, and a hidden message-only
// window receives the "show yourself" request. (This used to be a local
// socket, which pulled the whole Qt Network module into the app for one
// message.) The mutex doubles as the installer's AppMutex, so setup and
// uninstall can tell that PinIt is running.
//
#include <QObject>
#include <QString>

class SingleInstance : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstance(const QString &name, QObject *parent = nullptr);
    ~SingleInstance() override;

    // True for the first instance; it then listens for requests from later ones.
    bool isPrimary() const { return m_primary; }

    // Called by a later instance: ask the primary to show its window, and give
    // it permission to come to the front (Windows only lets the process the
    // user is interacting with — this one — hand over the foreground).
    // Returns false if the primary could not be reached.
    bool askPrimaryToShow() const;

signals:
    void showRequested();

private:
    QString m_name;
    void   *m_mutex = nullptr;    // HANDLE
    void   *m_window = nullptr;   // HWND of the primary's message-only window
    bool    m_primary = false;
};
