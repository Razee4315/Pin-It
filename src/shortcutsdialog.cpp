#include "shortcutsdialog.h"
#include "shortcuts.h"

#include <QAccessible>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QStringList>
#include <QVBoxLayout>

ShortcutsDialog::ShortcutsDialog(const persistence::ShortcutConfig &cfg, ApplyFn apply,
                                 QWidget *parent)
    : QDialog(parent)
    , m_apply(std::move(apply))
    , m_config(cfg)
{
    setWindowTitle(tr("Edit shortcuts"));

    auto *root = new QVBoxLayout(this);
    root->addWidget(new QLabel(tr("Pick the modifiers and key for each action.\n"
                                  "Each shortcut needs Win, Ctrl or Alt."), this));

    auto *grid = new QGridLayout;
    grid->addWidget(new QLabel(tr("Action"), this),  0, 0);
    grid->addWidget(new QLabel(QStringLiteral("Win"), this),   0, 1);
    grid->addWidget(new QLabel(QStringLiteral("Ctrl"), this),  0, 2);
    grid->addWidget(new QLabel(QStringLiteral("Alt"), this),   0, 3);
    grid->addWidget(new QLabel(QStringLiteral("Shift"), this), 0, 4);
    grid->addWidget(new QLabel(tr("Key"), this),     0, 5);

    m_togglePin    = addRow(grid, 1, tr("Pin / unpin"));
    m_opacityUp    = addRow(grid, 2, tr("Opacity +"));
    m_opacityDown  = addRow(grid, 3, tr("Opacity -"));
    m_toggleWindow = addRow(grid, 4, tr("Show / hide"));
    root->addLayout(grid);
    setConfig(cfg);

    // Problems are reported here, next to the controls, instead of in a
    // message box that has to be dismissed first.
    m_error = new QLabel(this);
    m_error->setProperty("role", "warning");
    m_error->setWordWrap(true);
    m_error->hide();
    root->addWidget(m_error);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel
                                             | QDialogButtonBox::RestoreDefaults, this);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &ShortcutsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this,
            [this]() {
                // Only fills in the form; nothing changes until OK.
                setConfig(persistence::ShortcutConfig());
                m_error->hide();
            });
}

ShortcutsDialog::Row ShortcutsDialog::addRow(QGridLayout *grid, int r, const QString &label)
{
    Row row;
    grid->addWidget(new QLabel(label, this), r, 0);
    row.win   = new QCheckBox(this);
    row.ctrl  = new QCheckBox(this);
    row.alt   = new QCheckBox(this);
    row.shift = new QCheckBox(this);
    row.key   = new QComboBox(this);
    row.key->addItems(shortcuts::keyLabels());

    // The column headings are separate labels, so each control carries the
    // full "action: what it is" name for screen readers.
    row.win->setAccessibleName(tr("%1: Win").arg(label));
    row.ctrl->setAccessibleName(tr("%1: Ctrl").arg(label));
    row.alt->setAccessibleName(tr("%1: Alt").arg(label));
    row.shift->setAccessibleName(tr("%1: Shift").arg(label));
    row.key->setAccessibleName(tr("%1: key").arg(label));

    grid->addWidget(row.win,   r, 1, Qt::AlignCenter);
    grid->addWidget(row.ctrl,  r, 2, Qt::AlignCenter);
    grid->addWidget(row.alt,   r, 3, Qt::AlignCenter);
    grid->addWidget(row.shift, r, 4, Qt::AlignCenter);
    grid->addWidget(row.key,   r, 5);
    return row;
}

void ShortcutsDialog::setRow(const Row &row, const QString &shortcut)
{
    const QStringList tokens = shortcuts::displayTokens(shortcut);

    row.win->setChecked(tokens.contains(QStringLiteral("Win")));
    row.ctrl->setChecked(tokens.contains(QStringLiteral("Ctrl")));
    row.alt->setChecked(tokens.contains(QStringLiteral("Alt")));
    row.shift->setChecked(tokens.contains(QStringLiteral("Shift")));
    if (!tokens.isEmpty()) {
        // A key from a hand-edited or older config that the editor doesn't
        // offer is listed as-is rather than silently replaced by "A".
        int idx = row.key->findText(tokens.last());
        if (idx < 0) {
            row.key->insertItem(0, tokens.last());
            idx = 0;
        }
        row.key->setCurrentIndex(idx);
    }
}

void ShortcutsDialog::setConfig(const persistence::ShortcutConfig &cfg)
{
    setRow(m_togglePin, cfg.togglePin);
    setRow(m_opacityUp, cfg.opacityUp);
    setRow(m_opacityDown, cfg.opacityDown);
    setRow(m_toggleWindow, cfg.toggleWindow);
}

void ShortcutsDialog::showError(const QString &message)
{
    m_error->setText(message);
    m_error->show();
    adjustSize();

    QAccessibleEvent alert(m_error, QAccessible::Alert);
    QAccessible::updateAccessibility(&alert);
}

void ShortcutsDialog::accept()
{
    auto build = [](const Row &row) {
        return shortcuts::build(row.win->isChecked(), row.ctrl->isChecked(),
                                row.alt->isChecked(), row.shift->isChecked(),
                                row.key->currentText());
    };
    // Shift alone is not enough: Shift+A as a global hotkey would swallow
    // every capital A typed anywhere.
    auto hasSafeModifier = [](const Row &row) {
        return row.win->isChecked() || row.ctrl->isChecked() || row.alt->isChecked();
    };

    const Row rows[] = {m_togglePin, m_opacityUp, m_opacityDown, m_toggleWindow};
    for (const Row &row : rows) {
        if (!hasSafeModifier(row)) {
            showError(tr("Each shortcut needs Win, Ctrl or Alt. Shift on its own would "
                         "capture ordinary typing."));
            return;
        }
    }

    persistence::ShortcutConfig cfg;
    cfg.togglePin    = build(m_togglePin);
    cfg.opacityUp    = build(m_opacityUp);
    cfg.opacityDown  = build(m_opacityDown);
    cfg.toggleWindow = build(m_toggleWindow);

    // No two actions may share a binding.
    const QStringList all = {cfg.togglePin, cfg.opacityUp, cfg.opacityDown, cfg.toggleWindow};
    QSet<QString> seen;
    for (const QString &s : all) {
        if (seen.contains(s)) {
            showError(tr("Two actions can't use the same shortcut."));
            return;
        }
        seen.insert(s);
    }

    // The real test: will Windows register them? Another application may
    // already own a combination, which no rule above can know.
    if (m_apply) {
        const QStringList refused = m_apply(cfg);
        if (!refused.isEmpty()) {
            showError(tr("Windows refused the shortcut for: %1. Another app is probably "
                         "using it — choose different keys. Your previous shortcuts are "
                         "still active.")
                          .arg(refused.join(QStringLiteral(", "))));
            return;
        }
    }

    m_config = cfg;
    QDialog::accept();
}
