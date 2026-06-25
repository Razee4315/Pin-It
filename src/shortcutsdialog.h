#pragma once
//
// ShortcutsDialog — lets the user rebind PinIt's four global shortcuts.
//
// Uses modifier checkboxes + a key dropdown instead of live key capture: on
// Windows the Win/Super key is swallowed by the OS and can't be captured
// reliably from a widget, so a structured editor is both robust and clear.
//
#include <QDialog>

#include "persistence.h"

class QCheckBox;
class QComboBox;
class QGridLayout;

class ShortcutsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ShortcutsDialog(const persistence::ShortcutConfig &cfg, QWidget *parent = nullptr);

    // The edited config (valid only after the dialog is accepted).
    persistence::ShortcutConfig config() const { return m_config; }

private:
    struct Row {
        QCheckBox *win = nullptr;
        QCheckBox *ctrl = nullptr;
        QCheckBox *alt = nullptr;
        QCheckBox *shift = nullptr;
        QComboBox *key = nullptr;
    };

    Row addRow(QGridLayout *grid, int r, const QString &label, const QString &shortcut);
    void accept() override;   // validate, then build m_config

    Row m_togglePin;
    Row m_opacityUp;
    Row m_opacityDown;
    Row m_toggleWindow;

    persistence::ShortcutConfig m_config;
};
