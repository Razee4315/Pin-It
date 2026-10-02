#include "sessionwatcher.h"

#include <windows.h>

namespace {

const wchar_t kWindowClass[] = L"PinItSessionWatcher";

LRESULT CALLBACK sessionWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto *self = reinterpret_cast<SessionWatcher *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) {
        if (message == WM_QUERYENDSESSION) {
            emit self->sessionEnding();
            return TRUE;   // PinIt never objects to a shutdown
        }
        if (message == WM_ENDSESSION && wParam == FALSE)
            emit self->sessionEndCancelled();
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace

SessionWatcher::SessionWatcher(QObject *parent)
    : QObject(parent)
{
    WNDCLASSW wc = {};
    wc.lpfnWndProc = sessionWindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWindowClass;
    RegisterClassW(&wc);

    // A top-level window (not message-only: those don't get session messages)
    // that is simply never shown.
    HWND hwnd = CreateWindowExW(0, kWindowClass, L"", WS_OVERLAPPED, 0, 0, 0, 0, nullptr,
                                nullptr, wc.hInstance, nullptr);
    if (hwnd)
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    m_window = hwnd;
}

SessionWatcher::~SessionWatcher()
{
    if (m_window)
        DestroyWindow(static_cast<HWND>(m_window));
    UnregisterClassW(kWindowClass, GetModuleHandleW(nullptr));
}
