#pragma once
#include <optional>

namespace stroke::win {
struct Modifiers {
    bool shift{}, caps_lock{}, control{}, alt{}, win{};
    constexpr bool shortcut() const noexcept { return control || alt || win; }
};

// Virtual-key A..Z use ASCII values on Windows. Shift selects English;
// only Caps Lock determines its case. Nonletters remain host-owned.
constexpr std::optional<wchar_t> english_letter(unsigned int key, Modifiers modifiers) noexcept {
    if (!modifiers.shift || modifiers.shortcut() || key < 'A' || key > 'Z') return std::nullopt;
    return static_cast<wchar_t>(key + (modifiers.caps_lock ? 0U : ('a' - 'A')));
}
}
