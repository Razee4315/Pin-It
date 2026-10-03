#include "winpin.h"

#include <windows.h>
#include <psapi.h>
#include <dwmapi.h>
#include <mmsystem.h>

#include <QFile>

#include <algorithm>

namespace {
// The one place a WindowId becomes an HWND again (and back).
inline HWND H(winpin::WindowId hwnd) { return reinterpret_cast<HWND>(hwnd); }
inline winpin::WindowId Id(HWND hwnd) { return reinterpret_cast<winpin::WindowId>(hwnd); }

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

QString windowTitle(WindowId hwnd)
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

QString processName(WindowId hwnd)
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

WindowId foregroundWindow()
{
    return Id(GetForegroundWindow());
}

bool isValidWindow(WindowId hwnd)
{
    return IsWindow(H(hwnd)) != FALSE;
}

bool isTopmost(WindowId hwnd)
{
    const LONG ex = GetWindowLongW(H(hwnd), GWL_EXSTYLE);
    return (static_cast<DWORD>(ex) & WS_EX_TOPMOST) != 0;
}

bool isLayered(WindowId hwnd)
{
    const LONG ex = GetWindowLongW(H(hwnd), GWL_EXSTYLE);
    return (static_cast<DWORD>(ex) & WS_EX_LAYERED) != 0;
}

QString className(WindowId hwnd)
{
    wchar_t buf[256] = {0};
    const int len = GetClassNameW(H(hwnd), buf, 256);
    return QString::fromWCharArray(buf, len);
}

QRect frameRect(WindowId hwnd)
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

bool isPinnable(WindowId hwnd)
{
    if (!isValidWindow(hwnd))
        return false;

    DWORD pid = 0;
    GetWindowThreadProcessId(H(hwnd), &pid);
    if (pid == GetCurrentProcessId())
        return false;   // PinIt's own windows

    return !isShellClass(className(hwnd));
}

bool applyTopmost(WindowId hwnd)
{
    return SetWindowPos(H(hwnd), HWND_TOPMOST, 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE) != FALSE;
}

bool removeTopmost(WindowId hwnd)
{
    return SetWindowPos(H(hwnd), HWND_NOTOPMOST, 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE) != FALSE;
}

bool setOpacityPercent(WindowId hwnd, int percent)
{
    percent = std::clamp(percent, kMinOpacity, kMaxOpacity);

    const LONG ex = GetWindowLongW(H(hwnd), GWL_EXSTYLE);
    if ((static_cast<DWORD>(ex) & WS_EX_LAYERED) == 0)
        SetWindowLongW(H(hwnd), GWL_EXSTYLE, ex | WS_EX_LAYERED);

    const BYTE alpha = static_cast<BYTE>(percentToAlpha(percent));
    return SetLayeredWindowAttributes(H(hwnd), RGB(0, 0, 0), alpha, LWA_ALPHA) != FALSE;
}

bool isClickThrough(WindowId hwnd)
{
    const LONG ex = GetWindowLongW(H(hwnd), GWL_EXSTYLE);
    return (static_cast<DWORD>(ex) & WS_EX_TRANSPARENT) != 0;
}

bool setClickThrough(WindowId hwnd, bool enabled)
{
    const LONG ex = GetWindowLongW(H(hwnd), GWL_EXSTYLE);
    const LONG wanted = enabled ? (ex | WS_EX_TRANSPARENT) : (ex & ~WS_EX_TRANSPARENT);
    if (wanted != ex)
        SetWindowLongW(H(hwnd), GWL_EXSTYLE, wanted);
    return isClickThrough(hwnd) == enabled;
}

bool restoreOpacity(WindowId hwnd, bool keepLayered)
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
        if (isPinnable(Id(hwnd)))
            out->push_back(hwnd);
        return TRUE;
    };
    EnumWindows(cb, reinterpret_cast<LPARAM>(&handles));

    QVector<PinnableWindow> result;
    result.reserve(handles.size());
    for (HWND h : handles) {
        PinnableWindow w;
        w.hwnd = Id(h);
        w.title = windowTitle(w.hwnd);
        if (w.title.isEmpty())
            continue;
        w.processName = processName(w.hwnd);
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

void playPinSound()
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
