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
    return 0;
}
