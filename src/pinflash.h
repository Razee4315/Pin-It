#pragma once
//
// pinflash — a brief coloured outline drawn around another app's window.
//
// This is PinIt's on-screen feedback: the outline appears exactly where the
// user is looking, the instant a window is pinned or unpinned, without a
// system notification. It is also used to point out which window a row in the
// list belongs to.
//
// The outline is a click-through, never-focused overlay that fades out by
// itself (or simply disappears when Windows animations are turned off).
//
#include <cstdint>

namespace pinflash {

enum class Kind {
    Pinned,     // accent colour
    Unpinned,   // neutral grey
};

void show(intptr_t hwnd, Kind kind);

} // namespace pinflash
