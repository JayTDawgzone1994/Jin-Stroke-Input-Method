#pragma once

#include "stroke/domain/types.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace stroke {

inline constexpr std::uint32_t config_schema_version = 1;
struct KeyBinding {
    char key; // Lowercase ASCII logical key. OS key/modifier decoding belongs to adapter.
    Stroke stroke;
};
struct Config {
    std::uint32_t schema_version{config_schema_version};
    std::size_t page_size{9};
    bool learning_enabled{false};
    std::vector<KeyBinding> bindings; // Deliberately empty until layout is confirmed.
};

[[nodiscard]] Status validate(const Config& config);
// QWE/ASD and UIO/JKL: top row dot, turn, wildcard; bottom horizontal, vertical, left-falling.
[[nodiscard]] Config default_keyboard_config();
[[nodiscard]] std::optional<Stroke> map_key(const Config& config, char key) noexcept;

} // namespace stroke
