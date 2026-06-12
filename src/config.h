// Config lives at %APPDATA%\WinTea\config.ini (UTF-16LE + BOM so Unicode paths
// survive GetPrivateProfileStringW). First run writes a commented template.
#pragma once

#include <string>
#include <vector>

#include "chord.h"

namespace wintea {

struct Config {
    Chord        terminal;       // default win+t
    Chord        adminTerminal;  // default win+alt+t
    std::wstring command;        // empty => auto-detect wt/pwsh/powershell/cmd
    std::wstring args;           // used verbatim in override mode
    std::wstring workdir;        // default %USERPROFILE% (unexpanded here)
    bool         followExplorer = true;
    bool         notifications = true;
};

struct LoadedConfig {
    Config                    config;
    std::vector<std::wstring> warnings; // bad-chord/duplicate messages -> balloons
};

// %APPDATA%\WinTea (created on demand) and the config.ini path within it.
std::wstring ConfigDir();
std::wstring ConfigPath();

// Writes the commented default template only if the file does not yet exist.
bool EnsureConfigExists();

// Reads config.ini, falling back to defaults for any missing/invalid field.
LoadedConfig LoadConfig();

} // namespace wintea
