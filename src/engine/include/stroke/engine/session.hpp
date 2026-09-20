#pragma once

#include "stroke/domain/commands.hpp"
#include "stroke/domain/config.hpp"
#include "stroke/dictionary/dictionary.hpp"
#include "stroke/engine/learning.hpp"

#include <memory>
#include <optional>

namespace stroke {

enum class SessionPhase { idle, composing, pending_commit };
struct SessionSnapshot {
    std::uint64_t revision{};
    SessionPhase phase{SessionPhase::idle};
    StrokeSequence strokes;
    std::vector<Candidate> visible_candidates;
    std::size_t page_index{};
    std::size_t total_candidates{};
    bool has_next_page{};
};
struct CommitRequest {
    std::uint64_t revision{};
    std::u32string text;
};
struct Update {
    bool consumed{};
    SessionSnapshot snapshot;
    std::optional<CommitRequest> commit;
    std::optional<char32_t> learned_character;
};

// One object per input context; thread-confined by its owner.
// Dictionary/configuration are held stable for the entire composition.
class Session final {
public:
    Session() = default;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) = delete;
    Session& operator=(Session&&) = delete;

    // Idle only; validation failure preserves the previous configuration.
    [[nodiscard]] Status configure(std::shared_ptr<const IDictionary> dictionary, Config config,
                                   LearningCounts learned = {});
    [[nodiscard]] SessionSnapshot snapshot() const;
    [[nodiscard]] const std::vector<Candidate>& candidates() const noexcept { return candidates_; }
    [[nodiscard]] const std::vector<std::size_t>& candidate_pages() const noexcept { return pages_; }
    // TSF hosts may supply their own page boundaries. Keeps the current first candidate visible.
    [[nodiscard]] Status set_candidate_pages(std::vector<std::size_t> pages);
    [[nodiscard]] Result<Update> process(const Command& command);
    // Logical ASCII key only. OS virtual keys/modifiers belong to the adapter.
    [[nodiscard]] bool handles_key(char key) const noexcept;
    [[nodiscard]] Result<Update> process_key(char key);
    // Only the adapter may acknowledge actual host insertion. Failed insertion keeps composition.
    [[nodiscard]] Result<Update> complete_commit(std::uint64_t revision, bool succeeded);
    void reset() noexcept;

private:
    [[nodiscard]] Result<Update> refresh(StrokeSequence strokes);
    [[nodiscard]] Update update(bool consumed) const;
    [[nodiscard]] Result<Update> change_page(PageDirection direction);
    [[nodiscard]] Result<Update> select(const SelectCandidate& selection);
    std::shared_ptr<const IDictionary> dictionary_;
    Config config_;
    SessionSnapshot state_;
    std::vector<Candidate> candidates_;
    std::vector<std::size_t> pages_;
    LearningCounts learned_;
    std::optional<char32_t> pending_character_;
};

} // namespace stroke
