// WinTea - press Win+T for a terminal, Win+Alt+T for an admin terminal.
//
// A single hidden top-level window owns everything: the low-level keyboard hook
// (whose callbacks arrive on this thread), the tray icon, and a config-file
// watcher. The message loop is MsgWaitForMultipleObjectsEx so we can wait on the
// directory-change handle and the message queue at once, and block at ~0% CPU
// when idle.
#include <windows.h>
#include <shellapi.h>
#include <wtsapi32.h>

#include <string>

#include "app.h"
#include "autostart.h"
#include "config.h"
#include "hook.h"
#include "launch.h"
#include "tray.h"
#include "version.h"

#pragma comment(lib, "wtsapi32.lib")

using namespace wintea;

namespace {

Config g_config;
HANDLE g_changeHandle = INVALID_HANDLE_VALUE;

std::wstring chordsLabel() {
    std::wstring t = g_config.terminal.enabled() ? FormatChord(g_config.terminal) : L"off";
    std::wstring a = g_config.adminTerminal.enabled() ? FormatChord(g_config.adminTerminal) : L"off";
    return t + L" / " + a;
}

std::wstring tooltipText() {
    if (!IsHookInstalled()) return L"WinTea (disabled)";
    return L"WinTea - " + chordsLabel();
}

void applyConfig(bool showReloadBalloon) {
    LoadedConfig loaded = LoadConfig();
    g_config = loaded.config;
    ConfigureHook(g_config.terminal, g_config.adminTerminal);
    TraySetState(IsHookInstalled(), tooltipText());

    for (const std::wstring& w : loaded.warnings)
        if (g_config.notifications)
            TrayShowBalloon(NIIF_WARNING, L"WinTea config", w);

    if (showReloadBalloon && g_config.notifications && loaded.warnings.empty())
        TrayShowBalloon(NIIF_INFO, L"WinTea", L"Config reloaded - " + chordsLabel());
}

void startConfigWatch() {
    std::wstring dir = ConfigDir();
    g_changeHandle = FindFirstChangeNotificationW(
        dir.c_str(), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME);
}

void openConfigInEditor() {
    ShellExecuteW(nullptr, L"open", ConfigPath().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

bool isElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elev = {};
    DWORD size = sizeof(elev);
    bool ok = GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &size);
    CloseHandle(token);
    return ok && elev.TokenIsElevated;
}

void showAbout(HWND hwnd) {
    std::wstring msg =
        L"WinTea " WINTEA_VERSION_STR L"\n\n"
        L"Terminal:  " + (g_config.terminal.enabled() ? FormatChord(g_config.terminal) : L"(off)") + L"\n"
        L"Admin:     " + (g_config.adminTerminal.enabled() ? FormatChord(g_config.adminTerminal) : L"(off)") + L"\n\n"
        L"Best served hot. MIT licensed.\n"
        WINTEA_REPO_URL;
    MessageBoxW(hwnd, msg.c_str(), L"About WinTea", MB_OK | MB_ICONINFORMATION);
}

void toggleEnabled(HWND hwnd) {
    if (IsHookInstalled()) {
        UninstallHook();
    } else {
        InstallHook(hwnd);
        ConfigureHook(g_config.terminal, g_config.adminTerminal);
    }
    TraySetState(IsHookInstalled(), tooltipText());
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == TaskbarCreatedMessage()) { // Explorer restarted: re-add our icon
        TrayReadd();
        return 0;
    }

    switch (msg) {
    case WM_APP_LAUNCH:
        LaunchTerminal(g_config, wParam != 0, hwnd);
        return 0;

    case WM_APP_RELOAD:
        applyConfig(/*showReloadBalloon*/ true);
        return 0;

    case WM_APP_NOTIFY: { // balloon routed from a worker thread
        Balloon* b = reinterpret_cast<Balloon*>(lParam);
        if (b) {
            if (g_config.notifications)
                TrayShowBalloon(b->flags, b->title, b->text, b->openConfigOnClick);
            delete b;
        }
        return 0;
    }

    case WM_APP_SHOW_EXISTING:
        if (g_config.notifications)
            TrayShowBalloon(NIIF_INFO, L"WinTea", L"Already brewing - check your tray.");
        return 0;

    case WM_APP_TRAY:
        switch (LOWORD(lParam)) {
        case WM_CONTEXTMENU:
            TrayShowMenu(hwnd, { IsHookInstalled(), IsAutostartEnabled(), chordsLabel() });
            break;
        case NIN_SELECT: // left click = quick enable/disable toggle
            toggleEnabled(hwnd);
            break;
        case NIN_BALLOONUSERCLICK:
            if (TrayLastBalloonOpensConfig()) openConfigInEditor();
            break;
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_ENABLED:   toggleEnabled(hwnd); break;
        case IDM_AUTOSTART: SetAutostartEnabled(!IsAutostartEnabled()); break;
        case IDM_OPEN_CONFIG: openConfigInEditor(); break;
        case IDM_RELOAD:    applyConfig(true); break;
        case IDM_ABOUT:     showAbout(hwnd); break;
        case IDM_GITHUB:
            ShellExecuteW(nullptr, L"open", WINTEA_REPO_URL, nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case IDM_QUIT:      DestroyWindow(hwnd); break;
        }
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_RELOAD_DEBOUNCE) {
            KillTimer(hwnd, TIMER_RELOAD_DEBOUNCE);
            applyConfig(true);
        }
        return 0;

    case WM_SETTINGCHANGE:
        if (lParam && wcscmp(reinterpret_cast<const wchar_t*>(lParam), L"ImmersiveColorSet") == 0)
            TrayRefreshTheme();
        return 0;

    case WM_WTSSESSION_CHANGE:
        // Lock/unlock and fast-user-switch: key-ups can be lost, so resync and
        // proactively re-hook (a timed-out hook is removed without notice).
        ResetModifierState();
        if (IsHookInstalled()) { UninstallHook(); InstallHook(hwnd); ConfigureHook(g_config.terminal, g_config.adminTerminal); }
        return 0;

    case WM_POWERBROADCAST:
        if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            ResetModifierState();
            if (IsHookInstalled()) { UninstallHook(); InstallHook(hwnd); ConfigureHook(g_config.terminal, g_config.adminTerminal); }
        }
        return TRUE;

