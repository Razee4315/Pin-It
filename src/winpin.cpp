#include "winpin.h"

#include <windows.h>
#include <psapi.h>
#include <mmsystem.h>

#include <QFile>

#include <algorithm>

namespace {
inline HWND H(void *hwnd) { return reinterpret_cast<HWND>(hwnd); }
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
        return QStringLiteral("Unknown");

    QVector<wchar_t> buf(len + 1);
    const int copied = GetWindowTextW(H(hwnd), buf.data(), len + 1);
    if (copied <= 0)
        return QStringLiteral("Unknown");

    return QString::fromWCharArray(buf.data(), copied);
}

QString processName(void *hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(H(hwnd), &pid);
    if (pid == 0)
        return QStringLiteral("Unknown");

    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc)
        return QStringLiteral("Unknown");

    wchar_t buf[MAX_PATH] = {0};
    DWORD size = MAX_PATH;
    QString result = QStringLiteral("Unknown");
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
        if (IsWindowVisible(hwnd)) {
            const LONG ex = GetWindowLongW(hwnd, GWL_EXSTYLE);
            if ((static_cast<DWORD>(ex) & WS_EX_TOOLWINDOW) == 0)
                out->push_back(hwnd);
        }
        return TRUE;
    };
    EnumWindows(cb, reinterpret_cast<LPARAM>(&handles));

    QVector<PinnableWindow> result;
    result.reserve(handles.size());
    for (HWND h : handles) {
        PinnableWindow w;
        w.hwnd = reinterpret_cast<intptr_t>(h);
        w.title = windowTitle(h);
        w.processName = processName(h);
        result.push_back(w);
    }
    return result;
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
