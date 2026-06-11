// Zero-dependency unit tests for the chord parser/formatter.
// Built as a console exe and run by ctest; nonzero exit = failure.
#define NOGDI        // drop the GDI Chord() function so it can't clash with wintea::Chord
#include <windows.h> // VK_* constants

#include "../src/chord.h"

#include <cstdio>
#include <string>

using namespace wintea;

static int g_failures = 0;

static void expect(bool cond, const char* what) {
    if (!cond) {
        printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

#define CHECK(cond) expect((cond), #cond)

int main() {
    // Basic parse.
    {
        auto p = ParseChord(L"win+t");
        CHECK(p.ok);
        CHECK(p.chord.mods == ModWin);
        CHECK(p.chord.vk == 'T');
        CHECK(p.chord.enabled());
    }
    // Admin chord, multiple modifiers, case/space insensitive.
    {
        auto p = ParseChord(L"  WIN + Alt + T ");
        CHECK(p.ok);
        CHECK(p.chord.mods == (ModWin | ModAlt));
        CHECK(p.chord.vk == 'T');
    }
    // The two default chords differ (admin must not collide with terminal).
    {
        CHECK(!(ParseChord(L"win+t").chord == ParseChord(L"win+alt+t").chord));
    }
    // Function keys, digits, named keys, vk escape.
    {
        CHECK(ParseChord(L"ctrl+shift+f12").chord.vk == VK_F12);
        CHECK(ParseChord(L"win+1").chord.vk == '1');
        CHECK(ParseChord(L"win+space").chord.vk == VK_SPACE);
        CHECK(ParseChord(L"win+enter").chord.vk == VK_RETURN);
        CHECK(ParseChord(L"win+vk20").chord.vk == 0x20);
    }
    // Modifier synonyms.
    {
        CHECK(ParseChord(L"super+t").chord.mods == ModWin);
        CHECK(ParseChord(L"control+t").chord.mods == ModCtrl);
    }
    // "none"/empty -> disabled but OK.
    {
        auto p = ParseChord(L"none");
        CHECK(p.ok);
        CHECK(!p.chord.enabled());
        auto e = ParseChord(L"   ");
        CHECK(e.ok);
        CHECK(!e.chord.enabled());
    }
    // Failures: no modifier, no key, two keys, junk.
    {
        CHECK(!ParseChord(L"t").ok);          // bare key
        CHECK(!ParseChord(L"win").ok);        // no key
        CHECK(!ParseChord(L"win+a+b").ok);    // two keys
        CHECK(!ParseChord(L"win+nope").ok);   // unknown token
    }
    // Format round-trips canonical order: Win+Ctrl+Alt+Shift.
    {
        CHECK(FormatChord(ParseChord(L"win+t").chord) == L"Win+T");
        CHECK(FormatChord(ParseChord(L"alt+win+t").chord) == L"Win+Alt+T");
        CHECK(FormatChord(ParseChord(L"shift+ctrl+f5").chord) == L"Ctrl+Shift+F5");
        CHECK(FormatChord(ParseChord(L"win+space").chord) == L"Win+Space");
        CHECK(FormatChord(Chord{}) == L"(disabled)");
    }
    // Re-parsing a formatted chord yields the same chord (round-trip).
    {
        for (const wchar_t* s : { L"win+t", L"win+alt+t", L"ctrl+shift+f5", L"win+1" }) {
            Chord a = ParseChord(s).chord;
            Chord b = ParseChord(FormatChord(a)).chord;
            CHECK(a == b);
        }
    }

    if (g_failures == 0) {
        printf("chord_test: all passed\n");
        return 0;
    }
    printf("chord_test: %d failure(s)\n", g_failures);
    return 1;
}
