#include "stroke/domain/config.hpp"

#include <array>

namespace stroke {

Status validate(const Config& config) {
    if (config.schema_version != config_schema_version) {
        return Error{ErrorCode::unsupported_version, "Unsupported configuration schema"};
    }
    if (config.page_size == 0 || config.page_size > 20) {
        return Error{ErrorCode::invalid_argument, "Page size must be between 1 and 20"};
    }
    std::array<bool, 26> seen{};
    for (const auto& binding : config.bindings) {
        if (binding.key < 'a' || binding.key > 'z' || !is_query_stroke(binding.stroke)) {
            return Error{ErrorCode::invalid_argument, "Invalid key or stroke"};
        }
        const auto index = static_cast<std::size_t>(binding.key - 'a');
        if (seen[index]) {
            return Error{ErrorCode::invalid_argument, "Duplicate key binding"};
        }
        seen[index] = true;
    }
    return std::monostate{};
}

std::optional<Stroke> map_key(const Config& config, char key) noexcept {
    for (const auto& binding : config.bindings) {
        if (binding.key == key) {
            return binding.stroke;
        }
    }
    return std::nullopt;
}

Config default_keyboard_config() {
    Config config;
    constexpr char left[] = "asdqwe", right[] = "jkluio";
    for (unsigned i = 0; i < 6; ++i) {
        config.bindings.push_back({left[i], static_cast<Stroke>(i + 1)});
        config.bindings.push_back({right[i], static_cast<Stroke>(i + 1)});
    }
    return config;
}

} // namespace stroke
