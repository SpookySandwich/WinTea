// Spawns a terminal on a detached worker thread so the hook/UI thread never
// blocks on ShellExecuteEx (the UAC prompt for runas blocks until answered, and
// a blocked low-level hook thread is silently removed by Windows).
#pragma once

#include <windows.h>

#include "config.h"

namespace wintea {

// Resolves the command per config (auto-detect chain or override), then launches
// asynchronously. `admin` selects the runas verb. Errors are reported as balloons
// posted back to `notifyHwnd` (WM_APP_NOTIFY).
void LaunchTerminal(const Config& config, bool admin, HWND notifyHwnd);

} // namespace wintea
