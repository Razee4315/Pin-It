#pragma once
//
// ShortcutsDialog — lets the user rebind PinIt's four global shortcuts.
//
// Uses modifier checkboxes + a key dropdown instead of live key capture: on
// Windows the Win/Super key is swallowed by the OS and can't be captured
// reliably from a widget, so a structured editor is both robust and clear.
//
// Nothing is accepted until Windows has actually registered the new set: the
// dialog asks its `apply` callback to try it and stays open, saying which
// shortcut was refused, if another application already owns one.
//
#include <QDialog>

#include <functional>

#include "persistence.h"

class QCheckBox;
class QComboBox;
class QGridLayout;
class QLabel;

class ShortcutsDialog : public QDialog
{
    Q_OBJECT
public:
    // Tries to make `config` the live hotkeys. Returns the names of the
    // actions that could not be registered — empty means it is now in effect.
    // On failure the callback must leave the previous hotkeys active.
    using ApplyFn = std::function<QStringList(const persistence::ShortcutConfig &config)>;

    ShortcutsDialog(const persistence::ShortcutConfig &cfg, ApplyFn apply,
                    QWidget *parent = nullptr);

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

    Row addRow(QGridLayout *grid, int r, const QString &label);
    void setRow(const Row &row, const QString &shortcut);
    void setConfig(const persistence::ShortcutConfig &cfg);
    void showError(const QString &message);
    void accept() override;   // validate, apply, then build m_config

    Row m_togglePin;
    Row m_opacityUp;
    Row m_opacityDown;
    Row m_toggleWindow;
    QLabel *m_error = nullptr;

    ApplyFn m_apply;
    persistence::ShortcutConfig m_config;
};
