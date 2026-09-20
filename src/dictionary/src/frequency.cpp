#include "stroke/dictionary/frequency.hpp"
#include "stroke/dictionary/text.hpp"
#include <charconv>

namespace stroke {
Result<FrequencyScores> parse_frequency(std::string_view bytes) {
    constexpr std::string_view header = "STROKE-FREQUENCY-1\n";
    if (bytes.size() > 4 * 1024 * 1024 || !bytes.starts_with(header)) {
        return Error{ErrorCode::invalid_data, "Invalid frequency table header or size"};
    }
    bytes.remove_prefix(header.size());
    FrequencyScores scores;
    while (!bytes.empty()) {
        const auto end = bytes.find('\n');
        const auto line = bytes.substr(0, end);
        bytes.remove_prefix(end == std::string_view::npos ? bytes.size() : end + 1);
        const auto tab = line.find('\t');
        if (tab == std::string_view::npos) return Error{ErrorCode::invalid_data, "Invalid frequency row"};
        const auto decoded = decode_utf8(line.substr(0, tab));
        const auto* text = std::get_if<std::u32string>(&decoded);
        std::uint32_t score{};
        const auto number = line.substr(tab + 1);
        const auto [last, error] = std::from_chars(number.data(), number.data() + number.size(), score);
        if (!text || text->size() != 1 || text->front() == 0 || error != std::errc{} ||
            last != number.data() + number.size() || !scores.emplace(text->front(), score).second ||
            scores.size() > 100000) {
            return Error{ErrorCode::invalid_data, "Invalid or duplicate frequency entry"};
        }
    }
    if (scores.empty()) return Error{ErrorCode::invalid_data, "Empty frequency table"};
    return scores;
}
}
