#pragma once

#include "stroke/domain/types.hpp"

#include <span>
#include <string>
#include <vector>

namespace stroke {

struct DictionaryInfo {
    std::uint32_t format_version{};
    std::string data_version;
    std::string source_revision;
};

// Immutable after construction. Results own their data; no dangling mapped-file views.
// Returns all prefix matches, one per character, with exact_match aggregated across codes.
// Final ranking and pagination belong to engine. Empty prefix returns no candidates.
// Query-only Stroke::wildcard matches exactly one stored stroke. exact_match means
// a matching code has the same length as the query, even if the query contains wildcards.
class IDictionary {
public:
    virtual ~IDictionary() = default;
    [[nodiscard]] virtual DictionaryInfo info() const = 0;
    [[nodiscard]] virtual Result<std::vector<Candidate>>
    lookup(std::span<const Stroke> prefix) const = 0;
};

} // namespace stroke
