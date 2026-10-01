#include "stroke/engine/association.hpp"
#include <algorithm>
#include <tuple>
namespace stroke {
std::vector<Candidate> Association::accepted(char32_t character) {
    if (!is_unicode_scalar(character) || !character ||
        (!phrases_ && (!personal_ || personal_->empty()))) {
        reset();
        return {};
    }
    context_ += character;
    if (context_.size() >= max_phrase_length)
        context_.erase(0, context_.size() - (max_phrase_length - 1));
    struct Priority {
        std::uint32_t base{}, count{};
        bool manual{};
    };
    for (std::size_t start = 0; start < context_.size(); ++start) {
        const auto prefix = context_.substr(start);
        std::map<char32_t, Priority> choices;
        if (phrases_)
            for (const auto& candidate : phrases_->next_exact(prefix))
                choices[candidate.character].base = candidate.frequency_score;
        if (personal_) {
            for (auto it = personal_->lower_bound(prefix);
                 it != personal_->end() && it->first.starts_with(prefix); ++it) {
                const auto& [word, use] = *it;
                if (word.size() <= prefix.size() || !valid_personal_phrase(word))
                    continue;
                const auto cp = word[prefix.size()];
                if (use.manual || (learning_ && use.count >= personal_phrase_threshold))
                    choices[cp].manual |= use.manual;
            }
            if (learning_)
                for (auto& [cp, priority] : choices) {
                    auto key = prefix;
                    key += cp;
                    if (const auto it = personal_->find(key); it != personal_->end())
                        priority.count = it->second.count;
                }
        }
        if (choices.empty())
            continue;
        std::vector<std::pair<char32_t, Priority>> sorted(choices.begin(), choices.end());
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
            const auto rank = [](const Priority& p) {
                return std::tuple{p.count, p.manual, p.base};
            };
            return rank(a.second) != rank(b.second) ? rank(a.second) > rank(b.second)
                                                    : a.first < b.first;
        });
        std::vector<Candidate> result;
        for (std::size_t i = 0; i < sorted.size(); ++i)
            result.push_back(
                {sorted[i].first, static_cast<std::uint32_t>(sorted.size() - i), false});
        return result;
    }
    return {};
}
} // namespace stroke
