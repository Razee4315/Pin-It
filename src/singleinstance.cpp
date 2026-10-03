#include "singleinstance.h"

#include <windows.h>

namespace {

constexpr UINT kShowMessage = WM_APP + 1;

inline const wchar_t *wide(const QString &s)
{
    return reinterpret_cast<const wchar_t *>(s.utf16());
}

// The window class is named after the instance, so a later launch can find
// the primary's window with FindWindowEx.
QString windowClassName(const QString &name)
{
    return name + QStringLiteral("_Window");
}

LRESULT CALLBACK instanceWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == kShowMessage) {
        auto *self = reinterpret_cast<SingleInstance *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self)
            emit self->showRequested();
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace

SingleInstance::SingleInstance(const QString &name, QObject *parent)
    : QObject(parent)
    , m_name(name)
{
    // Session-local by default, so each logged-in user gets their own PinIt.
    m_mutex = CreateMutexW(nullptr, FALSE, wide(m_name));
    m_primary = (GetLastError() != ERROR_ALREADY_EXISTS);
    if (!m_primary)
        return;

    const QString className = windowClassName(m_name);
    WNDCLASSW wc = {};
    wc.lpfnWndProc = instanceWindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = wide(className);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, wide(className), nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE,
                                nullptr, wc.hInstance, nullptr);
    if (hwnd)
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    m_window = hwnd;
}

SingleInstance::~SingleInstance()
{
    if (m_window) {
        DestroyWindow(static_cast<HWND>(m_window));
        UnregisterClassW(wide(windowClassName(m_name)), GetModuleHandleW(nullptr));
    }
    if (m_mutex)
        CloseHandle(static_cast<HANDLE>(m_mutex));
}

bool SingleInstance::askPrimaryToShow() const
{
    HWND primary = FindWindowExW(HWND_MESSAGE, nullptr, wide(windowClassName(m_name)), nullptr);
    if (!primary)
        return false;

    DWORD pid = 0;
    GetWindowThreadProcessId(primary, &pid);
    AllowSetForegroundWindow(pid);
    return PostMessageW(primary, kShowMessage, 0, 0) != FALSE;
}
