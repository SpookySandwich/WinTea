// Chord = a set of modifier keys + one ordinary key (e.g. Win+Alt+T).
//
// This is the one module with no Windows-runtime side effects, so it is the one
// we unit-test (tests/chord_test.cpp). Parsing/formatting must round-trip.
#pragma once

#include <cstdint>
#include <string>

namespace wintea {

// Note: names are PascalCase to avoid clashing with the MOD_WIN/MOD_ALT/etc.
// hotkey macros in <winuser.h>.
enum ModBit : uint8_t {
    ModWin   = 1 << 0,
    ModCtrl  = 1 << 1,
    ModAlt   = 1 << 2,
    ModShift = 1 << 3,
};

struct Chord {
    uint8_t  mods = 0; // ModBit mask
    uint16_t vk   = 0; // virtual-key code; 0 means "disabled"

    bool enabled() const { return vk != 0; }
    bool operator==(const Chord& o) const { return mods == o.mods && vk == o.vk; }
};

struct ChordParse {
    bool         ok = false;
    Chord        chord;
    std::wstring error; // human-readable, used in a warning balloon
};

// Parses "win+alt+t", "ctrl+shift+f12", "win+vk20", or "none" (-> disabled, ok).
// Rules: at least one modifier, exactly one non-modifier key.
ChordParse ParseChord(const std::wstring& text);

// Renders a chord as "Win+Alt+T" (for tooltips/menus). Disabled -> "(disabled)".
std::wstring FormatChord(const Chord& chord);

} // namespace wintea
