#include <doctest/doctest.h>
#include "input/HotkeyRules.h"

namespace {
    using namespace mod::hotkey_rules;
    constexpr std::uint32_t KeyE = 0x12;

    TEST_CASE("Escape cancels rebinding without leaking its release to the menu") {
        CaptureEscape escape;
        CHECK_FALSE(escape.Consume(false, true, false));
        CHECK(escape.Consume(true, true, false));
        // Capture ends on key-down; held and release events still belong to it.
        CHECK(escape.Consume(false, false, false));
        CHECK(escape.Consume(false, false, true));
        CHECK_FALSE(escape.Consume(false, true, false));
        CHECK_FALSE(escape.Consume(false, false, true));
    }

    TEST_CASE("Hotkeys require the exact modifiers and accept either Shift key") {
        for (const auto shift : {0x2Au, 0x36u}) {
            Keyboard keyboard;
            CHECK_FALSE(keyboard.Update(shift, true, true));
            CHECK(keyboard.Update(KeyE, true, true) == (Shift | KeyE));
            CHECK_FALSE(keyboard.Update(KeyE, false, false));
            CHECK_FALSE(keyboard.Update(0x1D, true, true));
            CHECK(keyboard.Update(KeyE, true, true) == (Shift | Control | KeyE));
        }
    }

    TEST_CASE("Held keys and releases do not repeat commands") {
        Keyboard keyboard;
        CHECK(keyboard.Update(KeyE, true, true) == KeyE);
        CHECK_FALSE(keyboard.Update(KeyE, true, false));
        CHECK_FALSE(keyboard.Update(KeyE, true, true));
        CHECK_FALSE(keyboard.Update(KeyE, false, false));
        CHECK(keyboard.Update(KeyE, true, true) == KeyE);
        keyboard.Reset();
        CHECK_FALSE(keyboard.Update(KeyE, true, false));
        CHECK_FALSE(keyboard.Update(256, true, true));
    }

    TEST_CASE("Unbound and distinct chords do not conflict") {
        Bindings bindings{};
        bindings[0] = Shift | KeyE;
        CHECK(Conflict(bindings, Shift | KeyE) == 0);
        CHECK_FALSE(Conflict(bindings, Shift | KeyE, 0));
        CHECK_FALSE(Conflict(bindings, KeyE));
        CHECK_FALSE(Conflict(bindings, Shift | Control | KeyE));
        CHECK_FALSE(Conflict(bindings, 0));
        CHECK(Valid(0));
        CHECK(Valid(Shift | Control | Alt | KeyE));
        for (const auto invalid : {Escape, Shift, Shift | 0x2A, 0xDBu, 0xDCu, 0xDDu, 0x800u})
            CHECK_FALSE(Valid(invalid));
    }

    TEST_CASE("Windows shortcuts never fall back to unmodified bindings") {
        for (const auto windows : {0xDBu, 0xDCu}) {
            Keyboard keyboard;
            CHECK_FALSE(keyboard.Update(windows, true, true));
            CHECK_FALSE(keyboard.Update(KeyE, true, true));
            keyboard.Update(KeyE, false, false);
            keyboard.Update(windows, false, false);
            CHECK(keyboard.Update(KeyE, true, true) == KeyE);
        }
    }

    TEST_CASE("Hotkeys are disabled during menus, text entry and game transitions") {
        CHECK(GameplayAllowed(true, false, false, false, true));
        CHECK_FALSE(GameplayAllowed(false, false, false, false, true));
        CHECK_FALSE(GameplayAllowed(true, true, false, false, true));
        CHECK_FALSE(GameplayAllowed(true, false, true, false, true));
        CHECK_FALSE(GameplayAllowed(true, false, false, true, true));
        CHECK_FALSE(GameplayAllowed(true, false, false, false, false));
    }
}
