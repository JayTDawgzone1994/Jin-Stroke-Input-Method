#include "stroke/domain/config.hpp"
#include "stroke/engine/session.hpp"
#include "stroke/tsf/context_token.hpp"

#include <iostream>

namespace {
int failures = 0;
void check(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}
bool ok(const stroke::Status& status) { return std::holds_alternative<std::monostate>(status); }
} // namespace

int main() {
    using namespace stroke;
    check(is_unicode_scalar(U'\U00020BB7'), "Supplementary-plane character is one scalar");
    check(!is_unicode_scalar(static_cast<char32_t>(0xD800)), "Reject lone UTF-16 surrogate");
    check(!is_unicode_scalar(static_cast<char32_t>(0x110000)), "Reject out-of-range Unicode");

    Config config;
    config.bindings = {{'w', Stroke::horizontal}, {'u', Stroke::horizontal}};
    check(ok(validate(config)), "Two keys may map to the same stroke");
    check(map_key(config, 'w') == map_key(config, 'u'), "Aliases have equivalent meaning");
    check(!map_key(config, 'q').has_value(), "Unbound key is left to the host");
    config.bindings.push_back({'w', Stroke::dot});
    check(!ok(validate(config)), "Conflicting key assignments are rejected");
    config.bindings = {{'a', static_cast<Stroke>(0)}};
    check(!ok(validate(config)), "Invalid serialized stroke value is rejected");
    config.bindings.clear();
    config.page_size = 0;
    check(!ok(validate(config)), "Zero page size is rejected");
    config.page_size = 9;
    config.schema_version = 2;
    check(!ok(validate(config)), "Future configuration is not silently accepted");

    Session first;
    Session second;
    auto detached = first.snapshot();
    detached.strokes.push_back(Stroke::dot);
    check(first.snapshot().strokes.empty(), "View snapshots cannot mutate session");
    const tsf::ContextToken before{1, first.snapshot().revision};
    first.reset();
    const tsf::ContextToken after{1, first.snapshot().revision};
    check(before != after, "Reset invalidates old callback revision");
    check(second.snapshot().revision == 0, "Sessions do not share input state");
    check(after != tsf::ContextToken{2, after.revision}, "Context identity is part of token");

    if (failures == 0) {
        std::cout << "Framework contracts passed\n";
    }
    return failures == 0 ? 0 : 1;
}
