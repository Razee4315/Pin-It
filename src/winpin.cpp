#include "winpin.h"

#include <windows.h>
#include <psapi.h>
#include <dwmapi.h>
#include <mmsystem.h>

#include <QFile>

#include <algorithm>

namespace {
inline HWND H(void *hwnd) { return reinterpret_cast<HWND>(hwnd); }

HWINEVENTHOOK g_foregroundHook = nullptr;
winpin::ForegroundCallback g_foregroundCallback = nullptr;
void *g_foregroundContext = nullptr;

void CALLBACK onForegroundEvent(HWINEVENTHOOK, DWORD event, HWND, LONG, LONG, DWORD, DWORD)
{
    if (event == EVENT_SYSTEM_FOREGROUND && g_foregroundCallback)
        g_foregroundCallback(g_foregroundContext);
}
} // namespace

namespace winpin {

int percentToAlpha(int percent)
{
    percent = std::clamp(percent, 0, 100);
    return (percent * 255 + 50) / 100;          // rounded
}

int alphaToPercent(int alpha)
{
    alpha = std::clamp(alpha, 0, 255);
    return (alpha * 100 + 127) / 255;           // rounded
}

QString windowTitle(void *hwnd)
{
    const int len = GetWindowTextLengthW(H(hwnd));
    if (len <= 0)
        return QString();

    QVector<wchar_t> buf(len + 1);
    const int copied = GetWindowTextW(H(hwnd), buf.data(), len + 1);
    if (copied <= 0)
        return QString();

    return QString::fromWCharArray(buf.data(), copied);
}

QString processName(void *hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(H(hwnd), &pid);
    if (pid == 0)
        return QString();

    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc)
        return QString();

    wchar_t buf[MAX_PATH] = {0};
    DWORD size = MAX_PATH;
    QString result;
    if (QueryFullProcessImageNameW(proc, 0, buf, &size)) {
        const QString full = QString::fromWCharArray(buf, size);
        const int slash = full.lastIndexOf(QLatin1Char('\\'));
        result = (slash >= 0) ? full.mid(slash + 1) : full;
    }
    CloseHandle(proc);
    return result;
}

void *foregroundWindow()
{
    return reinterpret_cast<void *>(GetForegroundWindow());
}

bool isValidWindow(void *hwnd)
{
    return IsWindow(H(hwnd)) != FALSE;
}

bool isTopmost(void *hwnd)
{
    const LONG ex = GetWindowLongW(H(hwnd), GWL_EXSTYLE);
    return (static_cast<DWORD>(ex) & WS_EX_TOPMOST) != 0;
}

bool isLayered(void *hwnd)
{
    const LONG ex = GetWindowLongW(H(hwnd), GWL_EXSTYLE);
    return (static_cast<DWORD>(ex) & WS_EX_LAYERED) != 0;
}

QString className(void *hwnd)
{
    wchar_t buf[256] = {0};
    const int len = GetClassNameW(H(hwnd), buf, 256);
    return QString::fromWCharArray(buf, len);
}

QRect frameRect(void *hwnd)
{
    RECT r = {};
    if (FAILED(DwmGetWindowAttribute(H(hwnd), DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(r)))
        && !GetWindowRect(H(hwnd), &r))
        return QRect();
    return QRect(QPoint(r.left, r.top), QPoint(r.right - 1, r.bottom - 1));
}

bool isShellClass(const QString &className)
{
    static const char *const kShellClasses[] = {
        "Progman",                              // desktop
        "WorkerW",                              // desktop (wallpaper host)
        "Shell_TrayWnd",                        // taskbar
        "Shell_SecondaryTrayWnd",               // taskbar on other monitors
        "NotifyIconOverflowWindow",             // tray overflow (Windows 10)
        "TopLevelWindowForOverflowXamlIsland",  // tray overflow (Windows 11)
        "Windows.UI.Core.CoreWindow",           // Start, Search, Action Center
        "XamlExplorerHostIslandWindow",         // Task View / Alt+Tab (Windows 11)
        "MultitaskingViewFrame",                // Task View / Alt+Tab (Windows 10)
        "TaskListThumbnailWnd",                 // taskbar thumbnails
        "ForegroundStaging",                    // shell focus hand-off helper
    };
    for (const char *shellClass : kShellClasses) {
        if (className == QLatin1String(shellClass))
            return true;
    }
    return false;
}

bool isPinnable(void *hwnd)
{
    if (!isValidWindow(hwnd))
        return false;

    DWORD pid = 0;
    GetWindowThreadProcessId(H(hwnd), &pid);
    if (pid == GetCurrentProcessId())
        return false;   // PinIt's own windows

    return !isShellClass(className(hwnd));
}

