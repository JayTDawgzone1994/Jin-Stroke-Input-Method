#include "../../src/windows/storage/layout.hpp"
#include "../../src/windows/storage/selection.hpp"
#include <iostream>
#include <stroke/dictionary/index.hpp>
#include <stroke/engine/session.hpp>
#ifdef STROKE_HAS_LAYOUT_STORE
#include "../../src/windows/storage/layout_store.hpp"
#include <fstream>
#endif
namespace {
int failures{};
void check(bool value, const char* label) {
    if (!value) {
        ++failures;
        std::cerr << label << '\n';
    }
}
template <class T> bool ok(const stroke::Result<T>& result) {
    return !std::holds_alternative<stroke::Error>(result);
}
} // namespace
int main() {
    using namespace stroke;
    using namespace stroke::win;
    Layout standard, traditional;
    traditional.mode = LayoutMode::traditional;
    for (const auto mode : {LayoutMode::standard, LayoutMode::traditional}) {
        Layout layout;
        layout.mode = mode;
        const auto config = layout_config(layout);
        check(ok(validate(config)), "Preset must validate");
        constexpr char keys[] = "qweasduiojkl";
        constexpr unsigned expected[] = {4, 5, 6, 1, 2, 3, 4, 5, 6, 1, 2, 3};
        for (unsigned i = 0; i < 12; ++i) {
            const auto value =
                mode == LayoutMode::standard ? expected[i] : (expected[i] + 2) % 6 + 1;
            check(map_key(config, keys[i]) == static_cast<Stroke>(value),
                  "Preset relative position incorrect");
        }
    }
    Layout custom;
    custom.mode = LayoutMode::custom;
    bind_key(custom, 'z', 1);
    bind_key(custom, 'a', 0);
    check(valid_layout(custom) && !map_key(layout_config(custom), 'a') &&
              map_key(layout_config(custom), 'z') == Stroke::horizontal,
          "Custom rebind and disabled key");
    for (const auto mode : {LayoutMode::standard, LayoutMode::traditional, LayoutMode::custom}) {
        custom.mode = mode;
        const auto decoded = decode_layout(encode_layout(custom));
        check(ok(decoded) && std::get<Layout>(decoded) == custom,
              "Custom retained across preset serialization");
    }
    bind_key(custom, 'x', 1);
    bind_key(custom, 'c', 1);
    for (const auto mode : {LayoutMode::standard, LayoutMode::traditional, LayoutMode::custom}) {
        auto reversed = custom;
        reversed.mode = mode;
        reversed.reverse_selection = true;
        const auto decoded = decode_layout(encode_layout(reversed));
        check(ok(decoded) && std::get<Layout>(decoded) == reversed,
              "Reverse selection and custom keys roundtrip");
    }
    check(!std::get<Layout>(decode_layout("SL10ajskdlquwieo")).reverse_selection,
          "Existing standard settings keep normal order");
    for (const bool reversed : {false, true}) {
        for (std::size_t count = 0; count <= 9; ++count) {
            for (char digit = '1'; digit <= '9'; ++digit) {
                const auto index = candidate_index(digit, reversed, count);
                const auto expected_index =
                    static_cast<std::size_t>(reversed ? '9' - digit : digit - '1');
                check(index.has_value() == (expected_index < count),
                      "Unavailable page slots cannot select");
                if (index)
                    check(*index == expected_index && candidate_digit(*index, reversed) == digit,
                          "Displayed label and selection key agree on every page size");
            }
        }
        check(!candidate_index('0', reversed, 9) && !candidate_index('a', reversed, 9),
              "Non-selection keys rejected");
    }
    check(candidate_digit(0, true) == '9' && candidate_digit(8, true) == '1', "Reverse endpoints");
    check(candidate_index('9', true, 3) == std::size_t{0} &&
              candidate_index('7', true, 3) == std::size_t{2} && !candidate_index('1', true, 3),
          "Short page uses 9,8,7 rather than shrinking the key range");
    check(valid_layout(custom) && ok(validate(layout_config(custom))),
          "Same-stroke aliases deduplicate");
    check(map_key(layout_config(custom), 'z') == Stroke::horizontal &&
              map_key(layout_config(custom), 'x') == Stroke::horizontal &&
              map_key(layout_config(custom), 'c') == Stroke::horizontal &&
              map_key(layout_config(custom), 'j') == Stroke::horizontal,
          "More than two aliases supported");
    bind_key(custom, 'z', 5);
    check(map_key(layout_config(custom), 'z') == static_cast<Stroke>(5),
          "Reassign replaces previous stroke");
    const auto before = custom;
    check(!bind_key(custom, '1', 1) && !bind_key(custom, ';', 1) && !bind_key(custom, 'a', 7) &&
              custom == before,
          "Reserved keys and invalid strokes leave draft unchanged");
    Layout preset;
    check(!bind_key(preset, 'a', 1) && preset.mode == LayoutMode::standard, "No-op retains preset");
    bind_key(preset, 'z', 1);
    check(preset.mode == LayoutMode::custom && map_key(layout_config(preset), 'q') == Stroke::dot,
          "Editing preset clones all existing bindings");
    auto migrated = decode_layout("SL15z-skdlquwieo");
    check(ok(migrated) && std::get<Layout>(migrated).reverse_selection &&
              map_key(layout_config(std::get<Layout>(migrated)), 'z') == Stroke::horizontal &&
              encode_layout(std::get<Layout>(migrated)).starts_with("SL2"),
          "Installed custom settings migrate without loss");
    check(!ok(decode_layout("SL12aaskdlaawieo")), "Conflicting legacy keys rejected");
    auto invalid = custom;
    invalid.custom[2] = 7;
    check(!valid_layout(invalid) && !ok(decode_layout(encode_layout(invalid))),
          "Invalid strokes rejected");
    for (const auto* text :
         {"", "SL20000000000000", "SL19ajskdlquwieo", "SL10ajskdlquwieoextra", "SL10ajskdlquwie"})
        check(!ok(decode_layout(text)), "Malformed settings rejected");
    custom.custom.fill(0);
    check(valid_layout(custom) && layout_config(custom).bindings.empty(),
          "All slots can be disabled");
    const auto encoded = encode_index({{"1", U'一', 0}, {"4", U'丶', 0}},
                                      {index_format_version, "test", "fixture"}, "Layout fixture");
    const auto dictionary = IndexedDictionary::decode(std::get<std::string>(encoded));
    Session session;
    check(ok(session.configure(std::get<std::shared_ptr<const IndexedDictionary>>(dictionary),
                               layout_config(standard))),
          "Configure standard");
    check(ok(session.process_key('a')), "Start standard query");
    check(!ok(session.configure(std::get<std::shared_ptr<const IndexedDictionary>>(dictionary),
                                layout_config(traditional))),
          "No layout change while composing");
    check(session.snapshot().strokes == StrokeSequence{Stroke::horizontal},
          "Pending query retains meaning");
    session.reset();
    check(ok(session.configure(std::get<std::shared_ptr<const IndexedDictionary>>(dictionary),
                               layout_config(traditional))),
          "Change at idle");
    check(ok(session.process_key('a')) && session.snapshot().strokes == StrokeSequence{Stroke::dot},
          "Next query uses traditional");
#ifdef STROKE_HAS_LAYOUT_STORE
    const auto directory = std::filesystem::current_path() / "layout-store-test";
    const auto path = directory / "layout.dat";
    check(ok(save_layout(path, standard)), "Atomic save");
    check(ok(save_layout(path, traditional)), "Atomic replace");
    auto loaded = load_layout(path);
    check(ok(loaded) && std::get<Layout>(loaded) == traditional, "Reload saved layout");
    auto reversed_saved = traditional;
    reversed_saved.reverse_selection = true;
    check(ok(save_layout(path, reversed_saved)), "Save reversed selection");
    loaded = load_layout(path);
    check(ok(loaded) && std::get<Layout>(loaded) == reversed_saved,
          "Reverse selection persists across reload");
    check(ok(save_layout(path, traditional)), "Restore normal selection");
    check(!ok(save_layout(path, invalid)), "Invalid save rejected");
    loaded = load_layout(path);
    check(ok(loaded) && std::get<Layout>(loaded) == traditional,
          "Failed save preserves existing file");
    {
        std::ofstream corrupt(path);
        corrupt << "bad";
    }
    check(!ok(load_layout(path)), "Corrupt file not silently accepted");
    std::filesystem::remove(path);
    loaded = load_layout(path);
    check(ok(loaded) && std::get<Layout>(loaded) == standard, "Missing file defaults to standard");
    std::filesystem::remove(directory);
#endif
    return failures ? 1 : 0;
}
