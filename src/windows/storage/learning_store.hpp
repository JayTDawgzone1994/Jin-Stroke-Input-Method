#pragma once
#include <filesystem>
#include <map>
#include <stroke/domain/personal_phrase.hpp>
#include <stroke/domain/types.hpp>

namespace stroke::win {
struct LearnedUse {
    std::uint32_t count{};
    std::uint64_t last_used{};
    bool operator==(const LearnedUse&) const = default;
};
using LearnedUses = std::map<char32_t, LearnedUse>;
struct LearningState {
    std::string generation;
    bool enabled{true};
    LearnedUses uses;
    PersonalPhrases phrases;
    bool operator==(const LearningState&) const = default;
};
std::filesystem::path learning_path();
// Nonblocking process-shared file lock; busy/unreadable returns Error, caller retries later.
// A successful result acknowledges the entire pending batch (or discards its obsolete generation).
Result<LearningState> sync_learning(const std::filesystem::path& path,
                                    const std::string& generation = {},
                                    const LearnedUses& pending = {},
                                    const PersonalPhrases& pending_phrases = {});
Result<LearningState> set_learning_enabled(const std::filesystem::path& path, bool enabled);
Result<LearningState> clear_learning(const std::filesystem::path& path);
Result<LearningState> add_personal_phrase(const std::filesystem::path& path, std::u32string word);
Result<LearningState> remove_personal_phrase(const std::filesystem::path& path,
                                             std::u32string word);
} // namespace stroke::win
