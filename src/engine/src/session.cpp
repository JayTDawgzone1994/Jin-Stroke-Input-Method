#include "stroke/engine/session.hpp"

#include <algorithm>
#include <map>
#include <type_traits>

namespace stroke {

namespace {
Result<std::vector<Candidate>> ranked(std::vector<Candidate> matches, const LearningCounts& learned, bool enabled) {
    std::map<char32_t, Candidate> unique;
    for (const auto& candidate : matches) {
        if (candidate.character == 0 || !is_unicode_scalar(candidate.character)) {
            return Error{ErrorCode::invalid_data, "Dictionary returned an invalid Unicode character"};
        }
        const auto [entry, inserted] = unique.emplace(candidate.character, candidate);
        if (!inserted) {
            entry->second.exact_match = entry->second.exact_match || candidate.exact_match;
            entry->second.frequency_score = std::max(entry->second.frequency_score, candidate.frequency_score);
        }
    }
    std::vector<Candidate> result;
    result.reserve(unique.size());
    for (const auto& [character, candidate] : unique) {
        static_cast<void>(character);
        result.push_back(candidate);
    }
    const auto score = [&](const Candidate& candidate) {
        const auto entry = learned.find(candidate.character);
        return static_cast<std::uint64_t>(candidate.frequency_score) +
            (enabled && entry != learned.end() ? learning_bonus(entry->second) : 0);
    };
    std::sort(result.begin(), result.end(), [&](const Candidate& a, const Candidate& b) {
        if (a.exact_match != b.exact_match) { return a.exact_match; }
        if (score(a) != score(b)) { return score(a) > score(b); }
        return a.character < b.character;
    });
    return result;
}
} // namespace

Status Session::configure(std::shared_ptr<const IDictionary> dictionary, Config config, LearningCounts learned) {
    if (state_.phase != SessionPhase::idle) {
        return Error{ErrorCode::unavailable, "Configuration may only change while idle"};
    }
    if (!dictionary) { return Error{ErrorCode::invalid_argument, "Dictionary is required"}; }
    const auto status = validate(config);
    if (const auto* error = std::get_if<Error>(&status)) { return *error; }
    dictionary_ = std::move(dictionary);
    config_ = std::move(config);
    learned_ = std::move(learned);
    reset();
    return std::monostate{};
}

SessionSnapshot Session::snapshot() const { return state_; }
Status Session::set_candidate_pages(std::vector<std::size_t> pages) {
    if (state_.phase != SessionPhase::composing || pages.empty() || pages.front() != 0 ||
        pages.back() >= candidates_.size() || !std::is_sorted(pages.begin(), pages.end()) ||
        std::adjacent_find(pages.begin(), pages.end()) != pages.end())
        return Error{ErrorCode::invalid_argument, "Invalid candidate pages or pending commit"};
    if (pages == pages_) return std::monostate{};
    const auto current = pages_[state_.page_index];
    auto next = state_;
    next.page_index = static_cast<std::size_t>(std::upper_bound(pages.begin(), pages.end(), current) - pages.begin() - 1);
    const auto start = pages[next.page_index];
    const auto end = next.page_index + 1 < pages.size() ? pages[next.page_index + 1] : candidates_.size();
    next.visible_candidates.assign(candidates_.begin() + static_cast<std::ptrdiff_t>(start),
                                   candidates_.begin() + static_cast<std::ptrdiff_t>(end));
    next.has_next_page = end < candidates_.size();
    ++next.revision;
    pages_ = std::move(pages); state_ = std::move(next);
    return std::monostate{};
}
Update Session::update(bool consumed) const { return {consumed, state_, std::nullopt}; }

bool Session::handles_key(char key) const noexcept {
    return dictionary_ && map_key(config_, key).has_value();
}

Result<Update> Session::process_key(char key) {
    if (!handles_key(key)) { return update(false); }
    return process(AppendStroke{*map_key(config_, key)});
}

Result<Update> Session::refresh(StrokeSequence strokes) {
    if (!dictionary_) { return Error{ErrorCode::unavailable, "Session has no dictionary"}; }
    if (strokes.empty()) { reset(); return update(true); }
    auto lookup = dictionary_->lookup(strokes);
    if (const auto* error = std::get_if<Error>(&lookup)) { return *error; }
    auto ordered = ranked(std::get<std::vector<Candidate>>(std::move(lookup)), learned_, config_.learning_enabled);
    if (const auto* error = std::get_if<Error>(&ordered)) { return *error; }
    auto candidates = std::get<std::vector<Candidate>>(std::move(ordered));
    SessionSnapshot next;
    next.revision = state_.revision + 1;
    next.phase = SessionPhase::composing;
    next.strokes = std::move(strokes);
    next.total_candidates = candidates.size();
    const auto page_end = std::min(config_.page_size, candidates.size());
    next.visible_candidates.assign(candidates.begin(), candidates.begin() + static_cast<std::ptrdiff_t>(page_end));
    next.has_next_page = page_end < candidates.size();
    // Publish only after lookup, validation, ranking and page construction succeed.
    std::vector<std::size_t> pages;
    for (std::size_t i = 0; i < candidates.size(); i += config_.page_size) pages.push_back(i);
    pages_ = std::move(pages);
    candidates_ = std::move(candidates);
    state_ = std::move(next);
    return update(true);
}

Result<Update> Session::change_page(PageDirection direction) {
    if (direction != PageDirection::previous && direction != PageDirection::next) {
        return Error{ErrorCode::invalid_argument, "Invalid page direction"};
    }
    if (state_.phase == SessionPhase::idle) { return update(false); }
    if ((direction == PageDirection::previous && state_.page_index == 0) ||
        (direction == PageDirection::next && !state_.has_next_page)) { return update(true); }
    auto next = state_;
    if (direction == PageDirection::next) { ++next.page_index; }
    else { --next.page_index; }
    const auto start = pages_[next.page_index];
    const auto end = next.page_index + 1 < pages_.size() ? pages_[next.page_index + 1] : candidates_.size();
    next.visible_candidates.assign(candidates_.begin() + static_cast<std::ptrdiff_t>(start),
                                   candidates_.begin() + static_cast<std::ptrdiff_t>(end));
    next.has_next_page = end < candidates_.size();
    ++next.revision;
    state_ = std::move(next);
    return update(true);
}

Result<Update> Session::select(const SelectCandidate& selection) {
    if (selection.revision != state_.revision || selection.index >= state_.visible_candidates.size() ||
        state_.phase != SessionPhase::composing) {
        return Error{ErrorCode::invalid_argument, "Stale or invalid candidate selection"};
    }
    CommitRequest commit{state_.revision + 1,
                         std::u32string(1, state_.visible_candidates[selection.index].character)};
    state_.revision = commit.revision;
    pending_character_ = state_.visible_candidates[selection.index].character;
    state_.phase = SessionPhase::pending_commit;
    return Update{true, state_, std::move(commit)};
}

Result<Update> Session::process(const Command& command) {
    if (std::holds_alternative<Cancel>(command)) {
        if (state_.phase == SessionPhase::idle) { return update(false); }
        reset();
        return update(true);
    }
    if (state_.phase == SessionPhase::pending_commit) {
        return Error{ErrorCode::unavailable, "Awaiting host commit acknowledgement"};
    }
    return std::visit([&](const auto& value) -> Result<Update> {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, AppendStroke>) {
            if (!is_query_stroke(value.value) || state_.strokes.size() >= max_stroke_sequence_length) {
                return Error{ErrorCode::invalid_argument, "Invalid stroke or input length limit reached"};
            }
            auto next = state_.strokes;
            next.push_back(value.value);
            return refresh(std::move(next));
        } else if constexpr (std::is_same_v<T, Backspace>) {
            if (state_.strokes.empty()) { return update(false); }
            auto next = state_.strokes;
            next.pop_back();
            return refresh(std::move(next));
        } else if constexpr (std::is_same_v<T, ChangePage>) {
            return change_page(value.direction);
        } else if constexpr (std::is_same_v<T, SelectCandidate>) {
            return select(value);
        } else {
            return update(false); // Cancel handled above.
        }
    }, command);
}

Result<Update> Session::complete_commit(std::uint64_t revision, bool succeeded) {
    if (state_.phase != SessionPhase::pending_commit || revision != state_.revision) {
        return Error{ErrorCode::invalid_argument, "Stale commit acknowledgement"};
    }
    const auto learned = succeeded && config_.learning_enabled ? pending_character_ : std::nullopt;
    if (succeeded) { reset(); }
    else { pending_character_.reset(); state_.phase = SessionPhase::composing; ++state_.revision; }
    auto result = update(true);
    result.learned_character = learned;
    return result;
}

void Session::reset() noexcept {
    pending_character_.reset();
    ++state_.revision;
    state_.phase = SessionPhase::idle;
    state_.strokes.clear();
    state_.visible_candidates.clear();
    state_.page_index = 0;
    state_.total_candidates = 0;
    state_.has_next_page = false;
    candidates_.clear();
    pages_.clear();
}

} // namespace stroke
