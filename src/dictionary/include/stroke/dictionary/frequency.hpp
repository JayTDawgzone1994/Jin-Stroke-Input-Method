#pragma once
#include "stroke/domain/types.hpp"
#include <map>
#include <string_view>

namespace stroke {
using FrequencyScores = std::map<char32_t, std::uint32_t>;
// UTF-8: STROKE-FREQUENCY-1 followed by character<TAB>unsigned-score lines.
// Strict, unique single Unicode scalars; zero means unscored.
[[nodiscard]] Result<FrequencyScores> parse_frequency(std::string_view bytes);
}
