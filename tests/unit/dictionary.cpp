#include "stroke/build/compiler.hpp"
#include "stroke/dictionary/text.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
int failures = 0;
void check(bool condition, const char* name) {
    if (!condition) { std::cerr << "FAIL: " << name << '\n'; ++failures; }
}
template <typename T> bool failed(const stroke::Result<T>& value) { return std::holds_alternative<stroke::Error>(value); }
template <typename T> T take(stroke::Result<T> value) {
    if (const auto* error = std::get_if<stroke::Error>(&value)) { throw std::runtime_error(error->message); }
    return std::get<T>(std::move(value));
}
void put(std::string& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) { bytes[offset + i] = static_cast<char>((value >> (8 * i)) & 0xFF); }
}
void recalculate(std::string& bytes) { put(bytes, 20, stroke::checksum(std::string_view(bytes).substr(24))); }
std::uint32_t get(const std::string& bytes, std::size_t offset) {
    std::uint32_t value{};
    for (unsigned i = 0; i < 4; ++i) { value |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + i])) << (8 * i); }
    return value;
}
stroke::StrokeSequence prefix(std::string_view text) {
    stroke::StrokeSequence result;
    for (const char c : text) { result.push_back(static_cast<stroke::Stroke>(c - '0')); }
    return result;
}
void unit_tests() {
    using namespace stroke;
    using namespace stroke::build;
    check(take(expand("(1|2)3\\11")) == std::set<std::string>{"1311", "2321"}, "Single-digit reference before a digit");
    check(take(expand("1(|2)(3|3)")) == std::set<std::string>{"13", "123"}, "Empty choice and duplicate alternatives");
    check(take(expand("(1|2)(3|4)\\2\\1")) == std::set<std::string>{"1331", "1441", "2332", "2442"}, "Correlated references");
    for (const auto* invalid : {"", "()", "(1|(2))", "\\1", "1|2", "(1", "16", "(1)\\2", "1\\0"}) {
        check(failed(expand(invalid)), "Reject malformed expression");
    }
    check(failed(expand(std::string(129, '1'))), "Reject overlong sequence");
    check(failed(expand("(1|2|3|4|5)(1|2|3|4|5)(1|2|3|4|5)(1|2|3|4|5)(1|2|3|4|5)(1|2|3|4|5)")), "Bound expansion");
    const std::string fixture = "# fixture\nU+4E00\t一\t1\nU+4E01\t丁\t12\nU+56FD\t国*\t25112141\nU+570B\t國^\t(25125115341|25125115431)\n";
    const auto data = take(parse_conway(fixture));
    check(data.size() == 4, "Parse shared/traditional/simplified");
    check(make_records(data, Scope::all).size() == 5, "All scope keeps all variants");
    check(make_records(data, Scope::traditional).size() == 4, "Traditional scope retains shared characters");
    check(failed(parse_conway("U+4E00\t丁\t1\n")), "Reject mismatching codepoint");
    check(failed(parse_conway(fixture + "U+4E00\t一\t1\n")), "Reject duplicate source entry");
    check(failed(parse_conway("U+4E00 bad\n")), "Do not silently skip malformed records");
    check(!failed(parse_conway("\xEF\xBB\xBF" + fixture)), "UTF-8 BOM accepted");
    check(!failed(verify_reference(data, "1\t一\n12\t丁\n25112141\t国\n25125115341\t國\n25125115431\t國\n")), "Reference equivalence");
    check(failed(verify_reference(data, "1\t一\n")), "Reference omissions are errors");

    const auto updated = take(apply_overrides(data,
        "add\tU+20BB7\t121251\tshared\tTest fixture, not a linguistic assertion\n"
        "replace\tU+4E01\t(12|21)\ttraditional\tSynthetic test replacement\n"
        "remove\tU+56FD\t-\t-\tTest removal\n"));
    check(updated.contains(U'𠮷') && !updated.contains(U'国'), "Supplementary character add and removal");
    check(updated.at(U'丁').sequences.size() == 2, "Replacement replaces complete sequence set");
    check(!data.contains(U'𠮷') && data.contains(U'国'), "Original dataset is untouched");
    check(failed(apply_overrides(data, "add\tU+4E00\t1\tshared\tconflict\n")), "Add conflict rejected");
    check(failed(apply_overrides(data, "replace\tU+20BB7\t1\tshared\tmissing\n")), "Replace missing rejected");
    check(failed(apply_overrides(data, "remove\tU+20BB7\t-\t-\tmissing\n")), "Remove missing rejected");
    check(failed(apply_overrides(data, "remove\tU+4E00\t-\t-\t\n")), "Reason required");
    check(failed(apply_overrides(data, "remove\tU+4E00\t-\t-\tx\nadd\tU+4E00\t1\tshared\tx\n")), "Duplicate layer edits rejected");
    const auto next_layer = take(apply_overrides(updated, "replace\tU+4E01\t12\tshared\tLater layer\n"));
    check(next_layer.at(U'丁').sequences.size() == 1, "Explicit later layer wins");

    const DictionaryInfo info{index_format_version, "test-v1", "test-revision"};
    auto records = make_records(updated, Scope::traditional);
    const auto encoded = take(encode_index(records, info, "Test attribution\n"));
    std::reverse(records.begin(), records.end());
    check(encoded == take(encode_index(records, info, "Test attribution\n")), "Deterministic serialization");
    const auto dictionary = take(IndexedDictionary::decode(encoded));
    check(dictionary->info().format_version == 2, "New indexes use score format 2");
    check(take(dictionary->lookup(prefix("1"))).front().frequency_score == 0, "Unscored source defaults to zero");
    auto obsolete = encoded;
    put(obsolete, 8, 1);
    const auto rejected = IndexedDictionary::decode(obsolete);
    check(failed(rejected) && std::get<Error>(rejected).code == ErrorCode::unsupported_version,
          "Old index format explicitly rejected without conversion");
    const auto new_scores = take(IndexedDictionary::decode(take(encode_index(
        {{"1", U'一', 0xFFFFFFFFU}}, info, "Score boundary fixture"))));
    check(take(new_scores->lookup(prefix("1"))).front().frequency_score == 0xFFFFFFFFU,
          "Maximum version 2 score is a valid score, not a sentinel");
    check(failed(encode_index({{"1", U'一', 1}}, {1, "old", "fixture"}, "test")),
          "Writer refuses legacy format to avoid ambiguous semantics");
    check(failed(encode_index({{"1", U'一', 1}, {"11", U'一', 2}}, info, "test")),
          "Conflicting scores for one character rejected");
    check(dictionary->info().data_version == "test-v1" && dictionary->notice() == "Test attribution\n", "Metadata retained");
    auto results = take(dictionary->lookup(prefix("1")));
    check(results.size() == 3, "Prefix lookup includes exact and longer codes");
    check(results.front().character == U'一' && results.front().exact_match, "Exact flag correct");
    results = take(dictionary->lookup(prefix("251")));
    check(results.size() == 1 && results[0].character == U'國', "Deduplicate a character with multiple matching codes");
    results = take(dictionary->lookup(prefix("121251")));
    check(results.size() == 1 && results[0].character == U'𠮷' && results[0].exact_match, "Supplementary scalar roundtrip");
    check(take(dictionary->lookup({})).empty(), "Empty prefix returns no candidates");
    check(failed(dictionary->lookup(prefix("0"))), "Invalid query rejected");
    check(take(dictionary->lookup(prefix("55555"))).empty(), "Missing query is successful empty result");
    records.push_back(records.front());
    check(failed(encode_index(records, info, "notice")), "Duplicate records rejected");
    check(checksum("123456789") == 0xCBF43926U, "CRC32 independent known vector");
    for (std::size_t length = 0; length < encoded.size(); ++length) {
        check(failed(IndexedDictionary::decode(std::string_view(encoded).substr(0, length))), "Every truncated index rejected");
    }
    auto damaged = encoded;
    damaged.back() ^= 1;
    check(failed(IndexedDictionary::decode(damaged)), "Corruption detected");
    damaged = encoded; put(damaged, 8, index_format_version + 1);
    check(failed(IndexedDictionary::decode(damaged)), "Future index version rejected");
    damaged = encoded; put(damaged, 16, 0xFFFFFFFFU);
    check(failed(IndexedDictionary::decode(damaged)), "Oversized metadata rejected before allocation");
    damaged = encoded; put(damaged, 12, 0xFFFFFFFFU);
    check(failed(IndexedDictionary::decode(damaged)), "Oversized count rejected");
    damaged = encoded; damaged += 'x'; recalculate(damaged);
    check(failed(IndexedDictionary::decode(damaged)), "Trailing bytes rejected even with valid checksum");
    const auto record_offset = 24U + get(encoded, 16);
    damaged = encoded; put(damaged, record_offset, 0xD800); recalculate(damaged);
    check(failed(IndexedDictionary::decode(damaged)), "Invalid scalar rejected despite valid checksum");
    damaged = encoded; damaged[record_offset + 12] = '0'; recalculate(damaged);
    check(failed(IndexedDictionary::decode(damaged)), "Invalid stroke rejected despite valid checksum");
    damaged = encoded; damaged[record_offset + 12] = '5'; recalculate(damaged);
    check(failed(IndexedDictionary::decode(damaged)), "Unsorted records rejected despite valid checksum");
    check(failed(decode_utf8("\xC0\xAF")), "Reject overlong UTF-8");
    check(failed(decode_utf8("\xED\xA0\x80")), "Reject UTF-8 encoded surrogate");
    check(take(decode_utf8(encode_utf8(U'𠮷'))) == U"𠮷", "Unicode supplementary roundtrip");
}
void full_test(const std::filesystem::path& root) {
    using namespace stroke;
    using namespace stroke::build;
    const auto data = take(parse_conway(take(read_bytes(root / "codepoint-character-sequence.txt"))));
    take(verify_reference(data, take(read_bytes(root / "sequence-characters.txt"))));
    check(data.size() == 28165, "Pinned full character count");
    const auto all = make_records(data, Scope::all);
    check(all.size() == 63006, "Pinned full pair count");
    const auto traditional = make_records(data, Scope::traditional);
    std::set<char32_t> chars;
    for (const auto& r : traditional) { chars.insert(r.character); }
    check(chars.size() == 25611, "Shared plus traditional count");
    check(chars.contains(U'一') && chars.contains(U'國') && !chars.contains(U'汉'), "Actual traditional filtering");
    check(chars.contains(U'国'), "Unmarked historical variants remain under conservative upstream policy");
    const auto dictionary = take(IndexedDictionary::decode(take(encode_index(traditional,
        {index_format_version, "full-test", "d66ba5f5aa4cb6583883dfe8c14de553bb43616a"}, "Conway CC-BY-4.0 test"))));
    // Compare the binary-search result with an independent linear oracle for every 1..3 stroke prefix.
    for (int code = 1; code <= 555; ++code) {
        const auto key = std::to_string(code);
        if (key.find_first_not_of("12345") != std::string::npos) { continue; }
        std::map<char32_t, bool> expected;
        for (const auto& r : traditional) {
            if (r.sequence.starts_with(key)) { expected[r.character] = expected[r.character] || r.sequence == key; }
        }
        std::map<char32_t, bool> actual;
        for (const auto& candidate : take(dictionary->lookup(prefix(key)))) { actual.emplace(candidate.character, candidate.exact_match); }
        check(actual == expected, "Prefix index agrees with independent oracle");
    }
    std::cout << "Conway: " << data.size() << " chars, " << all.size() << " pairs; traditional: "
              << chars.size() << " chars, " << traditional.size() << " pairs\n";
}
} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc == 2) { full_test(std::filesystem::path(argv[1])); }
        else { unit_tests(); }
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    return failures == 0 ? 0 : 1;
}
