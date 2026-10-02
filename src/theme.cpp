#include "theme.h"

namespace {

// Warm "paper" theme — ported from the original PinIt CSS variables.
const char *kStyleSheet = R"qss(
QWidget#central { background: #f8f6f2; }
QDialog { background: #f8f6f2; }
QLabel { color: #2a2622; font-family: "Segoe UI"; }

QLabel[role="title"]   { font-size: 17px; font-weight: 700; color: #2a2622; }
QLabel[role="section"] { font-size: 11px; font-weight: 700; color: #6b6760;
                         letter-spacing: 1px; }
QLabel[role="desc"]    { color: #6b6760; font-size: 12px; }
QLabel[role="muted"]   { color: #6b6760; font-size: 12px; }

QLabel[role="key"] {
    background: #f0ede6; border: 1px solid rgba(0,0,0,0.12);
    border-radius: 5px; padding: 3px 9px;
    color: #2a2622; font-weight: 700; font-size: 11px;
}
QLabel[role="plus"] { color: #9a948a; font-size: 12px; }

QLabel[role="warning"] {
    background: #fbeed3; border: 1px solid #e2c27d; border-radius: 8px;
    padding: 6px 9px; color: #5c4307; font-size: 12px;
}
QLabel[role="status"] {
    background: #2a2622; color: #f8f6f2; border-radius: 8px;
    padding: 6px 10px; font-size: 12px;
}

QFrame[role="card"] {
    background: #ffffff; border: 1px solid rgba(0,0,0,0.08);
    border-radius: 12px;
}

QPushButton {
    background: #ffffff; border: 1px solid rgba(0,0,0,0.12);
    border-radius: 8px; padding: 7px 14px; color: #2a2622; font-size: 12px;
}
QPushButton:hover { background: #f0ede6; }

QPushButton#primary {
    background: #c49464; border: none; color: #ffffff; font-weight: 700;
    padding: 9px 14px;
}
QPushButton#primary:hover { background: #b6855a; }

QPushButton#unpin {
    background: transparent; border: 1px solid rgba(0,0,0,0.12);
    border-radius: 5px; color: #9a948a; font-weight: 700; font-size: 12px;
    padding: 0;
}
QPushButton#unpin:hover { background: #f6e3da; color: #b6855a; border-color: #c49464; }

QCheckBox { color: #5a564e; font-size: 12px; spacing: 7px; }

QSlider::groove:horizontal { height: 4px; background: #e6e2da; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #c49464; border-radius: 2px; }
QSlider::handle:horizontal {
    background: #ffffff; border: 1px solid #c49464; width: 14px; height: 14px;
    margin: -6px 0; border-radius: 7px;
}
QScrollArea { background: transparent; border: none; }
)qss";

} // namespace

namespace theme {

QString styleSheet()
{
    return QString::fromUtf8(kStyleSheet);
}

} // namespace theme
