#pragma once
//
// PinRow — one pinned window in the main list:
//   [avatar] [title / process] [opacity slider] [%] [unpin]
//
// Rows are kept alive and updated in place while their window stays pinned,
// so a slider drag or keyboard focus survives other pins coming and going.
//
#include <QFrame>

#include "pinmanager.h"

class QLabel;
class QSlider;

// Console apps (PowerShell, cmd) set their window title to a full path.
// Show just the final component so lists stay readable.
QString displayTitle(const QString &title);

// A saved pin whose window is not open yet: [avatar] [title / process] [x].
class PendingRow : public QFrame
{
    Q_OBJECT
public:
    explicit PendingRow(const persistence::SavedPin &pin, QWidget *parent = nullptr);

signals:
    void forgetRequested();
};

class PinRow : public QFrame
{
    Q_OBJECT
public:
    explicit PinRow(const PinnedWindow &window, QWidget *parent = nullptr);

    // Reflect an opacity change made elsewhere (the hotkeys) without echoing
    // it back as a request.
    void setOpacity(int percent);
    void setTitle(const QString &title);

signals:
    void opacityRequested(int percent);
    void unpinRequested();

private:
    QLabel  *m_title = nullptr;
    QSlider *m_slider = nullptr;
    QLabel  *m_percent = nullptr;
};
