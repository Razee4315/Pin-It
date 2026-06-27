#include "shortcuts.h"

#include <QStringList>

#include <windows.h>

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
            // The key token. Map the common Tauri key codes we use.
            if (token.startsWith("Key") && token.size() == 4) {
                vk = token.at(3).toUpper().unicode();          // KeyT -> 'T'
            } else if (token.startsWith("Digit") && token.size() == 6) {
                vk = token.at(5).unicode();                    // Digit5 -> '5'
            } else if (lower == "equal" || token == "=") {
                vk = VK_OEM_PLUS;
            } else if (lower == "minus" || token == "-") {
                vk = VK_OEM_MINUS;
            } else if (token.size() == 1) {
                vk = token.at(0).toUpper().unicode();
            } else {
                return false;   // unknown key token
            }
            haveKey = true;
        }
    }
    return haveKey && vk != 0;
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

QString build(bool win, bool ctrl, bool alt, bool shift, const QString &key)
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

} // namespace shortcuts