    case WM_QUERYENDSESSION:
        return TRUE;

    case WM_DESTROY:
        TrayRemove();
        UninstallHook();
        WTSUnRegisterSessionNotification(hwnd);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// --- CLI subcommands -------------------------------------------------------

void writeConsole(const std::wstring& s) {
    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
        DWORD written = 0;
        WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), s.c_str(),
                      static_cast<DWORD>(s.size()), &written, nullptr);
    }
}

int cmdVersion() {
    writeConsole(L"WinTea " WINTEA_VERSION_STR L"\n");
    return 0;
}

int cmdExit() {
    HWND existing = FindWindowW(kWindowClass, nullptr);
    if (existing) PostMessageW(existing, WM_CLOSE, 0, 0);
    return 0;
}

// Injects the terminal chord with a non-sentinel tag, so the running instance's
// hook treats it as real input. Local dev aid; prints a marker and exits.
int cmdSelftest() {
    INPUT in[4] = {};
    auto key = [&](int idx, WORD vk, bool up) {
        in[idx].type = INPUT_KEYBOARD;
        in[idx].ki.wVk = vk;
        in[idx].ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
    };
    key(0, VK_LWIN, false);
    key(1, 'T', false);
    key(2, 'T', true);
    key(3, VK_LWIN, true);
    SendInput(4, in, sizeof(INPUT));
    writeConsole(L"SELFTEST fired win+t\n");
    return 0;
}

bool hasArg(int argc, wchar_t** argv, const wchar_t* flag) {
    for (int i = 1; i < argc; ++i)
        if (_wcsicmp(argv[i], flag) == 0) return true;
    return false;
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argv && hasArg(argc, argv, L"--version")) return cmdVersion();
    if (argv && hasArg(argc, argv, L"--exit"))    return cmdExit();
    if (argv && hasArg(argc, argv, L"--selftest")) return cmdSelftest();

    // Single instance.
    HANDLE mutex = CreateMutexW(nullptr, FALSE, kMutexName);
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(kWindowClass, nullptr);
        if (existing) PostMessageW(existing, WM_APP_SHOW_EXISTING, 0, 0);
        return 0;
    }

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kWindowClass;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    RegisterClassExW(&wc);

    // Top-level (NOT message-only) so we receive TaskbarCreated, WM_SETTINGCHANGE,
    // and session/power broadcasts. It is never shown.
    HWND hwnd = CreateWindowExW(0, kWindowClass, kAppName, 0,
                               0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) return 1;

    EnsureConfigExists();
    applyConfig(/*showReloadBalloon*/ false);

    if (!InstallHook(hwnd)) {
        MessageBoxW(nullptr, L"WinTea could not install its keyboard hook.",
                    L"WinTea", MB_OK | MB_ICONERROR);
        return 1;
    }
    ConfigureHook(g_config.terminal, g_config.adminTerminal);

    TrayAdd(hwnd, WM_APP_TRAY);
    TraySetState(true, tooltipText());
    WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION);
    startConfigWatch();

    if (isElevated())
        TrayShowBalloon(NIIF_WARNING, L"WinTea is running elevated",
            L"Both hotkeys will open admin terminals. Run WinTea normally for a "
            L"non-admin Win+T.");

    // Message loop that also waits on the config-change handle.
    bool running = true;
    while (running) {
        DWORD count = (g_changeHandle != INVALID_HANDLE_VALUE) ? 1 : 0;
        HANDLE handles[1] = { g_changeHandle };
        DWORD r = MsgWaitForMultipleObjectsEx(count, handles, INFINITE,
                                              QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (count == 1 && r == WAIT_OBJECT_0) {
            // Debounce: editors fire several notifications per save.
            SetTimer(hwnd, TIMER_RELOAD_DEBOUNCE, 300, nullptr);
            FindNextChangeNotification(g_changeHandle);
            continue;
        }

        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            if (m.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }

    if (g_changeHandle != INVALID_HANDLE_VALUE) FindCloseChangeNotification(g_changeHandle);
    if (mutex) CloseHandle(mutex);
    return 0;
}
