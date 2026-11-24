#include "stroke/engine/continuous.hpp"
#include "stroke/engine/phrase_rank.hpp"
#include <algorithm>
namespace stroke {
Status ContinuousSession::configure(std::shared_ptr<const IDictionary> dictionary, Config config,
                                    LearningCounts learned,
                                    std::shared_ptr<const PhraseIndex> phrases) {
    if (active())
        return Error{ErrorCode::unavailable, "Composition is active"};
    if (phrases && (!dictionary || phrases->stroke_version() != dictionary->info().data_version))
        return Error{ErrorCode::invalid_data,
                     "Phrase index belongs to a different stroke dictionary"};
    auto result = query_.configure(std::move(dictionary), std::move(config), std::move(learned));
    if (!std::holds_alternative<Error>(result))
        phrases_ = std::move(phrases);
    return result;
}
void ContinuousSession::clear_query() noexcept {
    query_.reset();
    editing_.reset();
    choosing_ = false;
}
void ContinuousSession::reset() noexcept {
    clear_query();
    draft_.clear();
    cursor_ = 0;
    pending_ = false;
}
Result<ContinuousUpdate> ContinuousSession::key(char key) {
    if (pending_)
        return Error{ErrorCode::unavailable, "Commit pending"};
    if (!editing_ && query_.snapshot().strokes.empty() && draft_.size() >= 256)
        return Error{ErrorCode::invalid_argument, "Composition length limit"};
    auto result = query_.process_key(key);
    if (auto error = std::get_if<Error>(&result))
        return *error;
    return ContinuousUpdate{std::get<Update>(result).consumed};
}
Result<ContinuousUpdate> ContinuousSession::accept(std::size_t index, bool explicit_choice) {
    const auto snapshot = query_.snapshot();
    if (index >= snapshot.visible_candidates.size())
        return Error{ErrorCode::invalid_argument, "No candidate at this position"};
    DraftCharacter character;
    character.strokes = snapshot.strokes;
    character.character = snapshot.visible_candidates[index].character;
    character.locked =
        explicit_choice || choosing_ || editing_.has_value() || snapshot.page_index != 0;
    const auto& all = query_.candidates();
    character.alternatives.assign(all.begin(), all.begin() + static_cast<std::ptrdiff_t>(std::min(
                                                                 all.size(), phrase_choice_limit)));
    if (editing_) {
        draft_[*editing_] = std::move(character);
        cursor_ = *editing_ + 1;
    } else {
        if (draft_.size() >= 256)
            return Error{ErrorCode::invalid_argument, "Composition length limit"};
        draft_.insert(draft_.begin() + static_cast<std::ptrdiff_t>(cursor_), std::move(character));
        ++cursor_;
    }
    clear_query();
    predict();
    return ContinuousUpdate{};
}
Result<ContinuousUpdate> ContinuousSession::literal(char32_t character) {
    if (pending_)
        return Error{ErrorCode::unavailable, "Commit pending"};
    if (character < U' ' || character > U'~')
        return Error{ErrorCode::invalid_argument, "Expected printable English text"};
    if (draft_.size() >= 256)
        return Error{ErrorCode::invalid_argument, "Composition length limit"};
    // Discard only the unaccepted query, including a reopened candidate list.
    // Already accepted Chinese characters keep their saved strokes and alternatives.
    clear_query();
    DraftCharacter slot;
    slot.character = character;
    slot.locked = true;
    slot.literal = true;
    draft_.insert(draft_.begin() + static_cast<std::ptrdiff_t>(cursor_), std::move(slot));
    ++cursor_;
    return ContinuousUpdate{};
}
Result<ContinuousUpdate> ContinuousSession::select(std::size_t index) {
    if (pending_)
        return Error{ErrorCode::unavailable, "Commit pending"};
    return accept(index, true);
}
Result<ContinuousUpdate> ContinuousSession::action(ContinuousAction action) {
    if (pending_)
        return Error{ErrorCode::unavailable, "Commit pending"};
    const bool has_query = !query_.snapshot().strokes.empty();
    switch (action) {
    case ContinuousAction::discard_query:
        clear_query();
        return ContinuousUpdate{};
    case ContinuousAction::next_character:
        if (has_query)
            return accept(0);
        return ContinuousUpdate{active()};
    case ContinuousAction::enter:
        if (choosing_)
            return accept(0);
        if (has_query) {
            auto r = accept(0);
            if (auto e = std::get_if<Error>(&r))
                return *e;
        }
        if (draft_.empty())
            return ContinuousUpdate{false};
        pending_ = true;
        return ContinuousUpdate{true, text()};
    case ContinuousAction::left:
    case ContinuousAction::right:
        if (editing_)
            clear_query();
        else if (has_query) {
            auto r = accept(0);
            if (auto e = std::get_if<Error>(&r))
                return *e;
        }
        if (action == ContinuousAction::left && cursor_)
            --cursor_;
        if (action == ContinuousAction::right && cursor_ < draft_.size())
            ++cursor_;
        return ContinuousUpdate{active()};
    case ContinuousAction::candidates:
        if (has_query) {
            choosing_ = true;
            return ContinuousUpdate{};
        }
        if (draft_.empty())
            return ContinuousUpdate{false};
        {
            const auto target = cursor_ < draft_.size() ? cursor_ : draft_.size() - 1;
            if (draft_[target].literal)
                return ContinuousUpdate{false};
            // Replay a saved query; a failed replay never changes confirmed characters.
            for (const auto stroke : draft_[target].strokes) {
                auto r = query_.process(AppendStroke{stroke});
                if (auto e = std::get_if<Error>(&r)) {
                    auto error = *e;
                    clear_query();
                    return error;
                }
            }
            (void)query_.prefer_candidate(draft_[target].character);
            editing_ = target;
            cursor_ = target;
            choosing_ = true;
            return ContinuousUpdate{};
        }
    case ContinuousAction::backspace:
        if (has_query) {
            auto r = query_.process(Backspace{});
            if (auto e = std::get_if<Error>(&r))
                return *e;
            if (query_.snapshot().strokes.empty())
                clear_query();
        } else if (cursor_) {
            draft_.erase(draft_.begin() + static_cast<std::ptrdiff_t>(--cursor_));
            predict();
        }
        return ContinuousUpdate{};
    case ContinuousAction::cancel:
        if (choosing_) {
            choosing_ = false;
            if (editing_)
                clear_query();
        } else if (has_query)
            clear_query();
        else
            reset();
        return ContinuousUpdate{};
    }
    return ContinuousUpdate{false};
}
void ContinuousSession::predict() {
    if (!phrases_ || draft_.empty())
        return;
    // Literal text separates Chinese phrase runs and is never rewritten by prediction.
    for (std::size_t start = 0; start < draft_.size();) {
        if (draft_[start].literal) { ++start; continue; }
        auto end = start;
        std::vector<std::vector<Candidate>> choices;
        while (end < draft_.size() && !draft_[end].literal) {
            const auto& slot = draft_[end++];
            choices.push_back(slot.locked ? std::vector<Candidate>{Candidate{slot.character}}
                                          : slot.alternatives);
        }
        const auto text = predict_phrases(*phrases_, choices);
        if (text.size() == end - start)
            for (std::size_t i = 0; i < text.size(); ++i)
                draft_[start + i].character = text[i];
        start = end;
    }
}
std::u32string ContinuousSession::text() const {
    std::u32string result;
    const auto snapshot = query_.snapshot();
    for (std::size_t i = 0; i <= draft_.size(); ++i) {
        if (i == cursor_ && !snapshot.strokes.empty() && !editing_) {
            for (const auto stroke : snapshot.strokes)
                result += U"一丨丿丶フ＊"[static_cast<unsigned>(stroke) - 1];
        }
        if (i < draft_.size())
            result += draft_[i].character;
    }
    return result;
}
std::size_t ContinuousSession::text_cursor() const {
    if (editing_)
        return *editing_;
    const auto snapshot = query_.snapshot();
    return cursor_ + snapshot.strokes.size();
}
std::u32string ContinuousSession::complete(bool success) {
    if (!pending_)
        return {};
    pending_ = false;
    if (!success)
        return {};
    auto result = text();
    reset();
    return result;
}
} // namespace stroke
