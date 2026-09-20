#include "stroke/build/compiler.hpp"
#include "stroke/dictionary/text.hpp"

#include <algorithm>
#include <charconv>
#include <functional>
#include <stdexcept>
#include <tuple>

namespace stroke::build {
namespace {
class ParseError : public std::runtime_error { using std::runtime_error::runtime_error; };
void require(bool condition, const std::string& message) {
    if (!condition) { throw ParseError(message); }
}
template <typename T> T take(Result<T> result) {
    if (const auto* error = std::get_if<Error>(&result)) { throw ParseError(error->message); }
    return std::get<T>(std::move(result));
}
std::vector<std::string_view> split(std::string_view text, char delimiter) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    for (;;) {
        const auto end = text.find(delimiter, start);
        if (end == std::string_view::npos) { out.push_back(text.substr(start)); break; }
        out.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return out;
}
bool digits(std::string_view value) {
    return std::all_of(value.begin(), value.end(), [](char c) { return c >= '1' && c <= '5'; });
}
char32_t codepoint(std::string_view text) {
    require(text.starts_with("U+") && text.size() >= 6 && text.size() <= 8, "Expected U+ hexadecimal code point");
    std::uint32_t value{};
    const auto result = std::from_chars(text.data() + 2, text.data() + text.size(), value, 16);
    require(result.ec == std::errc{} && result.ptr == text.data() + text.size() &&
            value != 0 && is_unicode_scalar(static_cast<char32_t>(value)), "Invalid Unicode code point");
    return static_cast<char32_t>(value);
}
template <typename F> void lines(std::string_view text, F consume) {
    require(text.size() <= max_index_bytes, "Source text exceeds size limit");
    take(decode_utf8(text));
    if (text.starts_with("\xEF\xBB\xBF")) { text.remove_prefix(3); }
    std::size_t number = 0;
    for (auto line : split(text, '\n')) {
        ++number;
        if (line.ends_with('\r')) { line.remove_suffix(1); }
        try { consume(line); }
        catch (const ParseError& error) { throw ParseError("Line " + std::to_string(number) + ": " + error.what()); }
    }
}
std::set<std::pair<std::string, char32_t>> pairs(const Dataset& data) {
    std::set<std::pair<std::string, char32_t>> out;
    for (const auto& [cp, entry] : data) {
        for (const auto& sequence : entry.sequences) { out.emplace(sequence, cp); }
    }
    return out;
}
} // namespace

Result<std::set<std::string>> expand(std::string_view expression) {
    try {
        require(!expression.empty() && expression.size() <= 2048, "Invalid expression length");
        struct Token { std::string literal; int group{-1}; };
        std::vector<Token> tokens;
        std::vector<std::vector<std::string>> groups;
        for (std::size_t i = 0; i < expression.size();) {
            const char c = expression[i++];
            if (c >= '1' && c <= '5') { tokens.push_back({std::string(1, c), -1}); }
            else if (c == '(') {
                const auto close = expression.find(')', i);
                require(close != std::string_view::npos && groups.size() < 9, "Unclosed or excessive capture groups");
                std::set<std::string> choices;
                for (const auto choice : split(expression.substr(i, close - i), '|')) {
                    require(digits(choice), "Only digits and alternatives are allowed inside groups");
                    choices.emplace(choice);
                }
                tokens.push_back({{}, static_cast<int>(groups.size())});
                groups.emplace_back(choices.begin(), choices.end());
                i = close + 1;
            } else if (c == '\\') {
                require(i < expression.size() && expression[i] >= '1' && expression[i] <= '9', "Invalid backreference");
                const auto group = static_cast<std::size_t>(expression[i++] - '1');
                require(group < groups.size(), "Backreference must name an earlier group");
                tokens.push_back({{}, static_cast<int>(group)});
            } else { throw ParseError("Invalid character outside capture group"); }
        }
        std::size_t combinations = 1;
        for (const auto& group : groups) {
            require(group.size() <= 4096 / combinations, "Expression expansion exceeds 4096 combinations");
            combinations *= group.size();
        }
        std::set<std::string> result;
        std::vector<std::string> selected(groups.size());
        std::function<void(std::size_t)> visit = [&](std::size_t group) {
            if (group != groups.size()) {
                for (const auto& choice : groups[group]) { selected[group] = choice; visit(group + 1); }
                return;
            }
            std::string sequence;
            for (const auto& token : tokens) {
                sequence += token.group < 0 ? token.literal : selected[static_cast<std::size_t>(token.group)];
                require(sequence.size() <= max_sequence_length, "Expanded sequence is too long");
            }
            require(!sequence.empty(), "Expanded sequence cannot be empty");
            result.insert(std::move(sequence));
        };
        visit(0);
        return result;
    } catch (const ParseError& error) { return Error{ErrorCode::invalid_data, error.what()}; }
}

Result<Dataset> parse_conway(std::string_view text) {
    try {
        Dataset result;
        std::size_t pair_count = 0;
        lines(text, [&](std::string_view line) {
            if (!line.starts_with("U+")) { return; }
            const auto fields = split(line, '\t');
            require(fields.size() == 3, "Expected three tab-separated Conway fields");
            auto cp_field = fields[0];
            if (cp_field.ends_with('!')) { cp_field.remove_suffix(1); }
            const auto cp = codepoint(cp_field);
            auto character = fields[1];
            CharacterClass classification = CharacterClass::shared;
            if (character.ends_with('^')) { classification = CharacterClass::traditional; character.remove_suffix(1); }
            else if (character.ends_with('*')) { classification = CharacterClass::simplified; character.remove_suffix(1); }
            const auto decoded = take(decode_utf8(character));
            require(decoded.size() == 1 && decoded[0] == cp, "Code point and character disagree");
            require(!result.contains(cp), "Duplicate source character");
            auto expanded = take(expand(fields[2]));
            pair_count += expanded.size();
            require(pair_count <= 1000000, "Too many expanded source records");
            result.emplace(cp, CharacterEntry{classification, std::move(expanded)});
            require(result.size() <= 100000, "Too many source characters");
        });
        require(!result.empty(), "Source has no character records");
        return result;
    } catch (const ParseError& error) { return Error{ErrorCode::invalid_data, error.what()}; }
}

Status verify_reference(const Dataset& data, std::string_view text) {
    try {
        std::set<std::pair<std::string, char32_t>> reference;
        lines(text, [&](std::string_view line) {
            if (line.empty() || line.starts_with('#')) { return; }
            const auto fields = split(line, '\t');
            require(fields.size() == 2 && !fields[0].empty() && fields[0].size() <= max_sequence_length &&
                    digits(fields[0]), "Invalid reference sequence");
            const auto chars = take(decode_utf8(fields[1]));
            require(!chars.empty(), "Empty reference candidates");
            for (const auto cp : chars) {
                require(reference.emplace(fields[0], cp).second, "Duplicate reference pair");
                require(reference.size() <= 1000000, "Reference exceeds record limit");
            }
        });
        require(reference == pairs(data), "Expanded source differs from upstream reference table");
        return std::monostate{};
    } catch (const ParseError& error) { return Error{ErrorCode::invalid_data, error.what()}; }
}

Result<Dataset> apply_overrides(Dataset data, std::string_view text) {
    try {
        std::set<char32_t> touched;
        std::size_t pair_count = 0;
        for (const auto& [cp, entry] : data) { static_cast<void>(cp); pair_count += entry.sequences.size(); }
        lines(text, [&](std::string_view line) {
            if (line.empty() || line.starts_with('#')) { return; }
            const auto fields = split(line, '\t');
            require(fields.size() == 5, "Override requires action, codepoint, expression, class, reason");
            const auto cp = codepoint(fields[1]);
            require(touched.insert(cp).second, "One override layer cannot modify a character twice");
            require(!fields[4].empty() && fields[4].find_first_not_of(' ') != std::string_view::npos,
                    "Override must document a reason/source");
            if (fields[0] == "remove") {
                require(fields[2] == "-" && fields[3] == "-", "Remove must use '-' expression and class");
                require(data.contains(cp), "Cannot remove a missing character");
                pair_count -= data.at(cp).sequences.size();
                data.erase(cp);
                return;
            }
            require(fields[0] == "add" || fields[0] == "replace", "Unknown override action");
            require(fields[0] == "add" ? !data.contains(cp) : data.contains(cp), "Add/replace conflicts with current dataset");
            CharacterClass classification{};
            if (fields[3] == "shared") { classification = CharacterClass::shared; }
            else if (fields[3] == "traditional") { classification = CharacterClass::traditional; }
            else if (fields[3] == "simplified") { classification = CharacterClass::simplified; }
            else { throw ParseError("Unknown character class"); }
            auto sequences = take(expand(fields[2]));
            if (data.contains(cp)) { pair_count -= data.at(cp).sequences.size(); }
            pair_count += sequences.size();
            require(pair_count <= 1000000 && (data.contains(cp) || data.size() < 100000), "Override exceeds dataset limits");
            data[cp] = {classification, std::move(sequences)};
        });
        return data;
    } catch (const ParseError& error) { return Error{ErrorCode::invalid_data, error.what()}; }
}

std::vector<IndexRecord> make_records(const Dataset& data, Scope scope) {
    std::vector<IndexRecord> result;
    for (const auto& [cp, entry] : data) {
        if (scope == Scope::traditional && entry.classification == CharacterClass::simplified) { continue; }
        for (const auto& sequence : entry.sequences) { result.push_back({sequence, cp, 0U}); }
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return std::tie(a.sequence, a.character) < std::tie(b.sequence, b.character);
    });
    return result;
}

} // namespace stroke::build
