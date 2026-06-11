// Low-level keyboard hook that claims the configured chords from Explorer.
//
// All functions here run on (or configure state owned by) the thread that calls
// Install — the same thread that pumps the message loop, because WH_KEYBOARD_LL
// callbacks are delivered to the installing thread. That single-thread invariant
// is why Configure/ResetModifiers need no locking.
#pragma once

#include <windows.h>

#include "chord.h"

namespace wintea {

// Installs the hook (idempotent). Matched chords post WM_APP_LAUNCH to `target`
// (wParam 0 = terminal, 1 = admin). Returns false if SetWindowsHookEx failed.
bool InstallHook(HWND target);

// Removes the hook (idempotent). Used by the tray Disable toggle.
void UninstallHook();

bool IsHookInstalled();

// Swaps the active chords (e.g. after a config reload). Safe to call anytime.
void ConfigureHook(const Chord& terminal, const Chord& admin);

// Clears tracked modifier state. Call after lock/unlock and resume-from-sleep,
// where key-ups can be lost on the secure desktop and leave modifiers "stuck".
void ResetModifierState();

} // namespace wintea
