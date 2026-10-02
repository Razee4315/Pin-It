#include "pinflash.h"
#include "winpin.h"

#include <QColor>
#include <QGuiApplication>
#include <QOperatingSystemVersion>
#include <QPainter>
#include <QPen>
#include <QPropertyAnimation>
#include <QScreen>
#include <QTimer>
#include <QWidget>

namespace {

constexpr int kBorderWidth = 4;
constexpr int kHoldMs = 300;   // fully visible
constexpr int kFadeMs = 250;   // then fades out

// Win32 reports window rectangles in physical pixels; Qt positions widgets in
// device-independent ones, per screen. Returns an empty rect if the window is
// not on any screen (e.g. minimised).
QRect toLogical(const QRect &native)
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (const QScreen *screen : screens) {
        const qreal dpr = screen->devicePixelRatio();
        const QRect logicalScreen = screen->geometry();
        const QRect nativeScreen(logicalScreen.topLeft(), logicalScreen.size() * dpr);
        if (!nativeScreen.contains(native.center()))
            continue;
        const QPoint offset = (native.topLeft() - nativeScreen.topLeft()) / dpr;
        return QRect(logicalScreen.topLeft() + offset, native.size() / dpr);
    }
    return QRect();
}

class FlashOverlay : public QWidget
{
public:
    FlashOverlay(const QRect &geometry, const QColor &color)
        : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                               | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus
                               | Qt::NoDropShadowWindowHint)
        , m_color(color)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_DeleteOnClose);
        setGeometry(geometry);
    }

    void run()
    {
        show();

        if (!winpin::animationsEnabled()) {
            QTimer::singleShot(kHoldMs + kFadeMs, this, &QWidget::close);
            return;
        }

        auto *fade = new QPropertyAnimation(this, "windowOpacity", this);
        fade->setDuration(kFadeMs);
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);
        connect(fade, &QAbstractAnimation::finished, this, &QWidget::close);
        QTimer::singleShot(kHoldMs, fade, [fade]() { fade->start(); });
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(m_color, kBorderWidth));
        painter.setBrush(Qt::NoBrush);

        // Windows 11 rounds window corners; Windows 10 does not.
        const bool rounded =
            QOperatingSystemVersion::current() >= QOperatingSystemVersion::Windows11;
        const qreal radius = rounded ? 8.0 : 0.0;
        const qreal inset = kBorderWidth / 2.0;
        painter.drawRoundedRect(QRectF(rect()).adjusted(inset, inset, -inset, -inset),
                                radius, radius);
    }

private:
    QColor m_color;
};

} // namespace

namespace pinflash {

void show(intptr_t hwnd, Kind kind)
{
    const QRect geometry = toLogical(winpin::frameRect(hwnd));
    if (geometry.isEmpty())
        return;

    const QColor color = kind == Kind::Pinned ? QColor(0xc4, 0x94, 0x64)
                                              : QColor(0x8a, 0x85, 0x7c);
    auto *overlay = new FlashOverlay(geometry, color);   // deletes itself on close
    overlay->run();
}

} // namespace pinflash
