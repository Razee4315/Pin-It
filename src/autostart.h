#pragma once
//
// autostart — "Start PinIt with Windows", stored where Windows itself reads
// it: the per-user Run registry key.
//
// The registry is the single source of truth. The installer can switch
// autostart on too, so a flag kept in PinIt's own settings file would drift
// out of step with what actually happens at login.
//
#include <QString>

namespace autostart {

bool isEnabled();
void setEnabled(bool enabled);

// If autostart is on but points at another location (a portable copy that was
// moved, or an older install), point it at this executable. Returns true if
// the entry was rewritten.
bool repairPath();

// The command line stored in the Run key for this executable.
QString command();

// Tests only: use a scratch registry key instead of the real Run key.
void setRegistryKeyForTesting(const QString &key);

} // namespace autostart
