#include "stroke/dictionary/index.hpp"
#include "stroke/dictionary/text.hpp"
#include "stroke/dictionary/frequency.hpp"

#include <algorithm>
#include <map>
#include <tuple>

namespace stroke {
namespace {
constexpr std::string_view magic = "STROKE01";
constexpr std::size_t header_size = 24;
bool less_record(const IndexRecord& a, const IndexRecord& b) {
    return std::tie(a.sequence, a.character) < std::tie(b.sequence, b.character);
}
bool valid_sequence(std::string_view s) {
    return !s.empty() && s.size() <= max_sequence_length &&
           std::all_of(s.begin(), s.end(), [](char c) { return c >= '1' && c <= '5'; });
}
void put_u32(std::string& out, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) { out.push_back(static_cast<char>((value >> (8 * i)) & 0xFF)); }
}
void put_string(std::string& out, std::string_view value) {
    put_u32(out, static_cast<std::uint32_t>(value.size()));
    out.append(value);
}
class Reader {
public:
    explicit Reader(std::string_view value) : bytes(value) {}
    bool u32(std::uint32_t& result) {
        if (bytes.size() - offset < 4) { return false; }
        result = 0;
        for (unsigned i = 0; i < 4; ++i) {
            result |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset++])) << (i * 8);
        }
        return true;
    }
    bool string(std::string& result) {
        std::uint32_t size{};
        if (!u32(size) || size > bytes.size() - offset) { return false; }
        result.assign(bytes.substr(offset, size));
        offset += size;
        return true;
    }
    std::string_view bytes;
    std::size_t offset{};
};
Error invalid(std::string message) { return {ErrorCode::invalid_data, std::move(message)}; }
bool valid_text(const std::string& text) {
    return text.find('\0') == std::string::npos &&
           std::holds_alternative<std::u32string>(decode_utf8(text));
}
} // namespace

std::uint32_t checksum(std::string_view bytes) noexcept {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const unsigned char byte : bytes) {
        crc ^= byte;
        for (unsigned i = 0; i < 8; ++i) { crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U))); }
    }
    return ~crc;
}

Result<std::string> encode_index(std::vector<IndexRecord> records, const DictionaryInfo& info,
                                 std::string_view notice) {
    if (info.format_version != index_format_version) {
        return Error{ErrorCode::unsupported_version, "Unsupported index version"};
    }
    if (records.empty() || records.size() > 1000000 || info.data_version.empty() ||
        info.source_revision.empty() || notice.empty() || notice.size() > 1024 * 1024 ||
        info.data_version.size() > 1024 || info.source_revision.size() > 1024) {
        return invalid("Invalid index size or metadata");
    }
    std::sort(records.begin(), records.end(), less_record);
    std::string metadata;
    put_string(metadata, info.data_version);
    put_string(metadata, info.source_revision);
    put_string(metadata, notice);
    std::string payload = metadata;
    for (std::size_t i = 0; i < records.size(); ++i) {
        const auto& r = records[i];
        if (!valid_sequence(r.sequence) || !is_unicode_scalar(r.character) || r.character == 0 ||
            (i != 0 && !less_record(records[i - 1], r))) { return invalid("Invalid or duplicate index record"); }
        put_u32(payload, static_cast<std::uint32_t>(r.character));
        put_u32(payload, r.frequency_score);
        put_string(payload, r.sequence);
        if (payload.size() > max_index_bytes - header_size) { return invalid("Index exceeds size limit"); }
    }
    std::string out{magic};
    put_u32(out, index_format_version);
    put_u32(out, static_cast<std::uint32_t>(records.size()));
    put_u32(out, static_cast<std::uint32_t>(metadata.size()));
    put_u32(out, checksum(payload));
    out += payload;
    // Apply exactly the runtime validation rules before publishing an artifact.
    const auto checked = IndexedDictionary::decode(out);
    if (const auto* error = std::get_if<Error>(&checked)) { return *error; }
    return out;
}

Result<std::shared_ptr<const IndexedDictionary>> IndexedDictionary::load(const std::filesystem::path& path) {
    auto bytes = read_bytes(path, max_index_bytes);
    if (const auto* error = std::get_if<Error>(&bytes)) { return *error; }
    auto decoded = decode(std::get<std::string>(bytes));
    if (const auto* error = std::get_if<Error>(&decoded)) return *error;
    const auto frequency_path = path.parent_path() / "frequency" / "character-score.tsv";
    std::error_code ec;
    const bool present = std::filesystem::exists(frequency_path, ec);
    if (ec) return Error{ErrorCode::io_error, "Cannot access frequency table"};
    if (!present) return decoded;
    auto frequency_bytes = read_bytes(frequency_path, 4 * 1024 * 1024);
    if (const auto* error = std::get_if<Error>(&frequency_bytes)) return *error;
    auto parsed = parse_frequency(std::get<std::string>(frequency_bytes));
    if (const auto* error = std::get_if<Error>(&parsed)) return *error;
    const auto& scores = std::get<FrequencyScores>(parsed);
    auto dictionary = std::shared_ptr<IndexedDictionary>(new IndexedDictionary(
        *std::get<std::shared_ptr<const IndexedDictionary>>(decoded)));
    for (auto& record : dictionary->records_) {
        const auto entry = scores.find(record.character);
        record.frequency_score = entry == scores.end() ? 0 : entry->second;
    }
    dictionary->notice_ += "\nActive score overlay: frequency/character-score.tsv.\n"
        "libchewing-data, Copyright (c) 2025 libchewing Core Team, LGPL-2.1-or-later.\n"
        "Attribution and corresponding sources: frequency/NOTICE.txt and frequency/sources/.\n";
    return std::shared_ptr<const IndexedDictionary>(std::move(dictionary));
}

