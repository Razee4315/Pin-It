#include "theme.h"

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
    const char *textControl;   // checkbox labels
    const char *keyBg;         // key chips, button hover
    const char *accent;        // primary button, slider
    const char *accentHover;
    const char *onAccent;      // text on the accent colour
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
    "#9a948a",              // textSubtle
    "#5a564e",              // textControl
    "#f0ede6",              // keyBg
    "#c49464",              // accent
    "#b6855a",              // accentHover
    "#ffffff",              // onAccent
    "#f6e3da",              // dangerHoverBg
    "#e6e2da",              // track
    "#fbeed3",              // warningBg
    "#e2c27d",              // warningBorder
    "#5c4307",              // warningText
    "#2a2622",              // statusBg
    "#f8f6f2",              // statusText
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

QPushButton#primary {
    background: $accent$; border: none; color: $onAccent$; font-weight: 700;
    padding: 9px 14px;
}
QPushButton#primary:hover { background: $accentHover$; }

QPushButton#unpin {
    background: transparent; border: 1px solid $border$;
    border-radius: 5px; color: $textSubtle$; font-weight: 700; font-size: 12px;
    padding: 0;
}
QPushButton#unpin:hover { background: $dangerHoverBg$; color: $accentHover$; border-color: $accent$; }

QCheckBox { color: $textControl$; font-size: 12px; spacing: 7px; }

QSlider::groove:horizontal { height: 4px; background: $track$; border-radius: 2px; }
QSlider::sub-page:horizontal { background: $accent$; border-radius: 2px; }
QSlider::handle:horizontal {
    background: $card$; border: 1px solid $accent$; width: 14px; height: 14px;
    margin: -6px 0; border-radius: 7px;
}
QScrollArea { background: transparent; border: none; }
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
        {"$textControl$", c.textControl},
        {"$keyBg$", c.keyBg},
        {"$accent$", c.accent},
        {"$accentHover$", c.accentHover},
        {"$onAccent$", c.onAccent},
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

} // namespace

namespace theme {

QString styleSheet()
{
    return build(kLight);
}

} // namespace theme
