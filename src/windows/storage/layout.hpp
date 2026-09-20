#pragma once
#include <stroke/domain/config.hpp>
#include <array>
#include <string>
#include <string_view>

namespace stroke::win {
// Slots are left/right keys for horizontal, vertical, left-falling, dot, turn, wildcard.
enum class LayoutMode { standard, traditional, custom };
struct Layout {
    LayoutMode mode{LayoutMode::standard};
    std::array<char, 12> custom{'a','j','s','k','d','l','q','u','w','i','e','o'};
    bool reverse_selection{};
    bool operator==(const Layout&) const = default;
};
inline std::array<char, 12> layout_keys(const Layout& layout) {
    if (layout.mode == LayoutMode::custom) return layout.custom;
    if (layout.mode == LayoutMode::traditional) return {'q','u','w','i','e','o','a','j','s','k','d','l'};
    return Layout{}.custom;
}
inline Config layout_config(const Layout& layout) {
    Config result;
    const auto keys = layout_keys(layout);
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (!keys[i]) continue;
        // Two hands may share a key only when it represents the same stroke.
        if (map_key(result, keys[i])) continue;
        result.bindings.push_back({keys[i], static_cast<Stroke>(i / 2 + 1)});
    }
    return result;
}
inline bool valid_layout(const Layout& layout) {
    if (layout.mode < LayoutMode::standard || layout.mode > LayoutMode::custom) return false;
    for (std::size_t i = 0; i < layout.custom.size(); ++i) {
        const char key = layout.custom[i];
        if (!key) continue;
        if (key < 'a' || key > 'z') return false;
        for (std::size_t j = 0; j < i; ++j)
            if (layout.custom[j] == key && i / 2 != j / 2) return false;
    }
    return true;
}
inline std::string encode_layout(const Layout& layout) {
    std::string text = "SL1";
    // Options digit: mode (0..2) plus 3 when selection labels are reversed.
    // Existing settings (0..2) retain their keys and normal selection order.
    text += static_cast<char>('0' + static_cast<int>(layout.mode) + (layout.reverse_selection ? 3 : 0));
    for (char key : layout.custom) text += key ? key : '-';
    return text;
}
inline Result<Layout> decode_layout(std::string_view text) {
    if (text.size() != 16 || text.substr(0, 3) != "SL1" || text[3] < '0' || text[3] > '5')
        return Error{ErrorCode::invalid_data, "Invalid layout format"};
    Layout result;
    result.mode = static_cast<LayoutMode>((text[3] - '0') % 3);
    result.reverse_selection = text[3] >= '3';
    for (std::size_t i = 0; i < 12; ++i) result.custom[i] = text[i + 4] == '-' ? '\0' : text[i + 4];
    if (!valid_layout(result)) return Error{ErrorCode::invalid_data, "Invalid layout keys"};
    return result;
}
}
