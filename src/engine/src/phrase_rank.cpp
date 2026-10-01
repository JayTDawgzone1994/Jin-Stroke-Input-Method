#include "stroke/engine/phrase_rank.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace stroke {
std::u32string predict_phrases(const PhraseIndex& phrases,
                               const std::vector<std::vector<Candidate>>& slots) {
    if (slots.empty() || slots.size() > 256)
        return {};
    PhraseLattice lattice;
    for (const auto& candidates : slots) {
        if (candidates.empty() || candidates.size() > phrase_choice_limit)
            return {};
        std::vector<char32_t> choices;
        for (const auto& candidate : candidates)
            choices.push_back(candidate.character);
        std::sort(choices.begin(), choices.end());
        choices.erase(std::unique(choices.begin(), choices.end()), choices.end());
        lattice.push_back(std::move(choices));
    }
    struct Step {
        double score{-std::numeric_limits<double>::infinity()};
        std::size_t previous{};
        std::u32string text;
    };
    std::vector<Step> best(slots.size() + 1);
    best[0].score = 0;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        auto improve = [&](std::size_t end, double score, std::u32string text) {
            if (score > best[end].score)
                best[end] = {score, i, std::move(text)};
        };
        improve(i + 1, best[i].score, std::u32string(1, slots[i].front().character));
        for (auto& match : phrases.matches(lattice, i)) {
            if (!match.score)
                continue;
            // Transparent heuristic: reward frequent multi-character words, penalize choices
            // far down the frequency/learning list. These priorities are not probabilities.
            double score = best[i].score + std::log1p(match.score) + 2.0 * (match.text.size() - 1);
            for (std::size_t offset = 0; offset < match.text.size(); ++offset) {
                const auto& choices = slots[i + offset];
                auto found = std::find_if(choices.begin(), choices.end(), [&](const auto& c) {
                    return c.character == match.text[offset];
                });
                score -= 2.0 * std::log1p(static_cast<double>(found - choices.begin()));
            }
            const auto end = i + match.text.size();
            improve(end, score, std::move(match.text));
        }
    }
    std::u32string result;
    for (std::size_t end = slots.size(); end; end = best[end].previous)
        result.insert(0, best[end].text);
    return result;
}
} // namespace stroke
