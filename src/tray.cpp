#include "tray.h"

#include <windows.h>
#include <shellapi.h>

#include "app.h"
#include "res/resource.h"

namespace wintea {
namespace {

NOTIFYICONDATAW g_nid = {};
HWND g_hwnd = nullptr;
bool g_added = false;
bool g_enabled = true;
bool g_lastBalloonOpensConfig = false;
UINT g_taskbarCreated = 0;

// Light taskbar => SystemUsesLightTheme == 1. We then want the DARK glyph so it
// reads against the light background; on a dark taskbar we want the light glyph.
bool taskbarIsLight() {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return false; // default assumption: dark taskbar
    DWORD val = 0, size = sizeof(val), type = 0;
    bool ok = RegQueryValueExW(key, L"SystemUsesLightTheme", nullptr, &type,
                               reinterpret_cast<BYTE*>(&val), &size) == ERROR_SUCCESS;
    RegCloseKey(key);
    return ok && type == REG_DWORD && val != 0;
}

HICON loadTrayIcon() {
    HINSTANCE inst = GetModuleHandleW(nullptr);
    int id = taskbarIsLight() ? IDI_TRAY_FOR_LIGHT : IDI_TRAY_FOR_DARK;
    HICON ico = static_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(id), IMAGE_ICON,
                    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
                    LR_DEFAULTCOLOR));
    if (!ico) // fall back to the app icon, then the stock icon, so we never show nothing
        ico = static_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    if (!ico)
        ico = LoadIconW(nullptr, IDI_APPLICATION);
    return ico;
}

void appendItem(HMENU m, UINT id, const wchar_t* text, bool checked = false,
                bool defaultItem = false) {
    MENUITEMINFOW mi = { sizeof(mi) };
    mi.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE;
    mi.wID = id;
    mi.dwTypeData = const_cast<wchar_t*>(text);
    mi.fState = (checked ? MFS_CHECKED : 0) | (defaultItem ? MFS_DEFAULT : 0);
    InsertMenuItemW(m, GetMenuItemCount(m), TRUE, &mi);
}

void appendSeparator(HMENU m) {
    MENUITEMINFOW mi = { sizeof(mi) };
    mi.fMask = MIIM_FTYPE;
    mi.fType = MFT_SEPARATOR;
    InsertMenuItemW(m, GetMenuItemCount(m), TRUE, &mi);
}

} // namespace

UINT TaskbarCreatedMessage() {
    if (!g_taskbarCreated)
        g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    return g_taskbarCreated;
}

bool TrayAdd(HWND hwnd, UINT callbackMsg) {
    g_hwnd = hwnd;
    TaskbarCreatedMessage();

    g_nid = {};
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1; // not guidItem: GUID icons break for unsigned portable exes that move
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    g_nid.uCallbackMessage = callbackMsg;
    g_nid.hIcon = loadTrayIcon();
    wcscpy_s(g_nid.szTip, L"WinTea - Win+T: terminal, Win+Alt+T: admin");

    if (!Shell_NotifyIconW(NIM_ADD, &g_nid)) return false;
    g_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
    g_added = true;
    return true;
}

void TrayReadd() {
    if (!g_hwnd) return;
    g_added = false;
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    g_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
    g_added = true;
}

void TrayRemove() {
    if (g_added) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        g_added = false;
    }
    if (g_nid.hIcon) { DestroyIcon(g_nid.hIcon); g_nid.hIcon = nullptr; }
}

void TraySetState(bool enabled, const std::wstring& tooltip) {
    g_enabled = enabled;
    g_nid.uFlags = NIF_TIP | NIF_SHOWTIP;
    wcscpy_s(g_nid.szTip, tooltip.c_str());
    if (g_added) Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void TrayRefreshTheme() {
    HICON old = g_nid.hIcon;
    g_nid.hIcon = loadTrayIcon();
    g_nid.uFlags = NIF_ICON;
    if (g_added) Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    if (old) DestroyIcon(old);
}

void TrayShowBalloon(DWORD niifFlags, const std::wstring& title,
                     const std::wstring& text, bool opensConfigOnClick) {
    g_lastBalloonOpensConfig = opensConfigOnClick;
    g_nid.uFlags = NIF_INFO;
    g_nid.dwInfoFlags = niifFlags;
    wcscpy_s(g_nid.szInfoTitle, title.c_str());
    wcscpy_s(g_nid.szInfo, text.c_str());
    if (g_added) Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

bool TrayLastBalloonOpensConfig() { return g_lastBalloonOpensConfig; }

void TrayShowMenu(HWND hwnd, const TrayMenuModel& model) {
    HMENU menu = CreatePopupMenu();
    std::wstring enabledText = model.enabled
        ? L"Enabled  (" + model.chordsLabel + L")"
        : L"Enabled  (disabled)";
    appendItem(menu, IDM_ENABLED, enabledText.c_str(), model.enabled, /*default*/ true);
    appendItem(menu, IDM_AUTOSTART, L"Start with Windows", model.autostart);
    appendSeparator(menu);
    appendItem(menu, IDM_OPEN_CONFIG, L"Open config");
    appendItem(menu, IDM_RELOAD, L"Reload config");
    appendSeparator(menu);
    appendItem(menu, IDM_ABOUT, L"About WinTea...");
    appendItem(menu, IDM_GITHUB, L"GitHub");
    appendSeparator(menu);
    appendItem(menu, IDM_QUIT, L"Quit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd); // required so the menu dismisses on outside click
    TrackPopupMenuEx(menu, TPM_RIGHTBUTTON, pt.x, pt.y, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0); // documented menu-dismiss workaround
    DestroyMenu(menu);
}

} // namespace wintea
