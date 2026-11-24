#pragma once
#include "layout.hpp"
#include <filesystem>

namespace stroke::win {
std::filesystem::path layout_path();
// Per-user, shared by unpackaged and MSIX desktop hosts. No package-local fallback.
std::filesystem::path user_storage_directory();
Status migrate_user_storage();
// Explicit roots also allow migration tests without touching user data.
Status migrate_user_storage(const std::filesystem::path& legacy,
                            const std::filesystem::path& shared);
Result<Layout> load_layout(const std::filesystem::path& path);
Status save_layout(const std::filesystem::path& path, const Layout& layout);
}
