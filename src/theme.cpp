#include "theme.h"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QString>
#include <QStyle>
#include <QStyleHints>

#include <utility>

namespace {

// Every colour the UI uses, by role. The style sheet below refers to them as
// $name$, so a colour is defined in exactly one place.
struct Colors {
    const char *window;        // window / dialog background
    const char *card;          // cards, buttons
    const char *cardBorder;
    const char *border;        // buttons, key chips
    const char *text;
    const char *textMuted;     // secondary text
    const char *textSubtle;    // "+" separators, the unpin glyph
    const char *keyBg;         // key chips, button hover
    const char *accent;        // slider, non-text accents (3:1 against the card)
    const char *accentStrong;  // primary button, focus rings (4.5:1 with onAccent)
    const char *accentHover;
    const char *onAccent;      // text on accentStrong
    const char *onAvatar;      // initial on the pastel app badges
    const char *dangerHoverBg; // unpin button hover
    const char *track;         // slider groove
    const char *warningBg;
    const char *warningBorder;
    const char *warningText;
    const char *statusBg;      // in-window message
    const char *statusText;
};

// Warm "paper" theme — ported from the original PinIt CSS variables.
constexpr Colors kLight = {
    "#f8f6f2",              // window
    "#ffffff",              // card
    "rgba(0,0,0,0.08)",     // cardBorder
    "rgba(0,0,0,0.12)",     // border
    "#2a2622",              // text
    "#6b6760",              // textMuted
    "#6b655d",              // textSubtle
    "#f0ede6",              // keyBg
    "#b07c4a",              // accent
    "#96653a",              // accentStrong
    "#855832",              // accentHover
    "#ffffff",              // onAccent
    "#2a2622",              // onAvatar
    "#f6e3da",              // dangerHoverBg
    "#e6e2da",              // track
    "#fbeed3",              // warningBg
    "#e2c27d",              // warningBorder
    "#5c4307",              // warningText
    "#2a2622",              // statusBg
    "#f8f6f2",              // statusText
};

// The same paper, at night. Text/background pairs keep at least 4.5:1.
constexpr Colors kDark = {
    "#1f1d1a",              // window
    "#2a2724",              // card
    "rgba(255,255,255,0.08)",   // cardBorder
    "rgba(255,255,255,0.16)",   // border
    "#f1ede6",              // text
    "#b8b1a6",              // textMuted
    "#aba498",              // textSubtle
    "#38342f",              // keyBg
    "#d0a06c",              // accent
    "#d9a871",              // accentStrong
    "#e6ba88",              // accentHover
    "#1f1d1a",              // onAccent
    "#2a2622",              // onAvatar
    "#4a3328",              // dangerHoverBg
    "#45403a",              // track
    "#4a3a17",              // warningBg
    "#7a6124",              // warningBorder
    "#f5deb0",              // warningText
    "#f1ede6",              // statusBg
    "#1f1d1a",              // statusText
};

const char *kStyleSheet = R"qss(
QWidget#central { background: $window$; }
QDialog { background: $window$; }
QLabel { color: $text$; font-family: "Segoe UI"; }

QLabel[role="title"]   { font-size: 17px; font-weight: 700; color: $text$; }
QLabel[role="section"] { font-size: 11px; font-weight: 700; color: $textMuted$;
                         letter-spacing: 1px; }
QLabel[role="desc"]    { color: $textMuted$; font-size: 12px; }
QLabel[role="muted"]   { color: $textMuted$; font-size: 12px; }

QLabel[role="key"] {
    background: $keyBg$; border: 1px solid $border$;
    border-radius: 5px; padding: 3px 9px;
    color: $text$; font-weight: 700; font-size: 11px;
}
QLabel[role="plus"] { color: $textSubtle$; font-size: 12px; }
QLabel[role="avatar"] {
    border-radius: 6px; color: $onAvatar$; font-weight: 700; font-size: 12px;
}

QLabel[role="warning"] {
    background: $warningBg$; border: 1px solid $warningBorder$; border-radius: 8px;
    padding: 6px 9px; color: $warningText$; font-size: 12px;
}
QLabel[role="status"] {
    background: $statusBg$; color: $statusText$; border-radius: 8px;
    padding: 6px 10px; font-size: 12px;
}

QFrame[role="card"] {
    background: $card$; border: 1px solid $cardBorder$;
    border-radius: 12px;
}

QPushButton {
    background: $card$; border: 1px solid $border$;
    border-radius: 8px; padding: 7px 14px; color: $text$; font-size: 12px;
}
QPushButton:hover { background: $keyBg$; }
QPushButton:focus { background: $keyBg$; border-color: $accentStrong$; }

/* The 2px border is always there (in the fill colour) so the focus ring can
   appear without the button changing size. */
QPushButton#primary {
    background: $accentStrong$; border: 2px solid $accentStrong$; color: $onAccent$;
    font-weight: 700; padding: 7px 12px;
}
QPushButton#primary:hover { background: $accentHover$; border-color: $accentHover$; }
QPushButton#primary:focus { border-color: $text$; }

