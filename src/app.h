// Shared types, constants, and window-message contract for WinTea.
#pragma once

#include <windows.h>
#include <string>

#include "chord.h"

namespace wintea {

// Marks input we inject ourselves so the hook ignores it. "WinT" in ASCII.
inline constexpr ULONG_PTR kInjectSentinel = 0x57696E54;

// Private window messages (all handled on the UI/hook thread).
enum : UINT {
    WM_APP_TRAY         = WM_APP + 1, // tray icon callback (NOTIFYICON_VERSION_4)
    WM_APP_LAUNCH       = WM_APP + 2, // wParam: 0 = normal, 1 = admin
    WM_APP_RELOAD       = WM_APP + 3, // tray "Reload config"
    WM_APP_NOTIFY       = WM_APP + 4, // wParam: NIIF_*, lParam: Balloon* (heap, we free)
    WM_APP_SHOW_EXISTING= WM_APP + 5, // a second instance asked us to surface ourselves
};

// Tray menu command IDs.
enum : UINT {
    IDM_ENABLED = 100,
    IDM_AUTOSTART,
    IDM_OPEN_CONFIG,
    IDM_RELOAD,
    IDM_ABOUT,
    IDM_GITHUB,
    IDM_QUIT,
};

// Timer IDs.
enum : UINT_PTR {
    TIMER_RELOAD_DEBOUNCE = 1,
};

// A balloon notification routed from a worker thread to the UI thread.
struct Balloon {
    DWORD       flags; // NIIF_INFO / NIIF_WARNING / NIIF_ERROR
    std::wstring title;
    std::wstring text;
    bool        openConfigOnClick = false;
};

// Window class / identity strings.
inline constexpr wchar_t kWindowClass[] = L"WinTea.MainWindow";
inline constexpr wchar_t kMutexName[]   = L"Local\\WinTea.SingleInstance";
inline constexpr wchar_t kAppName[]     = L"WinTea";

} // namespace wintea
