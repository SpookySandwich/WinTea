// Run-at-login via the HKCU Run key (no admin rights, no scheduled task in v1).
#pragma once

namespace wintea {

bool IsAutostartEnabled();          // true if our Run value points at this exe
bool SetAutostartEnabled(bool on);  // write/delete the Run value; returns success

} // namespace wintea
