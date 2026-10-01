#pragma once
#include "stroke/dictionary/phrases.hpp"
#include "stroke/engine/session.hpp"
namespace stroke {
struct DraftCharacter {
    StrokeSequence strokes;
    char32_t character{};
    std::vector<Candidate> alternatives;
    bool locked{};
};
enum class ContinuousAction { next_character, left, right, candidates, backspace, cancel, enter };
struct ContinuousUpdate {
    bool consumed{true};
    std::optional<std::u32string> commit;
};
// A transaction remains local until the host acknowledges final composition completion.
class ContinuousSession {
  public:
    Status configure(std::shared_ptr<const IDictionary> dictionary, Config config,
                     LearningCounts learned = {}, std::shared_ptr<const PhraseIndex> phrases = {});
    Session& query() noexcept { return query_; }
    const Session& query() const noexcept { return query_; }
    bool active() const {
        return !draft_.empty() || !query_.snapshot().strokes.empty() || pending_;
    }
    bool choosing() const noexcept { return choosing_; }
    bool pending() const noexcept { return pending_; }
    const std::vector<DraftCharacter>& draft() const noexcept { return draft_; }
    std::size_t cursor() const noexcept { return cursor_; }
    Result<ContinuousUpdate> key(char key);
    Result<ContinuousUpdate> action(ContinuousAction action);
    Result<ContinuousUpdate> select(std::size_t index);
    std::u32string text() const;
    std::size_t text_cursor() const;
    std::u32string complete(bool success);
    void reset() noexcept;

  private:
    Result<ContinuousUpdate> accept(std::size_t index, bool explicit_choice = false);
    void predict();
    void clear_query() noexcept;
    Session query_;
    std::shared_ptr<const PhraseIndex> phrases_;
    std::vector<DraftCharacter> draft_;
    std::size_t cursor_{};
    std::optional<std::size_t> editing_;
    bool choosing_{}, pending_{};
};
} // namespace stroke
