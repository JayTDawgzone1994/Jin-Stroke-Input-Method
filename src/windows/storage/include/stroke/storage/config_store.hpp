#pragma once

#include "stroke/domain/config.hpp"

namespace stroke {

// Call outside keyboard callbacks. Implementations must validate and atomically publish.
class IConfigStore {
public:
    virtual ~IConfigStore() = default;
    [[nodiscard]] virtual Result<Config> load() const = 0;
    [[nodiscard]] virtual Status save(const Config& config) = 0;
};

} // namespace stroke
