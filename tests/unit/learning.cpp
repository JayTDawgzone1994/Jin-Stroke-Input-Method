#include "stroke/engine/session.hpp"
#include "stroke/dictionary/index.hpp"
#include <iostream>
#include <stdexcept>
using namespace stroke;
template<class T> T take(Result<T> value) {
    if (const auto* error = std::get_if<Error>(&value)) throw std::runtime_error(error->message);
    return std::get<T>(std::move(value));
}
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        check(learning_bonus(0) == 0 && learning_bonus(1) == 5454 && learning_bonus(5) == 20000 &&
              learning_bonus(20) == 40000 && learning_bonus(0xFFFFFFFFU) < 60000, "Bounded diminishing bonus");
        auto dictionary = take(IndexedDictionary::decode(take(encode_index(
            {{"1", U'一', 0}, {"12", U'丁', 20000}, {"1234", U'木', 0}},
            {index_format_version, "test", "test"}, "test"))));
        Session session;
        Config config; config.learning_enabled = true;
        take(session.configure(dictionary, config, {{U'木', 20}}));
        auto state = take(session.process(AppendStroke{Stroke::horizontal})).snapshot;
        check(state.visible_candidates[0].character == U'一' && state.visible_candidates[1].character == U'木',
              "Personal score promotes within match group; exact remains first");
        check(std::holds_alternative<Error>(session.configure(dictionary, config, {{U'丁', 500}})),
              "Cannot change learning snapshot mid-composition");
        auto selected = take(session.process(SelectCandidate{state.revision, 1}));
        check(!selected.learned_character && selected.commit.has_value(), "Selection alone does not learn");
        auto failed = take(session.complete_commit(selected.commit->revision, false));
        check(!failed.learned_character && failed.snapshot.visible_candidates == state.visible_candidates,
              "Failed host write neither learns nor reorders");
        selected = take(session.process(SelectCandidate{failed.snapshot.revision, 1}));
        auto success = take(session.complete_commit(selected.commit->revision, true));
        check(success.learned_character == U'木' && success.snapshot.phase == SessionPhase::idle,
              "Only successful insertion emits learned character");
        check(std::holds_alternative<Error>(session.complete_commit(selected.commit->revision, true)),
              "Repeated acknowledgement cannot double count");
        state = take(session.process(AppendStroke{Stroke::horizontal})).snapshot;
        selected = take(session.process(SelectCandidate{state.revision, 0}));
        check(!take(session.process(Cancel{})).learned_character &&
              std::holds_alternative<Error>(session.complete_commit(selected.commit->revision, true)), "Cancel never learns");
        config.learning_enabled = false;
        take(session.configure(dictionary, config, {{U'木', 1000000}}));
        state = take(session.process(AppendStroke{Stroke::horizontal})).snapshot;
        check(state.visible_candidates[1].character == U'丁', "Disabled learning ignores saved counts");
        selected = take(session.process(SelectCandidate{state.revision, 1}));
        check(!take(session.complete_commit(selected.commit->revision, true)).learned_character, "Disabled learning emits nothing");
        std::cout << "Learning scoring, stable composition and success-only events passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