QPushButton#rowToggle {
    background: transparent; border: 1px solid $border$;
    border-radius: 5px; color: $textSubtle$; font-weight: 700; font-size: 12px;
    padding: 0;
}
QPushButton#rowToggle:hover { background: $keyBg$; color: $text$; }
QPushButton#rowToggle:focus { border-color: $accentStrong$; color: $text$; }
QPushButton#rowToggle:checked {
    background: $accentStrong$; border-color: $accentStrong$; color: $onAccent$;
}
QPushButton#rowToggle:checked:focus { border-color: $text$; }

QPushButton#unpin {
    background: transparent; border: 1px solid $border$;
    border-radius: 5px; color: $textSubtle$; font-weight: 700; font-size: 12px;
    padding: 0;
}
QPushButton#unpin:hover { background: $dangerHoverBg$; color: $accentHover$; border-color: $accentHover$; }
QPushButton#unpin:focus { background: $dangerHoverBg$; color: $accentHover$; border-color: $accentHover$; }

/* Low-key text buttons in the footer. */
QPushButton#link {
    background: transparent; border: 1px solid transparent; border-radius: 6px;
    padding: 3px 6px; color: $accentHover$; font-size: 12px;
}
QPushButton#link:hover { background: $keyBg$; }
QPushButton#link:focus { background: $keyBg$; border-color: $accentStrong$; }
QPushButton#link:disabled { color: $textSubtle$; }

/* Check boxes have no rules on purpose: the Fusion style draws them — box,
   tick and focus frame — from the palette, in both variants. */

QSlider::groove:horizontal { height: 4px; background: $track$; border-radius: 2px; }
QSlider::sub-page:horizontal { background: $accent$; border-radius: 2px; }
QSlider::handle:horizontal {
    background: $card$; border: 1px solid $accent$; width: 14px; height: 14px;
    margin: -6px 0; border-radius: 7px;
}
QSlider::handle:horizontal:focus { background: $accentStrong$; border-color: $accentStrong$; }
QScrollArea { background: transparent; border: none; }

QToolTip { background: $card$; color: $text$; border: 1px solid $border$; padding: 3px 6px; }
)qss";

QString build(const Colors &c)
{
    const std::pair<const char *, const char *> tokens[] = {
        {"$window$", c.window},
        {"$card$", c.card},
        {"$cardBorder$", c.cardBorder},
        {"$border$", c.border},
        {"$text$", c.text},
        {"$textMuted$", c.textMuted},
        {"$textSubtle$", c.textSubtle},
        {"$keyBg$", c.keyBg},
        {"$accent$", c.accent},
        {"$accentStrong$", c.accentStrong},
        {"$accentHover$", c.accentHover},
        {"$onAccent$", c.onAccent},
        {"$onAvatar$", c.onAvatar},
        {"$dangerHoverBg$", c.dangerHoverBg},
        {"$track$", c.track},
        {"$warningBg$", c.warningBg},
        {"$warningBorder$", c.warningBorder},
        {"$warningText$", c.warningText},
        {"$statusBg$", c.statusBg},
        {"$statusText$", c.statusText},
    };

    QString sheet = QString::fromUtf8(kStyleSheet);
    for (const auto &[token, value] : tokens)
        sheet.replace(QLatin1String(token), QLatin1String(value));
    return sheet;
}

// For the widgets the style sheet doesn't describe.
QPalette buildPalette(const Colors &c)
{
    const QColor window(c.window), card(c.card), text(c.text), subtle(c.textSubtle);
    const QColor strong(c.accentStrong);

    QPalette p;
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, card);
    p.setColor(QPalette::AlternateBase, QColor(c.keyBg));
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, card);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::ToolTipBase, card);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, subtle);
    p.setColor(QPalette::Highlight, strong);
    p.setColor(QPalette::HighlightedText, QColor(c.onAccent));
    p.setColor(QPalette::Link, strong);
    for (const QPalette::ColorRole role :
         {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, subtle);
    return p;
}

} // namespace

namespace theme {

Scheme systemScheme()
{
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark
               ? Scheme::Dark : Scheme::Light;
}

void apply(QApplication &app, Scheme scheme)
{
    const Colors &colors = scheme == Scheme::Dark ? kDark : kLight;

    // Fusion draws every control from the palette, so the same code looks the
    // same on Windows 10 and 11 and in both variants. (The native Windows
    // styles ignore much of a custom palette.)
    if (app.style()->name().compare(QLatin1String("fusion"), Qt::CaseInsensitive) != 0)
        QApplication::setStyle(QStringLiteral("Fusion"));
    QApplication::setPalette(buildPalette(colors));
    app.setStyleSheet(build(colors));
}

void followSystem(QApplication &app)
{
    apply(app, systemScheme());
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, &app,
                     [&app]() { apply(app, systemScheme()); });
}

} // namespace theme
