#pragma once
#include "stroke/domain/types.hpp"
#include <map>

namespace stroke {
using LearningCounts = std::map<char32_t, std::uint32_t>;
// Diminishing bonus: 1/5/20 uses add 5454/20000/40000; upper bound 60000.
// Use 64-bit arithmetic so a maximum dictionary score never wraps around.
[[nodiscard]] inline std::uint64_t learning_bonus(std::uint32_t count) noexcept {
    return 60000ULL * count / (static_cast<std::uint64_t>(count) + 10);
}
}
