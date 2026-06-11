// WinTea M1 mechanism spike (throwaway).
// Proves on this machine: a WH_KEYBOARD_LL hook can claim Win+T from Explorer,
// suppress the Start menu via a dummy key, and launch terminals (admin via runas)
// from a worker thread without ever blocking the hook thread.
//
// Modes:
//   spike [--timeout SEC]   install hook, log MATCH/LAUNCH lines to stdout
//   spike --reghotkey       show that RegisterHotKey(Win+T) is already taken
//   spike --fire SEQ [--force]   inject input: normal | repeat | shift | barewin
//   spike --check-start     print START-OPEN / START-CLOSED (DWM cloak attribute)
//   spike --wt-windows      list HWNDs of visible Windows Terminal top-levels
//   spike --close-wt H...   post WM_CLOSE to the given hex HWNDs

#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <process.h>
#include <cstdio>
#include <cwchar>
#include <cstdlib>
#include <vector>
#include <string>

#pragma comment(lib, "dwmapi.lib")

static const ULONG_PTR kSentinel = 0x57696E54; // "WinT"
enum : UINT {
    MSG_LAUNCH = WM_APP + 1, // wParam: 0 normal, 1 admin
    MSG_LOG    = WM_APP + 2, // wParam: event code
};

// ---------------------------------------------------------------- hook side
static DWORD g_mainTid = 0;
static bool  g_tracked[256] = {};
static UINT  g_swallowedVk = 0;

static bool physDown(int vk) {
    // Tracked state corrected by async state: a release eaten by the secure
    // desktop (UAC) never reaches the hook, but async state does get updated.
    return g_tracked[vk] && (GetAsyncKeyState(vk) & 0x8000) != 0;
}

static LRESULT CALLBACK hookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION)
        return CallNextHookEx(nullptr, code, wParam, lParam);
    const KBDLLHOOKSTRUCT* kb = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
    if (kb->dwExtraInfo == kSentinel) // our own dummy key only; other injected input is real
        return CallNextHookEx(nullptr, code, wParam, lParam);

    const bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    switch (kb->vkCode) {
    case VK_LWIN: case VK_RWIN:
    case VK_LMENU: case VK_RMENU:
    case VK_LCONTROL: case VK_RCONTROL:
    case VK_LSHIFT: case VK_RSHIFT:
        g_tracked[kb->vkCode] = isDown;
        break;
    }

    // Swallow gate: eat autorepeats and the final key-up of a matched chord key.
    if (g_swallowedVk != 0 && kb->vkCode == g_swallowedVk) {
        if (!isDown)
            g_swallowedVk = 0;
        return 1;
    }

    if (isDown && kb->vkCode == 'T') {
        const bool win   = physDown(VK_LWIN) || physDown(VK_RWIN);
        const bool alt   = physDown(VK_LMENU) || physDown(VK_RMENU) || (kb->flags & LLKHF_ALTDOWN) != 0;
        const bool ctrl  = physDown(VK_LCONTROL) || physDown(VK_RCONTROL);
        const bool shift = physDown(VK_LSHIFT) || physDown(VK_RSHIFT);
        if (win && !ctrl && !shift) { // exact: win+t or win+alt+t, nothing else
            g_swallowedVk = kb->vkCode;
            // Dirty the Win press so Explorer doesn't open Start on Win-up.
            INPUT dummy[2] = {};
            dummy[0].type = INPUT_KEYBOARD;
            dummy[0].ki.wVk = 0xFF;
            dummy[0].ki.dwExtraInfo = kSentinel;
            dummy[1] = dummy[0];
            dummy[1].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(2, dummy, sizeof(INPUT));
            PostThreadMessageW(g_mainTid, MSG_LAUNCH, alt ? 1 : 0, 0);
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

// -------------------------------------------------------------- launch side
struct LaunchSpec { bool admin; int seq; };

static void launchWorker(void* arg) {
    LaunchSpec* spec = static_cast<LaunchSpec*>(arg);
    wchar_t home[MAX_PATH];
    ExpandEnvironmentStringsW(L"%USERPROFILE%", home, MAX_PATH);

    const wchar_t* candidates[] = { L"wt.exe", L"pwsh.exe", L"powershell.exe", L"cmd.exe" };
    wchar_t resolved[MAX_PATH];
    const wchar_t* chosen = nullptr;
    for (const wchar_t* c : candidates) {
        if (SearchPathW(nullptr, c, nullptr, MAX_PATH, resolved, nullptr) > 0) { chosen = c; break; }
    }
    if (!chosen) {
        printf("LAUNCH #%d FAIL no-candidate\n", spec->seq);
        fflush(stdout);
        delete spec;
        return;
    }

    std::wstring params;
    if (wcscmp(chosen, L"wt.exe") == 0)
        params = std::wstring(L"-d \"") + home + L"\"";

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = spec->admin ? L"runas" : L"open";
    sei.lpFile = resolved;
    sei.lpParameters = params.empty() ? nullptr : params.c_str();
    sei.lpDirectory = home;
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&sei)) {
        const DWORD err = GetLastError();
        if (err == ERROR_CANCELLED)
            printf("LAUNCH #%d DECLINED (UAC) %ls\n", spec->seq, chosen);
        else
            printf("LAUNCH #%d FAIL err=%lu %ls\n", spec->seq, err, chosen);
    } else {
        DWORD exitCode = 0;
        if (sei.hProcess) {
            WaitForSingleObject(sei.hProcess, 1500);
            GetExitCodeProcess(sei.hProcess, &exitCode);
            CloseHandle(sei.hProcess);
        }
        if (exitCode != 0 && exitCode != STILL_ACTIVE)
            printf("LAUNCH #%d SUSPECT exit=%lu %ls (alias broken?)\n", spec->seq, exitCode, chosen);
        else
            printf("LAUNCH #%d OK %s %ls\n", spec->seq, spec->admin ? "admin" : "normal", resolved);
    }
    fflush(stdout);
    delete spec;
}

