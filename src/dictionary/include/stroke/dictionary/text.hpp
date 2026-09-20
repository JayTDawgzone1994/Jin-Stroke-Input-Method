#pragma once

#include "stroke/domain/types.hpp"

#include <filesystem>
#include <string_view>

namespace stroke {

[[nodiscard]] Result<std::u32string> decode_utf8(std::string_view text);
[[nodiscard]] std::string encode_utf8(char32_t scalar);
[[nodiscard]] Result<std::string> read_bytes(const std::filesystem::path& path,
                                            std::size_t limit = 64 * 1024 * 1024);

} // namespace stroke
