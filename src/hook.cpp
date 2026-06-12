#include "hook.h"

#include "app.h"

namespace wintea {
namespace {

// --- state, all touched only on the hook/UI thread ---
HHOOK g_hook = nullptr;
HWND  g_target = nullptr;
Chord g_terminal{};
Chord g_admin{};

bool  g_tracked[256] = {}; // physical down-state for the 8 modifier VKs we care about
UINT  g_swallowedVk = 0;   // chord key currently being eaten (autorepeat + key-up)

bool isModifierVk(DWORD vk) {
    switch (vk) {
    case VK_LWIN: case VK_RWIN:
    case VK_LMENU: case VK_RMENU:
    case VK_LCONTROL: case VK_RCONTROL:
    case VK_LSHIFT: case VK_RSHIFT:
        return true;
    default:
        return false;
    }
}

// A modifier counts as down only if both our tracker AND the async state agree.
// The async-AND repairs state when a key-up was lost on the secure desktop
// (UAC), where releases never reach the hook but async state still updates.
bool down(int vk) {
    return g_tracked[vk] && (GetAsyncKeyState(vk) & 0x8000) != 0;
}

uint8_t effectiveMods(const KBDLLHOOKSTRUCT* kb) {
    uint8_t m = 0;
    if (down(VK_LWIN) || down(VK_RWIN))         m |= ModWin;
    if (down(VK_LCONTROL) || down(VK_RCONTROL)) m |= ModCtrl;
    if (down(VK_LMENU) || down(VK_RMENU) || (kb->flags & LLKHF_ALTDOWN)) m |= ModAlt;
    if (down(VK_LSHIFT) || down(VK_RSHIFT))     m |= ModShift;
    return m;
}

// Dirties the pending Win press so Explorer doesn't open Start on Win-up.
void sendDummyKey() {
    INPUT in[2] = {};
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = 0xFF; // reserved VK; harmless, never produces a character
    in[0].ki.dwExtraInfo = kInjectSentinel;
    in[1] = in[0];
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
}

LRESULT CALLBACK proc(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION)
        return CallNextHookEx(nullptr, code, wParam, lParam);

    const KBDLLHOOKSTRUCT* kb = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);

    // Skip ONLY our own injected dummy key. Other injected input (macro pads,
    // RDP, the --selftest harness) must be processed as if real.
    if (kb->dwExtraInfo == kInjectSentinel)
        return CallNextHookEx(nullptr, code, wParam, lParam);

    const bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    if (isModifierVk(kb->vkCode))
        g_tracked[kb->vkCode] = isDown;

    // Swallow gate runs before matching: eat autorepeats (single-fire) and the
    // trailing key-up so no stray characters/key-ups leak to the focused app.
    if (g_swallowedVk != 0 && kb->vkCode == g_swallowedVk) {
        if (!isDown) g_swallowedVk = 0;
        return 1;
    }

    if (isDown) {
        uint8_t mods = effectiveMods(kb);
        const Chord pressed{ mods, static_cast<uint16_t>(kb->vkCode) };
        bool match = (g_terminal.enabled() && pressed == g_terminal) ||
                     (g_admin.enabled() && pressed == g_admin);
        if (match) {
            const bool admin = g_admin.enabled() && pressed == g_admin;
            g_swallowedVk = static_cast<UINT>(kb->vkCode);
            sendDummyKey();
            if (g_target)
                PostMessageW(g_target, WM_APP_LAUNCH, admin ? 1 : 0,
                             reinterpret_cast<LPARAM>(GetForegroundWindow()));
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

} // namespace

bool InstallHook(HWND target) {
    g_target = target;
    if (g_hook) return true;
    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, proc, GetModuleHandleW(nullptr), 0);
    ResetModifierState();
    return g_hook != nullptr;
}

void UninstallHook() {
    if (g_hook) {
        UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
    }
    ResetModifierState();
    g_swallowedVk = 0;
}

bool IsHookInstalled() { return g_hook != nullptr; }

void ConfigureHook(const Chord& terminal, const Chord& admin) {
    g_terminal = terminal;
    g_admin = admin;
}

void ResetModifierState() {
    for (bool& b : g_tracked) b = false;
}

} // namespace wintea
