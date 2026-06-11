#include "chord.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <vector>

namespace wintea {
namespace {

std::wstring lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return s;
}

std::wstring trim(const std::wstring& s) {
    size_t a = s.find_first_not_of(L" \t\r\n");
    if (a == std::wstring::npos) return L"";
    size_t b = s.find_last_not_of(L" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<std::wstring> splitPlus(const std::wstring& s) {
    std::vector<std::wstring> parts;
    std::wstring cur;
    for (wchar_t c : s) {
        if (c == L'+') {
            parts.push_back(trim(cur));
            cur.clear();
        } else {
            cur += c;
        }
    }
    parts.push_back(trim(cur));
    return parts;
}

// Named non-letter/digit keys the config may reference.
struct Named { const wchar_t* name; uint16_t vk; };
constexpr std::array<Named, 8> kNamed = {{
    {L"space", VK_SPACE},   {L"tab", VK_TAB},        {L"enter", VK_RETURN},
    {L"return", VK_RETURN}, {L"esc", VK_ESCAPE},     {L"escape", VK_ESCAPE},
    {L"backspace", VK_BACK},{L"delete", VK_DELETE},
}};

// Resolves a single key token (already lowercased) to a VK, or 0 on failure.
uint16_t resolveKey(const std::wstring& tok) {
    if (tok.size() == 1) {
        wchar_t c = tok[0];
        if (c >= L'a' && c <= L'z') return static_cast<uint16_t>(L'A' + (c - L'a'));
        if (c >= L'0' && c <= L'9') return static_cast<uint16_t>(c);
    }
    if (tok.size() >= 2 && tok[0] == L'f' && iswdigit(tok[1])) {
        int n = _wtoi(tok.c_str() + 1);
        if (n >= 1 && n <= 24) return static_cast<uint16_t>(VK_F1 + (n - 1));
    }
    for (const Named& n : kNamed)
        if (tok == n.name) return n.vk;
    if (tok.size() > 2 && tok[0] == L'v' && tok[1] == L'k') {
        wchar_t* end = nullptr;
        long v = wcstol(tok.c_str() + 2, &end, 16);
        if (end && *end == L'\0' && v > 0 && v <= 0xFF) return static_cast<uint16_t>(v);
    }
    return 0;
}

uint8_t resolveMod(const std::wstring& tok) {
    if (tok == L"win" || tok == L"super" || tok == L"meta") return ModWin;
    if (tok == L"ctrl" || tok == L"control")                return ModCtrl;
    if (tok == L"alt")                                      return ModAlt;
    if (tok == L"shift")                                    return ModShift;
    return 0;
}

} // namespace

ChordParse ParseChord(const std::wstring& text) {
    ChordParse out;
    std::wstring norm = lower(trim(text));
    if (norm.empty() || norm == L"none" || norm == L"off") {
        out.ok = true; // explicitly disabled
        return out;
    }

    Chord chord;
    bool haveKey = false;
    for (const std::wstring& tok : splitPlus(norm)) {
        if (tok.empty()) continue;
        if (uint8_t m = resolveMod(tok)) {
            chord.mods |= m;
            continue;
        }
        if (uint16_t vk = resolveKey(tok)) {
            if (haveKey) {
                out.error = L"more than one non-modifier key in \"" + trim(text) + L"\"";
                return out;
            }
            chord.vk = vk;
            haveKey = true;
            continue;
        }
        out.error = L"unknown key \"" + tok + L"\" in \"" + trim(text) + L"\"";
        return out;
    }

    if (!haveKey) {
        out.error = L"no key in \"" + trim(text) + L"\" (need e.g. win+t)";
        return out;
    }
    if (chord.mods == 0) {
        out.error = L"\"" + trim(text) + L"\" has no modifier (a bare key would hijack typing)";
        return out;
    }
    out.ok = true;
    out.chord = chord;
    return out;
}

std::wstring FormatChord(const Chord& chord) {
    if (!chord.enabled()) return L"(disabled)";
    std::wstring s;
    if (chord.mods & ModWin)   s += L"Win+";
    if (chord.mods & ModCtrl)  s += L"Ctrl+";
    if (chord.mods & ModAlt)   s += L"Alt+";
    if (chord.mods & ModShift) s += L"Shift+";

    for (const Named& n : kNamed) {
        if (n.vk == chord.vk) {
            std::wstring name = n.name;
            name[0] = static_cast<wchar_t>(towupper(name[0]));
            return s + name;
        }
    }
    if (chord.vk >= VK_F1 && chord.vk <= VK_F24)
        return s + L"F" + std::to_wstring(chord.vk - VK_F1 + 1);
    if ((chord.vk >= L'A' && chord.vk <= L'Z') || (chord.vk >= L'0' && chord.vk <= L'9'))
        return s + static_cast<wchar_t>(chord.vk);

    wchar_t buf[8];
    swprintf(buf, 8, L"vk%02X", chord.vk);
    return s + buf;
}

} // namespace wintea
