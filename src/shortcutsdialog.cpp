#include "shortcutsdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QStringList>
#include <QSet>

namespace {

// Display tokens for a Tauri-style shortcut, e.g. "super+ctrl+KeyT"
// -> ["Win", "Ctrl", "T"].
QStringList tokenize(const QString &s)
{
    QStringList out;
    for (const QString &raw : s.split(QLatin1Char('+'), Qt::SkipEmptyParts)) {
        const QString t = raw.trimmed();
        const QString lo = t.toLower();
        if (lo == "super" || lo == "meta" || lo == "win" || lo == "cmd")
            out << QStringLiteral("Win");
        else if (lo == "ctrl" || lo == "control")
            out << QStringLiteral("Ctrl");
        else if (lo == "alt")
            out << QStringLiteral("Alt");
        else if (lo == "shift")
            out << QStringLiteral("Shift");
        else if (t.startsWith("Key") && t.size() == 4)
            out << t.mid(3).toUpper();
        else if (t.startsWith("Digit") && t.size() == 6)
            out << t.mid(5);
        else if (lo == "equal")
            out << QStringLiteral("=");
        else if (lo == "minus")
            out << QStringLiteral("-");
        else
            out << t;
    }
    return out;
}

// Build a Tauri-style shortcut string from a row's controls.
QString buildShortcut(bool win, bool ctrl, bool alt, bool shift, const QString &key)
{
    QStringList parts;
    if (win)   parts << QStringLiteral("super");
    if (ctrl)  parts << QStringLiteral("ctrl");
    if (alt)   parts << QStringLiteral("alt");
    if (shift) parts << QStringLiteral("shift");

    QString tok;
    if (key.size() == 1 && key.at(0).isLetter())
        tok = QStringLiteral("Key") + key.toUpper();
    else if (key.size() == 1 && key.at(0).isDigit())
        tok = QStringLiteral("Digit") + key;
    else if (key == QLatin1String("="))
        tok = QStringLiteral("Equal");
    else if (key == QLatin1String("-"))
        tok = QStringLiteral("Minus");
    else
        tok = key;

    parts << tok;
    return parts.join(QLatin1Char('+'));
}

QStringList keyChoices()
{
    QStringList keys;
    for (char c = 'A'; c <= 'Z'; ++c)
        keys << QString(QChar(c));
    for (char c = '0'; c <= '9'; ++c)
        keys << QString(QChar(c));
    keys << QStringLiteral("=") << QStringLiteral("-");
    return keys;
}

} // namespace

ShortcutsDialog::ShortcutsDialog(const persistence::ShortcutConfig &cfg, QWidget *parent)
    : QDialog(parent)
    , m_config(cfg)
{
    setWindowTitle(tr("Edit shortcuts"));

    auto *root = new QVBoxLayout(this);
    root->addWidget(new QLabel(tr("Pick the modifiers and key for each action.\n"
                                  "Each shortcut needs at least one modifier."), this));

    auto *grid = new QGridLayout;
    grid->addWidget(new QLabel(tr("Action"), this),  0, 0);
    grid->addWidget(new QLabel(QStringLiteral("Win"), this),   0, 1);
    grid->addWidget(new QLabel(QStringLiteral("Ctrl"), this),  0, 2);
    grid->addWidget(new QLabel(QStringLiteral("Alt"), this),   0, 3);
    grid->addWidget(new QLabel(QStringLiteral("Shift"), this), 0, 4);
    grid->addWidget(new QLabel(tr("Key"), this),     0, 5);

    m_togglePin    = addRow(grid, 1, tr("Pin / unpin"),  cfg.togglePin);
    m_opacityUp    = addRow(grid, 2, tr("Opacity +"),    cfg.opacityUp);
    m_opacityDown  = addRow(grid, 3, tr("Opacity -"),    cfg.opacityDown);
    m_toggleWindow = addRow(grid, 4, tr("Show / hide"),  cfg.toggleWindow);
    root->addLayout(grid);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &ShortcutsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

ShortcutsDialog::Row ShortcutsDialog::addRow(QGridLayout *grid, int r,
                                             const QString &label, const QString &shortcut)
{
    const QStringList tokens = tokenize(shortcut);

    Row row;
    grid->addWidget(new QLabel(label, this), r, 0);
    row.win   = new QCheckBox(this);
    row.ctrl  = new QCheckBox(this);
    row.alt   = new QCheckBox(this);
    row.shift = new QCheckBox(this);
    row.key   = new QComboBox(this);
    row.key->addItems(keyChoices());

    row.win->setChecked(tokens.contains(QStringLiteral("Win")));
    row.ctrl->setChecked(tokens.contains(QStringLiteral("Ctrl")));
    row.alt->setChecked(tokens.contains(QStringLiteral("Alt")));
    row.shift->setChecked(tokens.contains(QStringLiteral("Shift")));
    if (!tokens.isEmpty()) {
        const int idx = row.key->findText(tokens.last());
        if (idx >= 0)
            row.key->setCurrentIndex(idx);
    }

    grid->addWidget(row.win,   r, 1, Qt::AlignCenter);
    grid->addWidget(row.ctrl,  r, 2, Qt::AlignCenter);
    grid->addWidget(row.alt,   r, 3, Qt::AlignCenter);
    grid->addWidget(row.shift, r, 4, Qt::AlignCenter);
    grid->addWidget(row.key,   r, 5);
    return row;
}

void ShortcutsDialog::accept()
{
    auto build = [](const Row &row) {
        return buildShortcut(row.win->isChecked(), row.ctrl->isChecked(),
                             row.alt->isChecked(), row.shift->isChecked(),
                             row.key->currentText());
    };
    auto hasModifier = [](const Row &row) {
        return row.win->isChecked() || row.ctrl->isChecked()
               || row.alt->isChecked() || row.shift->isChecked();
    };

    const Row rows[] = {m_togglePin, m_opacityUp, m_opacityDown, m_toggleWindow};
    for (const Row &row : rows) {
        if (!hasModifier(row)) {
            QMessageBox::warning(this, tr("Invalid shortcut"),
                tr("Each shortcut needs at least one modifier (Win/Ctrl/Alt/Shift)."));
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
            QMessageBox::warning(this, tr("Duplicate shortcut"),
                tr("Two actions can't use the same shortcut."));
            return;
        }
        seen.insert(s);
    }

    m_config = cfg;
    QDialog::accept();
}
