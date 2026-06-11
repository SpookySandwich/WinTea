# How WinTea works

> How Windows decides to open the Start menu, and how to politely stop it.

WinTea is small, but a few of its details are load-bearing and non-obvious. This is the tour.

## 1. Why `Win+T` needs a hook at all

The obvious way to claim a hotkey on Windows is [`RegisterHotKey`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey). It doesn't work for `Win+T`, because Explorer already registered it (it cycles taskbar buttons). You can prove this in five lines:

```cpp
if (!RegisterHotKey(nullptr, 1, MOD_WIN, 'T'))
    // GetLastError() == ERROR_HOTKEY_ALREADY_REGISTERED (1409)
```

So WinTea goes one level lower, to a **low-level keyboard hook**:

```cpp
SetWindowsHookExW(WH_KEYBOARD_LL, hookProc, hInstance, 0);
```

A `WH_KEYBOARD_LL` hook sees every key event *before* the system hotkey machinery matches it. Returning `1` from the hook swallows the event — Explorer's `Win+T` never matches. This is exactly how PowerToys Keyboard Manager and AutoHotkey override system shortcuts.

## 2. The hook must be nearly instant

Every keystroke on the machine flows through your hook synchronously. Two consequences:

- **You get a deadline.** If your hook takes longer than `HKCU\Control Panel\Desktop\LowLevelHooksTimeout` (capped at 1000 ms), Windows **silently removes it** — no error, no callback. So the hook does the absolute minimum: flip some state, inject one key, post one message. Everything expensive happens elsewhere.
- **You must not block.** Which brings us to the launch.

## 3. Launching can't happen on the hook thread

The admin terminal uses `ShellExecuteExW` with the `runas` verb, which **blocks until the user answers the UAC prompt**. If that ran on the hook thread, every keystroke system-wide would queue behind a modal dialog — and then the hook would time out and get removed.

So the hook only does `PostMessage(WM_APP_LAUNCH, …)`. The window procedure hands a fully-resolved launch spec to a **detached worker thread**, which does the slow `ShellExecuteExW` and the UAC wait. The hook thread is free again in microseconds.

```
key event ─▶ hook (µs) ─▶ PostMessage ─▶ window proc ─▶ worker thread ─▶ ShellExecuteEx
              │                                                              (UAC wait here)
              └─ returns 1 immediately, system input unblocked
```

## 4. Suppressing the Start menu

Here's the subtle one. Windows opens the Start menu when the `Win` key is pressed and released **with nothing meaningful in between** — a "clean tap." When WinTea swallows the `T`, the system still sees `Win` down … `Win` up, looking like a clean tap, and Start pops up.

The fix (used by PowerToys and AHK both) is to make the `Win` press *dirty*: inject a throwaway keystroke after swallowing `T`.

```cpp
INPUT dummy[2] = {};
dummy[0].type = INPUT_KEYBOARD;
dummy[0].ki.wVk = 0xFF;                       // a reserved VK; produces no character
dummy[0].ki.dwExtraInfo = kInjectSentinel;    // tag it so our own hook ignores it
dummy[1] = dummy[0];
dummy[1].ki.dwFlags = KEYEVENTF_KEYUP;
SendInput(2, dummy, sizeof(INPUT));
```

Now the `Win` sequence contains a keystroke, so it's no longer a clean tap, and Start stays closed.

Two things make this safe:

- The injected key is tagged with a **sentinel** in `dwExtraInfo` (`0x57696E54`, "WinT"). The hook skips events carrying that exact tag — but **only** that tag. It does *not* skip everything marked `LLKHF_INJECTED`, because macro pads, Remote Desktop, and test harnesses send legitimate injected input that should still work.
- `0xFF` is a reserved virtual-key that types nothing.

## 5. Reading modifiers correctly

You'd think you could ask `GetAsyncKeyState` whether `Alt` is down. But [the docs warn](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc) that inside the hook callback, the async state for the *current* key isn't updated yet. So WinTea tracks modifier up/down itself, from the events it sees.

That tracking can go stale in exactly one place: the **secure desktop**. When the UAC prompt appears, it's on a separate desktop, and key-ups that happen there never reach your hook. Press `Win+Alt+T`, release the keys while the prompt is up, and your tracker still thinks `Win+Alt` are held.

WinTea corrects this two ways:

1. A modifier counts as down only if **both** the tracker and `GetAsyncKeyState` agree (async state *is* valid for keys other than the current event, and it does get updated by releases on the secure desktop).
2. On session lock/unlock and resume-from-sleep, it clears tracked state and re-installs the hook (those are also the moments a hook is most likely to have been silently removed).

## 6. Matching is strict

The chord matcher requires **exact** equality across all four modifier classes (`win`, `ctrl`, `alt`, `shift`). `Win+T` and `Win+Alt+T` are distinct; `Win+Shift+T` matches neither and passes straight through to whatever wanted it. This is what keeps WinTea from stomping on shortcuts it wasn't asked to handle.

## 7. The launch fallback chain

In auto mode WinTea tries terminals in order until one starts: `wt.exe` → `pwsh.exe` → `powershell.exe` → `cmd.exe`, resolved via `SearchPathW` (which includes the `WindowsApps` alias folder where `wt.exe` lives).

`wt.exe` is a quirk: it hands off to `WindowsTerminal.exe` and exits almost immediately with code 0. A *non-zero* fast exit means the alias is broken (this actually happened to people after a January 2025 Windows update) — so after launching `wt`, WinTea briefly checks its exit code and falls through to the next terminal if it died. It also passes `-d "<workdir>"` because `wt` ignores the working directory when elevated.

---

That's the whole trick. The rest is tray plumbing, an INI parser, and a file-change watcher. Read [`src/hook.cpp`](../src/hook.cpp) — it's the most interesting ~150 lines in the project.