// --------------------------------------------------------------- utilities
struct EnumCtx { std::vector<HWND> wins; };

static bool isWtWindow(HWND h) {
    if (!IsWindowVisible(h)) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (!pid) return false;
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) return false;
    wchar_t path[MAX_PATH];
    DWORD len = MAX_PATH;
    bool wt = false;
    if (QueryFullProcessImageNameW(proc, 0, path, &len)) {
        const wchar_t* base = wcsrchr(path, L'\\');
        base = base ? base + 1 : path;
        wt = _wcsicmp(base, L"WindowsTerminal.exe") == 0;
    }
    CloseHandle(proc);
    return wt;
}

static BOOL CALLBACK enumWt(HWND h, LPARAM lp) {
    if (isWtWindow(h))
        reinterpret_cast<EnumCtx*>(lp)->wins.push_back(h);
    return TRUE;
}

static int cmdWtWindows() {
    EnumCtx ctx;
    EnumWindows(enumWt, reinterpret_cast<LPARAM>(&ctx));
    for (HWND h : ctx.wins) {
        wchar_t cls[128] = L"?";
        GetClassNameW(h, cls, 128);
        printf("WT %p class=%ls\n", static_cast<void*>(h), cls);
    }
    printf("WT-COUNT %zu\n", ctx.wins.size());
    return 0;
}

static int cmdCloseWt(int argc, wchar_t** argv, int first) {
    for (int i = first; i < argc; ++i) {
        HWND h = reinterpret_cast<HWND>(static_cast<INT_PTR>(wcstoull(argv[i], nullptr, 16)));
        if (IsWindow(h)) {
            PostMessageW(h, WM_CLOSE, 0, 0);
            printf("CLOSED %p\n", static_cast<void*>(h));
        }
    }
    return 0;
}

