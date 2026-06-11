#include "autostart.h"

#include <windows.h>

#include <string>

namespace wintea {
namespace {

constexpr wchar_t kRunKey[]   = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"WinTea";

std::wstring quotedExePath() {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (n == 0 || n == MAX_PATH) return L"";
    return L"\"" + std::wstring(path, n) + L"\"";
}

} // namespace

bool IsAutostartEnabled() {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return false;

    wchar_t buf[MAX_PATH + 2];
    DWORD size = sizeof(buf);
    DWORD type = 0;
    bool present = RegQueryValueExW(key, kValueName, nullptr, &type,
                                   reinterpret_cast<BYTE*>(buf), &size) == ERROR_SUCCESS;
    RegCloseKey(key);

    if (!present || type != REG_SZ) return false;
    // Treat a stale value (exe was moved) as "not enabled" so the menu offers to re-set it.
    return _wcsicmp(buf, quotedExePath().c_str()) == 0;
}

bool SetAutostartEnabled(bool on) {
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;

    LONG r;
    if (on) {
        std::wstring cmd = quotedExePath();
        r = RegSetValueExW(key, kValueName, 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(cmd.c_str()),
                           static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        r = RegDeleteValueW(key, kValueName);
        if (r == ERROR_FILE_NOT_FOUND) r = ERROR_SUCCESS; // already absent = success
    }
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

} // namespace wintea
