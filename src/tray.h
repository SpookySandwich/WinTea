// System-tray icon, context menu, and balloons. All calls run on the UI thread.
#pragma once

#include <windows.h>

#include <string>

namespace wintea {

struct TrayMenuModel {
    bool         enabled;
    bool         autostart;
    std::wstring chordsLabel; // e.g. "Win+T / Win+Alt+T"
};

// The message Explorer broadcasts when the taskbar is (re)created; the window
// proc compares incoming messages against this to re-add the icon.
UINT TaskbarCreatedMessage();

bool TrayAdd(HWND hwnd, UINT callbackMsg);     // initial NIM_ADD + version
void TrayReadd();                              // after a TaskbarCreated broadcast
void TrayRemove();                             // NIM_DELETE on shutdown
void TraySetState(bool enabled, const std::wstring& tooltip);
void TrayRefreshTheme();                       // re-pick glyph for light/dark taskbar
void TrayShowBalloon(DWORD niifFlags, const std::wstring& title,
                     const std::wstring& text, bool opensConfigOnClick = false);
bool TrayLastBalloonOpensConfig();             // for NIN_BALLOONUSERCLICK handling
void TrayShowMenu(HWND hwnd, const TrayMenuModel& model);

} // namespace wintea
