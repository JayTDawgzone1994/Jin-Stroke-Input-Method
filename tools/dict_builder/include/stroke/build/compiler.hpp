#pragma once

#include "stroke/dictionary/index.hpp"

#include <map>
#include <set>

namespace stroke::build {

enum class CharacterClass { shared, traditional, simplified };
enum class Scope { traditional, all };
struct CharacterEntry {
    CharacterClass classification{CharacterClass::shared};
    std::set<std::string> sequences;
};
using Dataset = std::map<char32_t, CharacterEntry>;

[[nodiscard]] Result<std::set<std::string>> expand(std::string_view expression);
[[nodiscard]] Result<Dataset> parse_conway(std::string_view text);
// Exact comparison against upstream's expanded table, BEFORE filtering or overrides.
[[nodiscard]] Status verify_reference(const Dataset& data, std::string_view text);
// Atomic: on any bad row/conflict, no changes escape. Each layer may touch a character once.
[[nodiscard]] Result<Dataset> apply_overrides(Dataset data, std::string_view text);
[[nodiscard]] std::vector<IndexRecord> make_records(const Dataset& data, Scope scope);

} // namespace stroke::build
