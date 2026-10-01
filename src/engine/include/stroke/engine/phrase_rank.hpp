#pragma once
#include "stroke/dictionary/phrases.hpp"
namespace stroke {
// Each slot is already ordered by single-character frequency and local learning.
// A manually fixed character is represented as a slot containing exactly one candidate.
[[nodiscard]] std::u32string predict_phrases(const PhraseIndex& phrases,
                                             const std::vector<std::vector<Candidate>>& slots);
} // namespace stroke
