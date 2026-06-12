<div align="center">

<img src="assets/wintea-logo.png" width="128" alt="WinTea logo">

# WinTea

**Win+T(ea) — a fresh terminal, instantly. Win+Alt+T — brewed extra strong (admin).**

[![CI](https://github.com/SpookySandwich/WinTea/actions/workflows/ci.yml/badge.svg)](https://github.com/SpookySandwich/WinTea/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/SpookySandwich/WinTea?sort=semver)](https://github.com/SpookySandwich/WinTea/releases)
[![Downloads](https://img.shields.io/github/downloads/SpookySandwich/WinTea/total)](https://github.com/SpookySandwich/WinTea/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![Single exe](https://img.shields.io/badge/single%20exe-~200%20KB-success)
![Dependencies](https://img.shields.io/badge/dependencies-zero-success)

</div>

<div align="center">

<!-- TODO: record assets/demo.gif (see CONTRIBUTING.md). Until then this link 404s. -->
<img src="assets/demo.gif" width="720" alt="WinTea demo: Win+T opens a terminal, Win+Alt+T opens an admin terminal">

</div>

---

**WinTea** sits in your tray and gives you two hotkeys:

| Press | You get |
| --- | --- |
| **`Win` + `T`** | a fresh terminal window, opened in the focused File Explorer folder when possible |
| **`Win` + `Alt` + `T`** | a terminal window **as administrator** (one UAC prompt), also following Explorer when possible |

That's it. One ~200 KB executable, an optional per-user installer, no .NET, no runtime,
**no network access at all**.

### Why it exists

- **Muscle memory beats mousing.** A terminal is a keystroke away from anywhere — no Start menu, no pinned-icon hunting.
- **The fastest admin shell on Windows.** `Win+Alt+T` is the shortest path to an elevated prompt that exists.
- **Small and honest.** ~1,100 lines of C++ you can read over one cup of tea. It composites no telemetry and phones nobody home.

---

## Install

**Installer (easiest).** Download **`WinTea-Setup-x.y.z.exe`** from the [latest release](https://github.com/SpookySandwich/WinTea/releases) and run it. It's a per-user install — **no admin prompt** — that adds a Start Menu entry with the teacup icon and offers to start WinTea with Windows.

**Portable.** Prefer a single file? Grab **`WinTea.exe`** from the [latest release](https://github.com/SpookySandwich/WinTea/releases), drop it anywhere, and turn on autostart from the tray menu.

**Package managers.**

```powershell
winget install WinTea     # after the first stable release
scoop install https://raw.githubusercontent.com/SpookySandwich/WinTea/master/packaging/scoop/wintea.json
```

<details>
<summary>Verify the download (recommended for any keyboard-hook tool)</summary>

```powershell
# checksum matches the published SHA256SUMS.txt
Get-FileHash .\WinTea.exe -Algorithm SHA256

# and/or verify it was built by this repo's CI, not tampered with:
gh attestation verify .\WinTea.exe -R SpookySandwich/WinTea
```
</details>

To start it with Windows, right-click the tray icon → **Start with Windows**.

---

## Usage

Run `WinTea.exe`. A teacup appears in your tray. Now:

- **`Win+T`** → terminal. **`Win+Alt+T`** → admin terminal.
- **Left-click** the tray icon to quickly enable/disable.
- **Right-click** for the menu (enable, autostart, open/reload config, about, quit).

### Configuration

Everything lives in a plain INI at **`%APPDATA%\WinTea\config.ini`** (created on first run). Save it and WinTea reloads automatically — no restart.

```ini
[hotkeys]
; chord = modifiers + one key, e.g. win+t, win+alt+t, ctrl+shift+f12
; modifiers: win ctrl alt shift
; keys: a-z  0-9  f1-f24  space tab enter esc backspace  vkXX (hex)
; set a hotkey to "none" to disable it
terminal       = win+t
admin_terminal = win+alt+t

[launch]
; command empty = auto-detect: wt -> pwsh -> powershell -> cmd
; if you set command, args are used verbatim (env vars like %USERPROFILE% expand)
; follow_explorer uses the focused File Explorer folder as the launch directory
command =
args    =
workdir = %USERPROFILE%
follow_explorer = true

[ui]
; tray balloons for errors and config reloads
notifications = true
```

Want PowerShell 7 in a specific folder instead of Windows Terminal?

```ini
[launch]
command = pwsh.exe
args    = -NoLogo
workdir = C:\dev
follow_explorer = false
```

---

## How it works

`Win+T` is already claimed by Explorer (it cycles taskbar buttons), so a normal global hotkey
registration for it simply **fails**. WinTea instead installs a **low-level keyboard hook**
(`WH_KEYBOARD_LL`) — the same primitive PowerToys and AutoHotkey use — sees the `Win+T` key event
*before* Explorer does, swallows it, and launches your terminal.

The subtle part is **not flashing the Start menu**: Windows opens Start when the `Win` key goes down
and up with nothing "interesting" in between. After swallowing `T`, WinTea injects a throwaway key so
the `Win` press is no longer "clean," and Start stays shut. The launch itself happens on a worker
thread, because the UAC prompt blocks — and a blocked keyboard hook would freeze typing for the whole
system.

The full story, with diagrams, is in **[docs/HOW-IT-WORKS.md](docs/HOW-IT-WORKS.md)**.

---

## Limitations (honest)

- **Elevated windows are a dead zone.** While a window running *as administrator* has focus, a normal-privilege hook receives no keys, so the hotkey won't fire there. This is Windows' UIPI security boundary, by design. (Roadmap: an opt-in elevated mode.)
- **Last keyboard tool to start wins.** If PowerToys or AutoHotkey also bind `Win+T`, whichever hooked most recently sees the event first.
- **AltGr layouts:** use the *left* `Alt` for the admin chord (AltGr injects a Ctrl, which the strict matcher rejects).
- **Sticky Keys** latched modifiers aren't supported (your typing is unaffected).

---

## Alternatives

WinTea is small on purpose. If one of these fits you better, use it — no hard feelings.

| Alternative | Use it if… | Caveat |
| --- | --- | --- |
| AutoHotkey: `#t::Run "wt"` | you already run AHK | needs the AHK runtime; menu-mask tuning to kill the Start flash |
| PowerToys Keyboard Manager "Run Program" | you already run PowerToys | program-launch elevation has open bugs ([#32206](https://github.com/microsoft/PowerToys/issues/32206), [#37300](https://github.com/microsoft/PowerToys/issues/37300)); much heavier |
| Windows Terminal "quake mode" (`Win`+`` ` ``) | you want one drop-down terminal | a single shared window, no per-press new windows, no admin chord |
| `Win+X` then `A` | you can't install anything | two-step, and opens whatever the Win+X default shell is |

---

## FAQ

**Why doesn't the Start menu flash when I press Win+T?**
WinTea injects a throwaway key after swallowing `T`, so Windows doesn't treat the `Win` press as a "tap." See [How it works](docs/HOW-IT-WORKS.md).

**Why does the admin terminal ask for UAC every time?**
Because WinTea has no resident elevated helper — that's the security posture, not an oversight. A background service that could spawn admin shells without a prompt is exactly the thing you don't want running.

**Nothing happens when an admin app is focused.**
Right — a normal-privilege program can't intercept keys destined for an elevated window (Windows UIPI). Click a non-elevated window and try again.

**I use Win+T to cycle taskbar buttons!**
Then rebind WinTea's `terminal` chord (e.g. `ctrl+alt+t`) or set it to `none`. If you live by taskbar cycling, WinTea may just not be for you — that's fine.

**My antivirus flagged it.**
Keyboard hooks + injected input + a Run-key entry is the classic *shape* of a keylogger, so heuristic scanners get nervous. WinTea has **zero networking code** (grep the source for `winhttp`/`winsock` — nothing), is built by public CI with [build provenance](https://github.com/SpookySandwich/WinTea/releases) you can verify, and is ~1,100 lines you can read. You can also try it in [Windows Sandbox](tools/wintea-sandbox.wsb) first.

**Does it phone home?**
No. There is no network code in the program at all.

---

## Building

Windows + MSVC + CMake + Ninja:

```powershell
cmake --preset release
cmake --build --preset release
ctest --preset release          # runs the chord-parser unit tests
```

Output: `build/release/WinTea.exe`. Regenerate icons after editing the art with
`pwsh tools/make-icons.ps1`.

---

## Roadmap

- Opt-in **elevated mode** (scheduled-task autostart + a de-elevation broker, so `Win+T` stays non-admin while still working over elevated windows).
- A small **settings window** (hotkey picker, terminal dropdown) for non-INI folks.
- **Summon/focus-existing** mode as an alternative to always opening a new window.
- **winget** submission; **ARM64** build.

Contributions welcome — see [CONTRIBUTING.md](CONTRIBUTING.md).

## License

MIT. *Best served hot.*
