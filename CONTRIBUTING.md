# Contributing to WinTea

Thanks for taking a look! WinTea is deliberately tiny — the goal is a tool you can
read in full over one cup of tea (~1,100 lines of C++). Please keep changes in that
spirit.

## Building

Windows + MSVC + CMake (3.21+) + Ninja:

```powershell
cmake --preset release
cmake --build --preset release
ctest --preset release
```

`build/release/WinTea.exe` is the result. There is also a `debug` preset.

## Project layout

| Path | What |
| --- | --- |
| `src/hook.cpp` | the low-level keyboard hook — the heart of the project |
| `src/launch.cpp` | worker-thread terminal launching, UAC, fallback chain |
| `src/chord.cpp` | chord parsing/formatting (unit-tested in `tests/`) |
| `src/config.cpp` | INI load + first-run template |
| `src/tray.cpp` | tray icon, menu, balloons, theme-aware glyph |
| `src/main.cpp` | window, message loop, config watcher, lifecycle |
| `spike/` | the throwaway mechanism spike used to validate the approach |
| `docs/HOW-IT-WORKS.md` | the design deep-dive |

## Style

- Run `clang-format` (config in `.clang-format`) before committing.
- Match the existing comment density: comment the *why*, especially the Win32
  gotchas — those are the reason this code is worth reading.
- Keep the hook callback allocation-free and non-blocking. If you're tempted to do
  real work there, post a message instead.

## Testing changes to the hook/launch path

The visual/interactive bits can't be fully automated, but a lot can:

- `ctest --preset release` runs the chord-parser tests.
- The `spike` target re-checks the core mechanism headlessly:
  ```powershell
  build/release/spike.exe --reghotkey        # confirms Explorer owns Win+T
  build/release/spike.exe --timeout 60       # run the hook; in another shell:
  build/release/spike.exe --fire normal      # inject Win+T, expect a terminal
  build/release/spike.exe --check-start      # expect START-CLOSED (no flash)
  ```
- Manual checklist for a hook/launch PR: no Start-menu flash; `Win+Shift+T` passes
  through; autorepeat fires once; UAC decline is silent and leaves no stuck
  modifiers; typing stays fluid while a UAC prompt is open.

## Recording the demo GIF

`assets/demo.gif` is referenced by the README and still needs recording:

1. Use [ScreenToGif](https://www.screentogif.com/) (free; has a keystroke overlay).
2. ~12 seconds at 1280×720: clean desktop → `Win+T` (keys shown on screen) →
   terminal appears → `Win+Alt+T` → UAC → an "Administrator:" terminal → end.
3. Keep it under ~4 MB (use the gifski export). Drop it at `assets/demo.gif`.

## Pull requests

Small, focused PRs are easiest to review. If you're changing behavior, update the
README/CHANGELOG and, where you can, add a chord test or a spike check.
