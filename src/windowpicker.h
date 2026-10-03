#pragma once
//
// WindowPicker — the "Pin a window…" dialog: a searchable list of the open
// application windows, for pinning one without having to focus it first.
//
#include <QDialog>

#include "winpin.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

class WindowPicker : public QDialog
{
    Q_OBJECT
public:
    explicit WindowPicker(const QVector<winpin::PinnableWindow> &windows,
                          QWidget *parent = nullptr);

    // The chosen window, or 0 if the dialog was cancelled.
    intptr_t selectedWindow() const;

protected:
    // Lets Up/Down move through the list while the cursor stays in the search box.
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyFilter(const QString &text);
    void updateState();

    QLineEdit   *m_search = nullptr;
    QListWidget *m_list = nullptr;
    QLabel      *m_emptyLabel = nullptr;
    QPushButton *m_pinButton = nullptr;
};
