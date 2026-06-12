// Helpers for reading filesystem context from Windows Shell UI.
#pragma once

#include <windows.h>

#include <string>

namespace wintea {

// If `foreground` is an Explorer window, returns the active folder path.
// Virtual shell locations and non-Explorer windows return an empty string.
std::wstring ExplorerDirectoryFromWindow(HWND foreground);

} // namespace wintea
