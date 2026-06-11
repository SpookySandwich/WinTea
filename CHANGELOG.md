# Changelog

All notable changes to WinTea are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres
to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.9.0] - 2026-06-11

First public preview.

### Added
- `Win+T` opens a terminal; `Win+Alt+T` opens an admin terminal (UAC).
- Low-level keyboard hook with Start-menu suppression and autorepeat single-fire.
- Worker-thread launch so the hook never blocks on UAC.
- Auto-detect terminal chain: `wt` → `pwsh` → `powershell` → `cmd`, with an
  override mode and a broken-`wt`-alias guard.
- INI config at `%APPDATA%\WinTea\config.ini` with live auto-reload.
- Tray icon (theme-aware), context menu, enable/disable, run-at-startup.
- Single-instance with `--exit` (clean upgrades), `--version`, `--selftest`.
- Session/power resync so chords keep working after lock/unlock and sleep.
- Per-monitor-v2 DPI manifest; static single ~200 KB exe, zero dependencies.

[Unreleased]: https://github.com/SpookySandwich/WinTea/compare/v0.9.0...HEAD
[0.9.0]: https://github.com/SpookySandwich/WinTea/releases/tag/v0.9.0
