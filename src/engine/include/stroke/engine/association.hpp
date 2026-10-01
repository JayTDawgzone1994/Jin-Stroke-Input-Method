#pragma once
#include "stroke/dictionary/phrases.hpp"
#include "stroke/domain/personal_phrase.hpp"
namespace stroke {
class Association final {
  public:
    void configure(std::shared_ptr<const PhraseIndex> phrases,
                   std::shared_ptr<const PersonalPhrases> personal = {}, bool learning = true) {
        phrases_ = std::move(phrases);
        set_personal(std::move(personal), learning);
        reset();
    }
    void set_personal(std::shared_ptr<const PersonalPhrases> personal, bool learning) {
        personal_ = std::move(personal);
        learning_ = learning;
    }
    std::vector<Candidate> accepted(char32_t character);
    const std::u32string& context() const noexcept { return context_; }
    void reset() noexcept { context_.clear(); }

  private:
    std::shared_ptr<const PhraseIndex> phrases_;
    std::shared_ptr<const PersonalPhrases> personal_;
    std::u32string context_;
    bool learning_{true};
};
} // namespace stroke
