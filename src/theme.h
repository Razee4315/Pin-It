#pragma once
//
// theme — PinIt's look, in a light and a dark variant.
//
// Widgets pick their styling through a "role" property or an object name
// (e.g. role="card", #primary); colours live here and nowhere else. Widgets
// without rules of their own (lists, combo boxes, menus, check boxes…) follow
// the matching palette, so the whole app changes together.
//
class QApplication;

namespace theme {

enum class Scheme { Light, Dark };

// What Windows is set to ("Choose your mode" in Settings > Colours).
Scheme systemScheme();

// Apply one variant to the whole application.
void apply(QApplication &app, Scheme scheme);

// Apply the variant Windows asks for, and switch when the user changes it.
void followSystem(QApplication &app);

} // namespace theme
