#pragma once

#include <cstdint>

namespace stroke::tsf {

// The adapter checks both fields before a deferred edit session can submit text.
// The context id is an adapter-issued lifetime id, not a raw COM pointer/address.
struct ContextToken {
    std::uint64_t context_id{};
    std::uint64_t revision{};
    bool operator==(const ContextToken&) const = default;
};

} // namespace stroke::tsf
