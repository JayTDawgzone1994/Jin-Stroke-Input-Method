#include "../../src/windows/tsf/keyboard_policy.hpp"
#include <iostream>

int main() {
    // Exhaust every virtual key and modifier combination, including navigation,
    // punctuation, Caps Lock, and Ctrl/Alt/Win shortcuts.
    for (unsigned int mask = 0; mask < 32; ++mask) {
        const stroke::win::Modifiers mods{(mask & 1) != 0, (mask & 2) != 0,
            (mask & 4) != 0, (mask & 8) != 0, (mask & 16) != 0};
        for (unsigned int key = 0; key < 256; ++key) {
            const auto actual = stroke::win::english_letter(key, mods);
            const bool expected = (mask == 1 || mask == 3) && key >= 65 && key <= 90;
            if (actual.has_value() != expected ||
                (expected && *actual != L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"[(key - 65) + (mask == 3 ? 26 : 0)])) {
                std::cerr << "Incorrect English key policy: " << key << ", " << mask << '\n';
                return 1;
            }
        }
    }
    using namespace stroke::win;
    for (unsigned int mask = 0; mask < 32; ++mask) {
        const Modifiers mods{(mask & 1) != 0, (mask & 2) != 0, (mask & 4) != 0,
                             (mask & 8) != 0, (mask & 16) != 0};
        for (unsigned int key = 0; key < 256; ++key) {
            const auto actual = english_letter(key, mods, true);
            const bool expected = !mods.shortcut() && key >= 'A' && key <= 'Z';
            const bool upper = mods.caps_lock != mods.shift;
            if (actual.has_value() != expected ||
                (expected && *actual != static_cast<wchar_t>(key + (upper ? 0U : 32U)))) {
                std::cerr << "Incorrect persistent English policy\n";
                return 1;
            }
        }
    }
    auto check = [](bool value, const char* message) {
        if (!value) std::cerr << message << '\n';
        return value;
    };
    ShiftTap tap;
    for (const auto shift : {0xA0U, 0xA1U}) {
        tap.down(shift, {.shift = true});
        tap.down(shift, {.shift = true});
        if (!check(tap.up(shift) && !tap.up(shift), "Shift repeats toggle exactly once")) return 1;
        for (const auto chord : {unsigned('A'), 0x25U, 0x20U, 0x11U, 0x12U, 0x5BU}) {
            tap.down(shift, {.shift = true});
            tap.down(chord, {.shift = true});
            if (!check(!tap.up(shift), "Shift chord must not toggle")) return 1;
        }
        tap.down(shift, {.shift = true, .control = true});
        if (!check(!tap.up(shift), "Ctrl held before Shift must not toggle")) return 1;
        tap.down(shift, {.shift = true});
        tap.cancel();
        if (!check(!tap.up(shift), "Focus loss cancels Shift gesture")) return 1;
    }
    tap.down(0xA0, {.shift = true});
    tap.down(0xA1, {.shift = true});
    if (!check(!tap.up(0xA0) && !tap.up(0xA1), "Overlapping Shift keys must not toggle")) return 1;
    return 0;
}