static int cmdCheckStart() {
    HWND start = FindWindowW(L"Windows.UI.Core.CoreWindow", L"Start");
    if (!start) {
        // Win11 variants host Start differently; fall back to foreground probe.
        HWND fg = GetForegroundWindow();
        wchar_t cls[128] = L"";
        if (fg) GetClassNameW(fg, cls, 128);
        printf("START-NOTFOUND foreground=%ls\n", cls);
        return 2;
    }
    DWORD cloaked = 0;
    DwmGetWindowAttribute(start, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    const bool open = IsWindowVisible(start) && cloaked == 0;
    printf(open ? "START-OPEN\n" : "START-CLOSED\n");
    return open ? 1 : 0;
}

static int cmdRegHotkey() {
    if (RegisterHotKey(nullptr, 1, MOD_WIN | MOD_NOREPEAT, 'T')) {
        printf("REGHOTKEY UNEXPECTED-SUCCESS (Explorer did not claim Win+T?)\n");
        UnregisterHotKey(nullptr, 1);
        return 1;
    }
    const DWORD err = GetLastError();
    printf("REGHOTKEY FAIL err=%lu%s\n", err,
           err == ERROR_HOTKEY_ALREADY_REGISTERED ? " (already registered - as expected)" : "");
    return 0;
}

// ------------------------------------------------------------------- --fire
static void sendVk(WORD vk, bool up) {
    INPUT in = {};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
    // dwExtraInfo deliberately 0: the hook must treat foreign injected input as real.
    SendInput(1, &in, sizeof(in));
}
static void pause(DWORD ms) { Sleep(ms); }

static int cmdFire(const wchar_t* seq, bool force) {
    if (!force) {
        LASTINPUTINFO lii = { sizeof(lii) };
        GetLastInputInfo(&lii);
        const DWORD idle = GetTickCount() - lii.dwTime;
        if (idle < 3000) {
            printf("FIRE-REFUSED user-active idle=%lums (use --force)\n", idle);
            return 3;
        }
    }
    if (wcscmp(seq, L"normal") == 0) {
        sendVk(VK_LWIN, false); pause(30);
        sendVk('T', false);     pause(30);
        sendVk('T', true);      pause(30);
        sendVk(VK_LWIN, true);
    } else if (wcscmp(seq, L"repeat") == 0) {
        sendVk(VK_LWIN, false); pause(30);
        for (int i = 0; i < 4; ++i) { sendVk('T', false); pause(20); } // autorepeat
        sendVk('T', true);      pause(30);
        sendVk(VK_LWIN, true);
    } else if (wcscmp(seq, L"shift") == 0) { // must pass through to Explorer untouched
        sendVk(VK_LWIN, false);   pause(30);
        sendVk(VK_LSHIFT, false); pause(30);
        sendVk('T', false);       pause(30);
        sendVk('T', true);        pause(30);
        sendVk(VK_LSHIFT, true);  pause(30);
        sendVk(VK_LWIN, true);
    } else if (wcscmp(seq, L"barewin") == 0) { // control: Start menu SHOULD open
        sendVk(VK_LWIN, false); pause(40);
        sendVk(VK_LWIN, true);
    } else if (wcscmp(seq, L"esc") == 0) {
        sendVk(VK_ESCAPE, false); pause(20);
        sendVk(VK_ESCAPE, true);
    } else {
        printf("FIRE-UNKNOWN %ls\n", seq);
        return 2;
    }
    printf("FIRED %ls\n", seq);
    return 0;
}

// -------------------------------------------------------------------- main
static BOOL WINAPI ctrlHandler(DWORD) {
    PostThreadMessageW(g_mainTid, WM_QUIT, 0, 0);
    return TRUE;
}

int wmain(int argc, wchar_t** argv) {
    if (argc >= 2) {
        if (wcscmp(argv[1], L"--reghotkey") == 0)   return cmdRegHotkey();
        if (wcscmp(argv[1], L"--check-start") == 0) return cmdCheckStart();
        if (wcscmp(argv[1], L"--wt-windows") == 0)  return cmdWtWindows();
        if (wcscmp(argv[1], L"--close-wt") == 0)    return cmdCloseWt(argc, argv, 2);
        if (wcscmp(argv[1], L"--fire") == 0 && argc >= 3)
            return cmdFire(argv[2], argc >= 4 && wcscmp(argv[3], L"--force") == 0);
    }

    DWORD timeoutSec = 600;
    for (int i = 1; i + 1 < argc; ++i)
        if (wcscmp(argv[i], L"--timeout") == 0)
            timeoutSec = static_cast<DWORD>(_wtoi(argv[i + 1]));

    g_mainTid = GetCurrentThreadId();
    SetConsoleCtrlHandler(ctrlHandler, TRUE);

    HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, hookProc, GetModuleHandleW(nullptr), 0);
    if (!hook) {
        printf("HOOK-FAIL err=%lu\n", GetLastError());
        return 1;
    }
    printf("HOOK-OK timeout=%lus (Win+T = terminal, Win+Alt+T = admin, Ctrl+C quits)\n", timeoutSec);
    fflush(stdout);

    SetTimer(nullptr, 1, timeoutSec * 1000, nullptr);

    int launches = 0;
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == MSG_LAUNCH) {
            ++launches;
            printf("MATCH %s (launch #%d)\n", msg.wParam ? "admin" : "normal", launches);
            fflush(stdout);
            LaunchSpec* spec = new LaunchSpec{ msg.wParam != 0, launches };
            HANDLE t = reinterpret_cast<HANDLE>(_beginthread(launchWorker, 0, spec));
            if (t == INVALID_HANDLE_VALUE) { printf("THREAD-FAIL\n"); delete spec; }
        } else if (msg.message == WM_TIMER) {
            printf("TIMEOUT\n");
            break;
        }
    }
    UnhookWindowsHookEx(hook);
    printf("EXIT launches=%d\n", launches);
    return 0;
}
