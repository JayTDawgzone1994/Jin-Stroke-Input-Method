#include "stroke/dictionary/phrases.hpp"
#include "stroke/dictionary/text.hpp"
#include <algorithm>
#include <charconv>
namespace stroke {
namespace {
constexpr std::string_view header = "STROKE-PHRASES-1\n";
constexpr std::size_t byte_limit = 16 * 1024 * 1024;
bool metadata(std::string_view version, std::string_view revision) {
    return !version.empty() && version.size() <= 256 &&
           version.find_first_of("\r\n\t") == std::string_view::npos && revision.size() == 40 &&
           revision.find_first_not_of("0123456789abcdefABCDEF") == std::string_view::npos;
}
} // namespace
Result<std::string> encode_phrases(const PhraseScores& scores, std::string_view version,
                                   std::string_view revision) {
    if (!metadata(version, revision) || scores.empty() || scores.size() > 200000)
        return Error{ErrorCode::invalid_argument, "Invalid phrase metadata or count"};
    std::string body;
    for (const auto& [term, score] : scores) {
        if (term.size() < 2 || term.size() > max_phrase_length)
            return Error{ErrorCode::invalid_argument, "Invalid phrase length"};
        for (auto cp : term) {
            if (!is_unicode_scalar(cp) || cp < 0x100 || cp == 0xFEFF)
                return Error{ErrorCode::invalid_argument, "Invalid phrase scalar"};
            body += encode_utf8(cp);
        }
        body += '\t' + std::to_string(score) + '\n';
    }
    std::string out = std::string(header) + std::string(version) + '\n' + std::string(revision) +
                      '\n' + std::to_string(checksum(body)) + '\n' + body;
    if (out.size() > byte_limit)
        return Error{ErrorCode::invalid_argument, "Phrase file too large"};
    return out;
}
Result<std::shared_ptr<const PhraseIndex>> PhraseIndex::load(const std::filesystem::path& path) {
    auto bytes = read_bytes(path, byte_limit);
    if (auto error = std::get_if<Error>(&bytes))
        return *error;
    return decode(std::get<std::string>(bytes));
}
Result<std::shared_ptr<const PhraseIndex>> PhraseIndex::decode(std::string_view bytes) {
    if (bytes.size() > byte_limit || !bytes.starts_with(header))
        return Error{ErrorCode::invalid_data, "Invalid phrase header or size"};
    bytes.remove_prefix(header.size());
    auto line = [&]() {
        const auto end = bytes.find('\n');
        if (end == std::string_view::npos)
            return std::string_view{};
        auto value = bytes.substr(0, end);
        bytes.remove_prefix(end + 1);
        return value;
    };
    const auto version = line(), revision = line(), crc = line();
    if (!metadata(version, revision) || crc.empty())
        return Error{ErrorCode::invalid_data, "Invalid phrase metadata or checksum"};
    std::uint32_t expected{};
    const auto [last, error] = std::from_chars(crc.data(), crc.data() + crc.size(), expected);
    if (error != std::errc{} || last != crc.data() + crc.size() || checksum(bytes) != expected)
        return Error{ErrorCode::invalid_data, "Invalid phrase metadata or checksum"};
    std::shared_ptr<PhraseIndex> index(new PhraseIndex);
    index->stroke_version_ = version;
    index->revision_ = revision;
    std::u32string previous;
    while (!bytes.empty()) {
        auto row = line();
        const auto tab = row.find('\t');
        if (row.empty() || tab == std::string_view::npos)
            return Error{ErrorCode::invalid_data, "Invalid phrase row or missing final newline"};
        auto decoded = decode_utf8(row.substr(0, tab));
        const auto* term = std::get_if<std::u32string>(&decoded);
        auto number = row.substr(tab + 1);
        std::uint32_t score{};
        const auto [tail, err] =
            std::from_chars(number.data(), number.data() + number.size(), score);
        if (!term || term->size() < 2 || term->size() > max_phrase_length || *term <= previous ||
            err != std::errc{} || tail != number.data() + number.size() || ++index->size_ > 200000)
            return Error{ErrorCode::invalid_data, "Invalid or unsorted phrase"};
        std::size_t node{};
        for (auto cp : *term) {
            if (cp < 0x100 || cp == 0xFEFF)
                return Error{ErrorCode::invalid_data, "Invalid phrase scalar"};
            auto found = index->nodes_[node].edges.find(cp);
            if (found == index->nodes_[node].edges.end()) {
                const auto next = index->nodes_.size();
                index->nodes_[node].edges.emplace(cp, next);
                index->nodes_.emplace_back();
                node = next;
            } else
                node = found->second;
        }
        index->nodes_[node].score = score;
        previous = *term;
    }
    if (!index->size_)
        return Error{ErrorCode::invalid_data, "Empty phrase index"};
    // Descendants are created after parents, so one reverse pass caches continuation scores.
    for (std::size_t i = index->nodes_.size(); i-- > 0;) {
        auto& node = index->nodes_[i];
        node.continuation_score = node.score.value_or(0);
        for (const auto& [cp, child] : node.edges) {
            (void)cp;
            node.continuation_score =
                std::max(node.continuation_score, index->nodes_[child].continuation_score);
        }
    }
    return std::shared_ptr<const PhraseIndex>(std::move(index));
}
std::vector<Candidate> PhraseIndex::next(std::u32string_view context) const {
    if (context.empty())
        return {};
    if (context.size() >= max_phrase_length)
        context.remove_prefix(context.size() - (max_phrase_length - 1));
    for (std::size_t start = 0; start < context.size(); ++start) {
        auto result = next_exact(context.substr(start));
        if (result.empty())
            continue;
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            return a.frequency_score != b.frequency_score ? a.frequency_score > b.frequency_score
                                                          : a.character < b.character;
        });
        return result;
    }
    return {};
}
std::vector<Candidate> PhraseIndex::next_exact(std::u32string_view prefix) const {
    if (prefix.empty() || prefix.size() >= max_phrase_length)
        return {};
    std::size_t node = 0;
    for (auto cp : prefix) {
        const auto edge = nodes_[node].edges.find(cp);
        if (edge == nodes_[node].edges.end())
            return {};
        node = edge->second;
    }
    std::vector<Candidate> result;
    for (const auto& [cp, child] : nodes_[node].edges)
        if (const auto score = nodes_[child].continuation_score; score)
            result.push_back({cp, score, false});
    return result;
}
std::vector<PhraseMatch> PhraseIndex::matches(const PhraseLattice& lattice,
                                              std::size_t start) const {
    if (start >= lattice.size())
        return {};
    std::vector<PhraseMatch> matches;
    struct Path {
        std::size_t node;
        std::u32string text;
    };
    std::vector<Path> frontier{{0, {}}};
    // Limits pathological broad-prefix queries; normal precise input traverses a single path.
    std::size_t visited{};
    for (std::size_t depth = 0;
         depth < max_phrase_length && start + depth < lattice.size() && !frontier.empty();
         ++depth) {
        std::vector<Path> next;
        const auto& choices = lattice[start + depth];
        for (const auto& path : frontier) {
            const auto& edges = nodes_[path.node].edges;
            auto append = [&](char32_t cp, std::size_t node) {
                if (++visited > 8192)
                    return;
                auto text = path.text;
                text += cp;
                if (nodes_[node].score && depth)
                    matches.push_back({text, *nodes_[node].score});
                next.push_back({node, std::move(text)});
            };
            if (edges.size() < choices.size()) {
                for (const auto& [cp, node] : edges)
                    if (std::binary_search(choices.begin(), choices.end(), cp))
                        append(cp, node);
            } else {
                for (auto cp : choices)
                    if (auto edge = edges.find(cp); edge != edges.end())
                        append(cp, edge->second);
            }
            if (visited > 8192)
                break;
        }
        if (visited > 8192)
            break;
        frontier = std::move(next);
    }
    return matches;
}
} // namespace stroke