bool applyTopmost(void *hwnd)
{
    return SetWindowPos(H(hwnd), HWND_TOPMOST, 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE) != FALSE;
}

bool removeTopmost(void *hwnd)
{
    return SetWindowPos(H(hwnd), HWND_NOTOPMOST, 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE) != FALSE;
}

bool setOpacityPercent(void *hwnd, int percent)
{
    percent = std::clamp(percent, kMinOpacity, kMaxOpacity);

    const LONG ex = GetWindowLongW(H(hwnd), GWL_EXSTYLE);
    if ((static_cast<DWORD>(ex) & WS_EX_LAYERED) == 0)
        SetWindowLongW(H(hwnd), GWL_EXSTYLE, ex | WS_EX_LAYERED);

    const BYTE alpha = static_cast<BYTE>(percentToAlpha(percent));
    return SetLayeredWindowAttributes(H(hwnd), RGB(0, 0, 0), alpha, LWA_ALPHA) != FALSE;
}

int opacityPercent(void *hwnd)
{
    const LONG ex = GetWindowLongW(H(hwnd), GWL_EXSTYLE);
    if ((static_cast<DWORD>(ex) & WS_EX_LAYERED) == 0)
        return 100;

    COLORREF color = 0;
    BYTE alpha = 255;
    DWORD flags = 0;
    if (GetLayeredWindowAttributes(H(hwnd), &color, &alpha, &flags))
        return alphaToPercent(alpha);
    return 100;
}

bool restoreOpacity(void *hwnd, bool keepLayered)
{
    SetLayeredWindowAttributes(H(hwnd), RGB(0, 0, 0), 255, LWA_ALPHA);

    // The window had WS_EX_LAYERED before we ever touched it (it manages its
    // own transparency) — leave its style alone, just reset our alpha above.
    if (keepLayered)
        return true;

    const LONG ex = GetWindowLongW(H(hwnd), GWL_EXSTYLE);
    if ((static_cast<DWORD>(ex) & WS_EX_LAYERED) != 0) {
        SetWindowLongW(H(hwnd), GWL_EXSTYLE, ex & ~WS_EX_LAYERED);
        SetWindowPos(H(hwnd), nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    return true;
}

QVector<PinnableWindow> enumerateWindows()
{
    QVector<HWND> handles;

    auto cb = [](HWND hwnd, LPARAM lparam) -> BOOL {
        auto *out = reinterpret_cast<QVector<HWND> *>(lparam);
        if (!IsWindowVisible(hwnd))
            return TRUE;
        const LONG ex = GetWindowLongW(hwnd, GWL_EXSTYLE);
        if ((static_cast<DWORD>(ex) & WS_EX_TOOLWINDOW) != 0)
            return TRUE;
        BOOL cloaked = FALSE;
        if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))
            && cloaked)
            return TRUE;
        if (isPinnable(hwnd))
            out->push_back(hwnd);
        return TRUE;
    };
    EnumWindows(cb, reinterpret_cast<LPARAM>(&handles));

    QVector<PinnableWindow> result;
    result.reserve(handles.size());
    for (HWND h : handles) {
        PinnableWindow w;
        w.hwnd = reinterpret_cast<intptr_t>(h);
        w.title = windowTitle(h);
        if (w.title.isEmpty())
            continue;
        w.processName = processName(h);
        result.push_back(w);
    }
    return result;
}

bool watchForeground(ForegroundCallback callback, void *context)
{
    stopWatchingForeground();
    g_foregroundCallback = callback;
    g_foregroundContext = context;
    // Out-of-context: Windows queues the event to this thread, so the callback
    // runs from our own message loop — nothing is injected into other apps.
    g_foregroundHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                                       nullptr, onForegroundEvent, 0, 0,
                                       WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    return g_foregroundHook != nullptr;
}

void stopWatchingForeground()
{
    if (g_foregroundHook)
        UnhookWinEvent(g_foregroundHook);
    g_foregroundHook = nullptr;
    g_foregroundCallback = nullptr;
    g_foregroundContext = nullptr;
}

bool animationsEnabled()
{
    BOOL enabled = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0);
    return enabled != FALSE;
}

void beep()
{
    // Play a soft bundled "tick" instead of the harsh system ding. PlaySound
    // with SND_MEMORY plays a WAV image straight from memory, so we avoid Qt
    // Multimedia entirely (just winmm). The buffer is loaded once and kept for
    // the process lifetime because SND_ASYNC reads it after this returns.
    static const QByteArray wav = [] {
        QFile f(QStringLiteral(":/tick.wav"));
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }();

    if (!wav.isEmpty())
        PlaySoundW(reinterpret_cast<const wchar_t *>(wav.constData()), nullptr,
                   SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    else
        MessageBeep(MB_OK);   // fallback if the resource is somehow missing
}

} // namespace winpin
