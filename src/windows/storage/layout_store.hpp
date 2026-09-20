#pragma once
#include "layout.hpp"
#include <filesystem>

namespace stroke::win {
std::filesystem::path layout_path();
Result<Layout> load_layout(const std::filesystem::path& path);
Status save_layout(const std::filesystem::path& path, const Layout& layout);
}
