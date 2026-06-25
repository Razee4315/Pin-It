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

} // namespace shortcuts
