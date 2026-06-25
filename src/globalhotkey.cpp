#include "globalhotkey.h"
#include "shortcuts.h"

#include <windows.h>

namespace {

// Hotkey ids passed to RegisterHotKey; also matched in the event filter.
enum HotkeyId {
    IdTogglePin    = 1,
    IdOpacityUp    = 2,
    IdOpacityDown  = 3,
    IdToggleWindow = 4,
};

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
    unsigned mods = 0, vk = 0;
    if (!shortcuts::parse(shortcut, mods, vk))
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
