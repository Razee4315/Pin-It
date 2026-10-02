#pragma once
//
// pinmatch — decides whether a saved pin belongs to a live window.
//
// Header-only and free of Win32 so the rule can be unit-tested on its own.
//
#include <QString>

#include "persistence.h"

namespace pinmatch {

// A saved pin is re-applied only to a window of the same app that still shows
// the saved title. Matching on the process alone pinned whichever window of
// that app happened to come first (any browser window, any UWP app hosted by
// ApplicationFrameHost.exe, even the desktop for explorer.exe).
inline bool matches(const persistence::SavedPin &saved, const QString &processName,
                    const QString &title)
{
    return !saved.title.isEmpty() && saved.title == title
           && saved.processName.compare(processName, Qt::CaseInsensitive) == 0;
}

} // namespace pinmatch
