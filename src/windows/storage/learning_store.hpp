#pragma once
#include <stroke/domain/types.hpp>
#include <filesystem>
#include <map>

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
    bool operator==(const LearningState&) const = default;
};
std::filesystem::path learning_path();
// Nonblocking process-shared file lock; busy/unreadable returns Error, caller retries later.
// A successful result acknowledges the entire pending batch (or discards its obsolete generation).
Result<LearningState> sync_learning(const std::filesystem::path& path,
    const std::string& generation = {}, const LearnedUses& pending = {});
Result<LearningState> set_learning_enabled(const std::filesystem::path& path, bool enabled);
Result<LearningState> clear_learning(const std::filesystem::path& path);
}
