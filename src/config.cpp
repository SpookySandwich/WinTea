#include "config.h"

#include <windows.h>
#include <shlobj.h>

#include <cstdio>

#pragma comment(lib, "shell32.lib")

namespace wintea {
namespace {

constexpr wchar_t kTemplate[] =
    L"; WinTea config - edit and save; it reloads automatically.\r\n"
    L"; (or use the tray menu: Reload config)\r\n"
    L"\r\n"
    L"[hotkeys]\r\n"
    L"; chord = modifiers + one key, e.g. win+t, win+alt+t, ctrl+shift+f12\r\n"
    L"; modifiers: win ctrl alt shift\r\n"
    L"; keys: a-z  0-9  f1-f24  space tab enter esc backspace  vkXX (hex)\r\n"
    L"; set a hotkey to \"none\" to disable it\r\n"
    L"terminal       = win+t\r\n"
    L"admin_terminal = win+alt+t\r\n"
    L"\r\n"
    L"[launch]\r\n"
    L"; command empty = auto-detect: wt -> pwsh -> powershell -> cmd\r\n"
    L"; if you set command, args are used verbatim (env vars like %USERPROFILE% expand)\r\n"
    L"command =\r\n"
    L"args    =\r\n"
    L"workdir = %USERPROFILE%\r\n"
    L"\r\n"
    L"[ui]\r\n"
    L"; tray balloons for errors and config reloads\r\n"
    L"notifications = true\r\n";

std::wstring readString(const wchar_t* section, const wchar_t* key,
                        const wchar_t* def, const std::wstring& path) {
    wchar_t buf[1024];
    GetPrivateProfileStringW(section, key, def, buf, 1024, path.c_str());
    return buf;
}

bool readBool(const wchar_t* section, const wchar_t* key, bool def,
              const std::wstring& path) {
    std::wstring v = readString(section, key, def ? L"true" : L"false", path);
    return v == L"true" || v == L"1" || v == L"yes" || v == L"on";
}

// Parses a chord string from the ini; on failure keeps `fallback` and records why.
Chord readChord(const wchar_t* key, const Chord& fallback,
                const std::wstring& path, std::vector<std::wstring>& warnings) {
    std::wstring raw = readString(L"hotkeys", key, L"", path);
    if (raw.empty()) return fallback;
    ChordParse p = ParseChord(raw);
    if (!p.ok) {
        warnings.push_back(L"hotkeys/" + std::wstring(key) + L": " + p.error +
                           L" - keeping previous binding");
        return fallback;
    }
    return p.chord;
}

} // namespace

std::wstring ConfigDir() {
    PWSTR appData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
        std::wstring dir = std::wstring(appData) + L"\\WinTea";
        CoTaskMemFree(appData);
        return dir;
    }
    return L".";
}

std::wstring ConfigPath() {
    return ConfigDir() + L"\\config.ini";
}

bool EnsureConfigExists() {
    std::wstring dir = ConfigDir();
    CreateDirectoryW(dir.c_str(), nullptr); // ok if it already exists

    std::wstring path = ConfigPath();
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
        return true; // present already

    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    const unsigned char bom[2] = { 0xFF, 0xFE }; // UTF-16LE BOM
    DWORD written = 0;
    WriteFile(h, bom, sizeof(bom), &written, nullptr);
    WriteFile(h, kTemplate, static_cast<DWORD>(wcslen(kTemplate) * sizeof(wchar_t)),
              &written, nullptr);
    CloseHandle(h);
    return true;
}

LoadedConfig LoadConfig() {
    LoadedConfig out;
    std::wstring path = ConfigPath();

    Config& c = out.config;
    c.terminal      = ParseChord(L"win+t").chord;
    c.adminTerminal = ParseChord(L"win+alt+t").chord;

    c.terminal      = readChord(L"terminal", c.terminal, path, out.warnings);
    c.adminTerminal = readChord(L"admin_terminal", c.adminTerminal, path, out.warnings);

    if (c.terminal.enabled() && c.adminTerminal.enabled() &&
        c.terminal == c.adminTerminal) {
        out.warnings.push_back(L"terminal and admin_terminal are the same chord; "
                               L"disabling admin_terminal");
        c.adminTerminal = Chord{};
    }

    c.command = readString(L"launch", L"command", L"", path);
    c.args    = readString(L"launch", L"args", L"", path);
    c.workdir = readString(L"launch", L"workdir", L"%USERPROFILE%", path);
    c.notifications = readBool(L"ui", L"notifications", true, path);
    return out;
}

} // namespace wintea
