#pragma once
#include "stroke/dictionary/index.hpp"
#include <map>
#include <optional>
namespace stroke {
inline constexpr std::size_t max_phrase_length = 8;
inline constexpr std::size_t phrase_choice_limit = 64;
using PhraseScores = std::map<std::u32string, std::uint32_t>;
struct PhraseMatch {
    std::u32string text;
    std::uint32_t score{};
};
// Sorted unique Unicode choices at each character position.
using PhraseLattice = std::vector<std::vector<char32_t>>;
[[nodiscard]] Result<std::string> encode_phrases(const PhraseScores& scores,
                                                 std::string_view stroke_version,
                                                 std::string_view revision);
class PhraseIndex final {
  public:
    [[nodiscard]] static Result<std::shared_ptr<const PhraseIndex>> decode(std::string_view bytes);
    [[nodiscard]] static Result<std::shared_ptr<const PhraseIndex>>
    load(const std::filesystem::path& path);
    [[nodiscard]] const std::string& stroke_version() const noexcept { return stroke_version_; }
    [[nodiscard]] const std::string& revision() const noexcept { return revision_; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    // Bounded trie intersection, not an exponential enumeration of all character combinations.
    // Longest usable suffix of the committed context; priorities are maximum descendant scores.
    [[nodiscard]] std::vector<Candidate> next(std::u32string_view context) const;
    [[nodiscard]] std::vector<Candidate> next_exact(std::u32string_view prefix) const;
    [[nodiscard]] std::vector<PhraseMatch> matches(const PhraseLattice& lattice,
                                                   std::size_t start) const;

  private:
    struct Node {
        std::map<char32_t, std::size_t> edges;
        std::optional<std::uint32_t> score;
        std::uint32_t continuation_score{};
    };
    std::vector<Node> nodes_{1};
    std::string stroke_version_, revision_;
    std::size_t size_{};
};
} // namespace stroke
