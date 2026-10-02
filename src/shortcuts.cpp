#include "shortcuts.h"

#include <QStringList>

#include <windows.h>

namespace {

// Keys with a name rather than a letter or digit: the Tauri token stored in
// pinned.json, the label shown to the user, and the Win32 virtual-key code.
struct NamedKey {
    const char *token;
    const char *label;
    unsigned    vk;
};

const NamedKey kNamedKeys[] = {
    {"Equal",        "=",         VK_OEM_PLUS},
    {"Minus",        "-",         VK_OEM_MINUS},
    {"Comma",        ",",         VK_OEM_COMMA},
    {"Period",       ".",         VK_OEM_PERIOD},
    {"Slash",        "/",         VK_OEM_2},
    {"Semicolon",    ";",         VK_OEM_1},
    {"Quote",        "'",         VK_OEM_7},
    {"BracketLeft",  "[",         VK_OEM_4},
    {"BracketRight", "]",         VK_OEM_6},
    {"Backslash",    "\\",        VK_OEM_5},
    {"Backquote",    "`",         VK_OEM_3},
    {"Space",        "Space",     VK_SPACE},
    {"ArrowUp",      "Up",        VK_UP},
    {"ArrowDown",    "Down",      VK_DOWN},
    {"ArrowLeft",    "Left",      VK_LEFT},
    {"ArrowRight",   "Right",     VK_RIGHT},
    {"Home",         "Home",      VK_HOME},
    {"End",          "End",       VK_END},
    {"PageUp",       "Page Up",   VK_PRIOR},
    {"PageDown",     "Page Down", VK_NEXT},
    {"Insert",       "Insert",    VK_INSERT},
    {"Delete",       "Delete",    VK_DELETE},
};

constexpr int kFunctionKeyCount = 24;   // F1..F24

// "F1".."F24" -> 1..24, anything else -> 0.
int functionKeyNumber(const QString &token)
{
    if (token.size() < 2 || token.size() > 3 || token.at(0).toUpper() != QLatin1Char('F'))
        return 0;
    bool ok = false;
    const int n = token.mid(1).toInt(&ok);
    return (ok && n >= 1 && n <= kFunctionKeyCount) ? n : 0;
}

const NamedKey *namedKeyByToken(const QString &token)
{
    for (const NamedKey &key : kNamedKeys) {
        if (token.compare(QLatin1String(key.token), Qt::CaseInsensitive) == 0)
            return &key;
    }
    return nullptr;
}

const NamedKey *namedKeyByLabel(const QString &label)
{
    for (const NamedKey &key : kNamedKeys) {
        if (label.compare(QLatin1String(key.label), Qt::CaseInsensitive) == 0)
            return &key;
    }
    return nullptr;
}

// Resolve the key part of a shortcut ("KeyT", "Digit5", "F9", "Equal"…).
bool keyFromToken(const QString &token, unsigned &vk)
{
    if (token.startsWith(QLatin1String("Key")) && token.size() == 4 && token.at(3).isLetter()) {
        vk = token.at(3).toUpper().unicode();          // KeyT -> 'T'
        return true;
    }
    if (token.startsWith(QLatin1String("Digit")) && token.size() == 6 && token.at(5).isDigit()) {
        vk = token.at(5).unicode();                    // Digit5 -> '5'
        return true;
    }
    if (const int n = functionKeyNumber(token)) {
        vk = VK_F1 + (n - 1);
        return true;
    }
    if (const NamedKey *key = namedKeyByToken(token)) {
        vk = key->vk;
        return true;
    }
    // Hand-written configs: a bare character such as "T", "5", "=" or "-".
    if (token.size() == 1) {
        if (const NamedKey *key = namedKeyByLabel(token)) {
            vk = key->vk;
            return true;
        }
        if (token.at(0).isLetterOrNumber()) {
            vk = token.at(0).toUpper().unicode();
            return true;
        }
    }
    return false;
}

// What the key part of a shortcut looks like to the user.
QString labelForToken(const QString &token)
{
    if (token.startsWith(QLatin1String("Key")) && token.size() == 4)
        return token.mid(3).toUpper();
    if (token.startsWith(QLatin1String("Digit")) && token.size() == 6)
        return token.mid(5);
    if (functionKeyNumber(token))
        return token.toUpper();
    if (const NamedKey *key = namedKeyByToken(token))
        return QLatin1String(key->label);
    return token;   // unknown: show it as written
}

// Inverse of labelForToken. A label we don't know is passed through untouched,
// so a hand-edited key the editor can't offer survives a round trip.
QString tokenForLabel(const QString &label)
{
    if (label.size() == 1 && label.at(0).isLetter())
        return QStringLiteral("Key") + label.toUpper();
    if (label.size() == 1 && label.at(0).isDigit())
        return QStringLiteral("Digit") + label;
    if (functionKeyNumber(label))
        return label.toUpper();
    if (const NamedKey *key = namedKeyByLabel(label))
        return QLatin1String(key->token);
    return label;
}

} // namespace

namespace shortcuts {

bool parse(const QString &s, unsigned &mods, unsigned &vk)
{
    mods = 0;
    vk = 0;
    bool haveKey = false;

    const QStringList parts = s.split(QLatin1Char('+'), Qt::SkipEmptyParts);
    for (const QString &tokenRaw : parts) {
        const QString token = tokenRaw.trimmed();
        const QString lower = token.toLower();

        if (lower == "super" || lower == "meta" || lower == "win" || lower == "cmd") {
            mods |= MOD_WIN;
        } else if (lower == "ctrl" || lower == "control") {
            mods |= MOD_CONTROL;
        } else if (lower == "alt") {
            mods |= MOD_ALT;
        } else if (lower == "shift") {
            mods |= MOD_SHIFT;
        } else {
            if (!keyFromToken(token, vk))
                return false;   // unknown key token
            haveKey = true;
        }
    }
    return haveKey && vk != 0;
}

bool hasSafeModifier(unsigned mods)
{
    return (mods & (MOD_WIN | MOD_CONTROL | MOD_ALT)) != 0;
}

QStringList displayTokens(const QString &s)
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
        else
            out << labelForToken(t);
    }
    return out;
}

QStringList keyLabels()
{
    QStringList keys;
    for (char c = 'A'; c <= 'Z'; ++c)
        keys << QString(QLatin1Char(c));
    for (char c = '0'; c <= '9'; ++c)
        keys << QString(QLatin1Char(c));
    for (int n = 1; n <= kFunctionKeyCount; ++n)
        keys << QStringLiteral("F%1").arg(n);
    for (const NamedKey &key : kNamedKeys)
        keys << QLatin1String(key.label);
    return keys;
}

QString build(bool win, bool ctrl, bool alt, bool shift, const QString &keyLabel)
{
    QStringList parts;
    if (win)   parts << QStringLiteral("super");
    if (ctrl)  parts << QStringLiteral("ctrl");
    if (alt)   parts << QStringLiteral("alt");
    if (shift) parts << QStringLiteral("shift");
    parts << tokenForLabel(keyLabel);
    return parts.join(QLatin1Char('+'));
}

} // namespace shortcuts
