#pragma once

#include "stroke/dictionary/dictionary.hpp"

#include <filesystem>
#include <memory>
#include <string_view>

namespace stroke {

inline constexpr std::uint32_t index_format_version = 2;
inline constexpr std::size_t max_sequence_length = max_stroke_sequence_length;
inline constexpr std::size_t max_index_bytes = 64 * 1024 * 1024;

struct IndexRecord {
    std::string sequence;
    char32_t character{};
    std::uint32_t frequency_score{};
    bool operator==(const IndexRecord&) const = default;
};

// Shared on-disk format support. Explicit little-endian fields, never raw C++ structs.
[[nodiscard]] std::uint32_t checksum(std::string_view bytes) noexcept;
[[nodiscard]] Result<std::string> encode_index(std::vector<IndexRecord> records,
                                              const DictionaryInfo& info,
                                              std::string_view notice);

class IndexedDictionary final : public IDictionary {
public:
    [[nodiscard]] static Result<std::shared_ptr<const IndexedDictionary>>
    load(const std::filesystem::path& path);
    [[nodiscard]] static Result<std::shared_ptr<const IndexedDictionary>>
    decode(std::string_view bytes);
    [[nodiscard]] DictionaryInfo info() const override;
    [[nodiscard]] Result<std::vector<Candidate>> lookup(std::span<const Stroke> prefix) const override;
    [[nodiscard]] std::size_t record_count() const noexcept;
    [[nodiscard]] const std::string& notice() const noexcept;

private:
    IndexedDictionary() = default;
    DictionaryInfo info_;
    std::string notice_;
    std::vector<IndexRecord> records_;
};

} // namespace stroke
