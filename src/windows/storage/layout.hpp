#pragma once
#include <array>
#include <string>
#include <string_view>
#include <stroke/domain/config.hpp>
namespace stroke::win {
enum class LayoutMode { standard, traditional, custom };
// One stroke per letter; zero is unbound. Multiple letters may share a stroke.
using KeyBindings = std::array<unsigned char, 26>;
inline KeyBindings preset_bindings(LayoutMode mode) {
    KeyBindings result{};
    const std::string_view keys = mode == LayoutMode::traditional ? "quwieoajskdl" : "ajskdlquwieo";
    for (std::size_t i = 0; i < keys.size(); ++i)
        result[keys[i] - 'a'] = static_cast<unsigned char>(i / 2 + 1);
    return result;
}
struct Layout {
    LayoutMode mode{LayoutMode::standard};
    KeyBindings custom{preset_bindings(LayoutMode::standard)};
    bool reverse_selection{};
    bool operator==(const Layout&) const = default;
};
inline KeyBindings layout_keys(const Layout& layout) {
    return layout.mode == LayoutMode::custom ? layout.custom : preset_bindings(layout.mode);
}
inline Config layout_config(const Layout& layout) {
    Config result;
    const auto keys = layout_keys(layout);
    for (std::size_t i = 0; i < keys.size(); ++i)
        if (keys[i])
            result.bindings.push_back({static_cast<char>('a' + i), static_cast<Stroke>(keys[i])});
    return result;
}
inline bool valid_layout(const Layout& layout) {
    if (layout.mode < LayoutMode::standard || layout.mode > LayoutMode::custom)
        return false;
    for (auto value : layout.custom)
        if (value > 6)
            return false;
    return true;
}
inline bool bind_key(Layout& layout, char key, unsigned char stroke) {
    if (key < 'a' || key > 'z' || stroke > 6)
        return false;
    auto keys = layout_keys(layout);
    if (keys[key - 'a'] == stroke)
        return false;
    keys[key - 'a'] = stroke;
    layout.custom = keys;
    layout.mode = LayoutMode::custom;
    return true;
}
inline std::string encode_layout(const Layout& layout) {
    std::string text = "SL2";
    text +=
        static_cast<char>('0' + static_cast<int>(layout.mode) + (layout.reverse_selection ? 3 : 0));
    for (auto stroke : layout.custom)
        text += static_cast<char>('0' + stroke);
    return text;
}
inline Result<Layout> decode_layout(std::string_view text) {
    const bool legacy = text.size() == 16 && text.substr(0, 3) == "SL1";
    if ((!legacy && (text.size() != 30 || text.substr(0, 3) != "SL2")) || text[3] < '0' ||
        text[3] > '5')
        return Error{ErrorCode::invalid_data, "Invalid layout format"};
    Layout result;
    result.mode = static_cast<LayoutMode>((text[3] - '0') % 3);
    result.reverse_selection = text[3] >= '3';
    result.custom.fill(0);
    if (legacy) {
        // Preserve installed settings; subsequent saves use SL2.
        for (std::size_t i = 0; i < 12; ++i) {
            const auto key = text[i + 4];
            if (key == '-')
                continue;
            if (key < 'a' || key > 'z')
                return Error{ErrorCode::invalid_data, "Invalid layout key"};
            auto& slot = result.custom[key - 'a'];
            const auto stroke = static_cast<unsigned char>(i / 2 + 1);
            if (slot && slot != stroke)
                return Error{ErrorCode::invalid_data, "Conflicting layout key"};
            slot = stroke;
        }
    } else {
        for (std::size_t i = 0; i < 26; ++i) {
            if (text[i + 4] < '0' || text[i + 4] > '6')
                return Error{ErrorCode::invalid_data, "Invalid layout stroke"};
            result.custom[i] = static_cast<unsigned char>(text[i + 4] - '0');
        }
    }
    return result;
}
} // namespace stroke::win