Result<std::shared_ptr<const IndexedDictionary>> IndexedDictionary::decode(std::string_view bytes) {
    if (bytes.size() < header_size || bytes.size() > max_index_bytes || !bytes.starts_with(magic)) {
        return invalid("Invalid index header");
    }
    Reader header(bytes.substr(8, 16));
    std::uint32_t version{}, count{}, metadata_size{}, crc{};
    if (!header.u32(version) || !header.u32(count) || !header.u32(metadata_size) || !header.u32(crc)) {
        return invalid("Truncated header");
    }
    if (version != index_format_version) {
        return Error{ErrorCode::unsupported_version, "Unsupported index version"};
    }
    const auto payload = bytes.substr(header_size);
    if (metadata_size > payload.size() || metadata_size > 1024 * 1024 + 4096 ||
        count == 0 || count > 1000000 || count > (payload.size() - metadata_size) / 13 ||
        checksum(payload) != crc) { return invalid("Invalid index size, count or checksum"); }
    auto dictionary = std::shared_ptr<IndexedDictionary>(new IndexedDictionary);
    dictionary->info_.format_version = version;
    Reader metadata(payload.substr(0, metadata_size));
    if (!metadata.string(dictionary->info_.data_version) || !metadata.string(dictionary->info_.source_revision) ||
        !metadata.string(dictionary->notice_) || metadata.offset != metadata.bytes.size() ||
        dictionary->info_.data_version.empty() || dictionary->info_.data_version.size() > 1024 ||
        dictionary->info_.source_revision.empty() || dictionary->info_.source_revision.size() > 1024 ||
        dictionary->notice_.empty() || !valid_text(dictionary->notice_) ||
        !valid_text(dictionary->info_.data_version) || !valid_text(dictionary->info_.source_revision)) {
        return invalid("Invalid metadata");
    }
    Reader reader(payload.substr(metadata_size));
    dictionary->records_.reserve(count);
    std::map<char32_t, std::uint32_t> scores;
    for (std::uint32_t i = 0; i < count; ++i) {
        IndexRecord r;
        std::uint32_t cp{};
        if (!reader.u32(cp) || !reader.u32(r.frequency_score) || !reader.string(r.sequence)) {
            return invalid("Truncated record");
        }
        r.character = static_cast<char32_t>(cp);
        if (!is_unicode_scalar(r.character) || cp == 0 || !valid_sequence(r.sequence) ||
            (!dictionary->records_.empty() && !less_record(dictionary->records_.back(), r))) {
            return invalid("Invalid character, sequence or record ordering");
        }
        const auto [score, inserted] = scores.emplace(r.character, r.frequency_score);
        if (!inserted && score->second != r.frequency_score) { return invalid("Conflicting scores for one character"); }
        dictionary->records_.push_back(std::move(r));
    }
    if (reader.offset != reader.bytes.size()) { return invalid("Unexpected trailing index data"); }
    return std::shared_ptr<const IndexedDictionary>(std::move(dictionary));
}

DictionaryInfo IndexedDictionary::info() const { return info_; }
std::size_t IndexedDictionary::record_count() const noexcept { return records_.size(); }
const std::string& IndexedDictionary::notice() const noexcept { return notice_; }

Result<std::vector<Candidate>> IndexedDictionary::lookup(std::span<const Stroke> prefix) const {
    if (prefix.size() > max_sequence_length) { return Error{ErrorCode::invalid_argument, "Prefix too long"}; }
    std::string key;
    for (const auto value : prefix) {
        if (!is_query_stroke(value)) { return Error{ErrorCode::invalid_argument, "Invalid stroke"}; }
        key.push_back(value == Stroke::wildcard ? '*' : static_cast<char>('0' + static_cast<unsigned>(value)));
    }
    std::vector<Candidate> result;
    if (key.empty()) { return result; }
    // Narrow by the literal prefix before the first wildcard. Scan that bounded range,
    // never expand wildcard combinations (which would grow as 5^N).
    const auto literal = key.substr(0, key.find('*'));
    auto it = std::lower_bound(records_.begin(), records_.end(), literal,
        [](const IndexRecord& r, std::string_view k) { return r.sequence < k; });
    std::map<char32_t, Candidate> candidates;
    for (; it != records_.end() && it->sequence.starts_with(literal); ++it) {
        if (it->sequence.size() < key.size()) continue;
        bool matches = true;
        for (std::size_t i = literal.size(); i < key.size(); ++i) {
            if (key[i] != '*' && key[i] != it->sequence[i]) { matches = false; break; }
        }
        if (!matches) continue;
        const bool exact = it->sequence.size() == key.size();
        const auto [entry, inserted] = candidates.emplace(it->character,
            Candidate{it->character, it->frequency_score, exact});
        if (!inserted) {
            entry->second.exact_match = entry->second.exact_match || exact;
            entry->second.frequency_score = std::max(entry->second.frequency_score, it->frequency_score);
        }
    }
    result.reserve(candidates.size());
    for (const auto& [cp, candidate] : candidates) { static_cast<void>(cp); result.push_back(candidate); }
    return result;
}

} // namespace stroke
