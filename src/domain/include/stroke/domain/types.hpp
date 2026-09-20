#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <variant>
#include <vector>

namespace stroke {

// wildcard is a query token only; stored dictionary strokes remain strictly 1..5.
enum class Stroke : std::uint8_t { horizontal = 1, vertical, left_falling, dot, turn, wildcard };
using StrokeSequence = std::vector<Stroke>;
inline constexpr std::size_t max_stroke_sequence_length = 128;

[[nodiscard]] constexpr bool is_valid(Stroke value) noexcept {
    const auto number = static_cast<std::uint8_t>(value);
    return number >= 1 && number <= 5;
}

[[nodiscard]] constexpr bool is_query_stroke(Stroke value) noexcept {
    return is_valid(value) || value == Stroke::wildcard;
}

[[nodiscard]] constexpr bool is_unicode_scalar(char32_t value) noexcept {
    return value <= 0x10FFFF && !(value >= 0xD800 && value <= 0xDFFF);
}

struct Candidate {
    char32_t character{};
    std::uint32_t frequency_score{}; // Higher is preferred; zero means unscored.
    bool exact_match{};
    bool operator==(const Candidate&) const = default;
};

enum class ErrorCode { invalid_argument, unsupported_version, invalid_data, unavailable, io_error };
struct Error {
    ErrorCode code;
    std::string message; // Developer diagnostic, not localized UI text.
};

template <typename T> using Result = std::variant<T, Error>;
using Status = Result<std::monostate>;

} // namespace stroke
