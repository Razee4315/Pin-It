#include "globalhotkey.h"

#include <windows.h>

namespace {

// Hotkey ids passed to RegisterHotKey; also matched in the event filter.
enum HotkeyId {
    IdTogglePin    = 1,
    IdOpacityUp    = 2,
    IdOpacityDown  = 3,
    IdToggleWindow = 4,
};

// Parse a Tauri-style shortcut ("super+ctrl+KeyT") into Win32 modifiers + vk.
// Returns false if any token is unrecognised.
bool parseShortcut(const QString &s, UINT *mods, UINT *vk)
{
    *mods = 0;
    *vk = 0;
    bool haveKey = false;

    const QStringList parts = s.split(QLatin1Char('+'), Qt::SkipEmptyParts);
    for (QString tokenRaw : parts) {
        const QString token = tokenRaw.trimmed();
        const QString lower = token.toLower();

        if (lower == "super" || lower == "meta" || lower == "win" || lower == "cmd") {
            *mods |= MOD_WIN;
        } else if (lower == "ctrl" || lower == "control") {
            *mods |= MOD_CONTROL;
        } else if (lower == "alt") {
            *mods |= MOD_ALT;
        } else if (lower == "shift") {
            *mods |= MOD_SHIFT;
        } else {
            // It's the key. Map the common Tauri key codes we use.
            if (token.startsWith("Key") && token.size() == 4) {
                *vk = token.at(3).toUpper().unicode();          // KeyT -> 'T'
            } else if (token.startsWith("Digit") && token.size() == 6) {
                *vk = token.at(5).unicode();                    // Digit5 -> '5'
            } else if (lower == "equal" || token == "=") {
                *vk = VK_OEM_PLUS;
            } else if (lower == "minus" || token == "-") {
                *vk = VK_OEM_MINUS;
            } else if (token.size() == 1) {
                *vk = token.at(0).toUpper().unicode();
            } else {
                return false;   // unknown key token
            }
            haveKey = true;
        }
    }
    return haveKey && *vk != 0;
}

} // namespace

GlobalHotkeyManager::GlobalHotkeyManager(QObject *parent)
    : QObject(parent)
{
}

GlobalHotkeyManager::~GlobalHotkeyManager()
{
    unregisterAll();
}

bool GlobalHotkeyManager::registerOne(int id, const QString &shortcut)
{
    UINT mods = 0, vk = 0;
    if (!parseShortcut(shortcut, &mods, &vk))
        return false;

    // MOD_NOREPEAT: holding the keys fires once, not a stream.
    return RegisterHotKey(nullptr, id, mods | MOD_NOREPEAT, vk) != FALSE;
}

bool GlobalHotkeyManager::registerAll(const persistence::ShortcutConfig &c)
{
    unregisterAll();
    m_failed.clear();
    m_anyRegistered = false;

    struct Entry { int id; const char *label; QString shortcut; };
    const Entry entries[] = {
        { IdTogglePin,    "Pin/Unpin", c.togglePin },
        { IdOpacityUp,    "Opacity +", c.opacityUp },
        { IdOpacityDown,  "Opacity -", c.opacityDown },
        { IdToggleWindow, "Show/Hide", c.toggleWindow },
    };

    for (const Entry &e : entries) {
        if (registerOne(e.id, e.shortcut))
            m_anyRegistered = true;
        else
            m_failed << QString::fromLatin1(e.label);
    }
    return m_anyRegistered;
}

void GlobalHotkeyManager::unregisterAll()
{
    for (int id : { IdTogglePin, IdOpacityUp, IdOpacityDown, IdToggleWindow })
        UnregisterHotKey(nullptr, id);
}

bool GlobalHotkeyManager::nativeEventFilter(const QByteArray &eventType,
                                            void *message, qintptr *result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(result);

    MSG *msg = static_cast<MSG *>(message);
    if (msg->message != WM_HOTKEY)
        return false;

    switch (msg->wParam) {
    case IdTogglePin:    emit togglePin();    return true;
    case IdOpacityUp:    emit opacityUp();    return true;
    case IdOpacityDown:  emit opacityDown();  return true;
    case IdToggleWindow: emit toggleWindow(); return true;
    default:             return false;
    }
}
