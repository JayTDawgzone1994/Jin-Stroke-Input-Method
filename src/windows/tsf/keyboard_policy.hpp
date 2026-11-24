#pragma once
#include <optional>

namespace stroke::win {
struct Modifiers {
    bool shift{}, caps_lock{}, control{}, alt{}, win{};
    constexpr bool shortcut() const noexcept { return control || alt || win; }
};

// Virtual-key A..Z use ASCII values on Windows. The Windows adapter translates
// punctuation separately using the host keyboard layout.
constexpr std::optional<wchar_t> english_letter(unsigned int key, Modifiers modifiers,
                                                bool english_mode = false) noexcept {
    if ((!modifiers.shift && !english_mode) || modifiers.shortcut() || key < 'A' || key > 'Z') return std::nullopt;
    // Preserve the existing Caps-Lock-only rule for temporary Shift English.
    // Persistent English behaves like the ordinary keyboard (Shift reverses case).
    const bool uppercase = modifiers.caps_lock != (english_mode && modifiers.shift);
    return static_cast<wchar_t>(key + (uppercase ? 0U : ('a' - 'A')));
}
constexpr unsigned shift_bit(unsigned int key) noexcept {
    return key == 0x10 || key == 0xA0 ? 1U : key == 0xA1 ? 2U : 0U;
}
// Only an isolated Shift press/release toggles. Repeats are idempotent, and
// overlapping left/right Shift presses or any other key cancel the gesture.
class ShiftTap {
  public:
    void down(unsigned int key, Modifiers modifiers) noexcept {
        const auto bit = shift_bit(key);
        if (!bit) { if (held_) eligible_ = false; return; }
        if (held_ & bit) return;
        if (!held_) eligible_ = !modifiers.shortcut();
        else eligible_ = false;
        held_ |= bit;
    }
    bool up(unsigned int key) noexcept {
        const auto bit = shift_bit(key);
        if (!bit || !(held_ & bit)) return false;
        held_ &= ~bit;
        const bool toggle = !held_ && eligible_;
        if (!held_) eligible_ = false;
        return toggle;
    }
    bool held() const noexcept { return held_ != 0; }
    void cancel() noexcept { held_ = 0; eligible_ = false; }
  private:
    unsigned held_{};
    bool eligible_{};
};
}
