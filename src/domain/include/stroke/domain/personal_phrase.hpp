#pragma once
#include "types.hpp"
#include <map>
#include <string>
#include <string_view>
namespace stroke {
inline constexpr std::size_t personal_phrase_limit = 20000;
inline constexpr std::uint32_t personal_phrase_threshold = 3;
struct PersonalPhrase {
    std::uint32_t count{};
    std::uint64_t last_used{};
    bool manual{};
    bool operator==(const PersonalPhrase&) const = default;
};
using PersonalPhrases = std::map<std::u32string, PersonalPhrase>;
inline bool is_personal_character(char32_t cp) noexcept {
    return (cp >= 0x3400 && cp <= 0x4DBF) || (cp >= 0x4E00 && cp <= 0x9FFF) ||
           (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0x20000 && cp <= 0x2FA1F) ||
           (cp >= 0x30000 && cp <= 0x323AF);
}
inline bool valid_personal_phrase(std::u32string_view text) noexcept {
    if (text.size() < 2 || text.size() > 8)
        return false;
    for (auto cp : text)
        if (!is_personal_character(cp))
            return false;
    return true;
}
} // namespace stroke
