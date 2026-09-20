#pragma once
#include <cstddef>
#include <optional>

namespace stroke::win {
// Page labels and keyboard selection share one mapping, including partial pages.
inline char candidate_digit(std::size_t index, bool reversed) noexcept {
    if (index >= 9) return '\0';
    return static_cast<char>(reversed ? '9' - index : '1' + index);
}
inline std::optional<std::size_t> candidate_index(char digit, bool reversed, std::size_t visible) noexcept {
    if (digit < '1' || digit > '9') return std::nullopt;
    const auto index = static_cast<std::size_t>(reversed ? '9' - digit : digit - '1');
    return index < visible ? std::optional<std::size_t>{index} : std::nullopt;
}
}
