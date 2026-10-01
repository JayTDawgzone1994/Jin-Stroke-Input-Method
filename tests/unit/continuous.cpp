#include <iostream>
#include <stdexcept>
#include <stroke/dictionary/index.hpp>
#include <stroke/engine/continuous.hpp>
using namespace stroke;
void check(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
template <class T> T take(Result<T> result) {
    if (auto error = std::get_if<Error>(&result))
        throw std::runtime_error(error->message);
    return std::get<T>(std::move(result));
}
int main() {
    try {
        auto dictionary = take(IndexedDictionary::decode(take(
            encode_index({{"1", U'你', 20}, {"1", U'妳', 10}, {"2", U'好', 20}, {"3", U'𠮷', 20}},
                         {index_format_version, "test", "test"}, "fixture"))));
        auto config = default_keyboard_config();
        ContinuousSession session;
        take(session.configure(dictionary, config));
        check(!take(session.action(ContinuousAction::next_character)).consumed,
              "Idle space belongs to host");
        take(session.key('a'));
        check(session.text() == U"一" && session.text_cursor() == 1,
              "Unaccepted query shows strokes even with a matching candidate");
        take(session.action(ContinuousAction::next_character));
        check(session.text() == U"你" && session.query().snapshot().strokes.empty(),
              "Space accepts without committing");
        take(session.action(ContinuousAction::next_character));
        check(session.draft().size() == 1, "Space never duplicates");
        take(session.key('s'));
        take(session.action(ContinuousAction::next_character));
        check(session.text() == U"你好", "Two characters retained");
        take(session.action(ContinuousAction::left));
        take(session.action(ContinuousAction::left));
        take(session.action(ContinuousAction::candidates));
        check(session.choosing() && session.text() == U"你好",
              "Reopen original strokes without changing chosen word");
        take(session.select(1));
        check(session.text() == U"妳好", "Replace only selected character");
        auto commit = take(session.action(ContinuousAction::enter));
        check(commit.commit == U"妳好" && session.pending(), "Enter commits whole phrase");
        check(session.complete(false).empty() && session.text() == U"妳好",
              "Refusal retains phrase");
        take(session.action(ContinuousAction::enter));
        check(session.complete(true) == U"妳好" && !session.active(),
              "Learn only confirmed phrase");
        take(session.key('d'));
        take(session.action(ContinuousAction::next_character));
        check(session.text() == U"𠮷" && session.text_cursor() == 1, "Unicode scalar cursor");
        take(session.action(ContinuousAction::left));
        take(session.key('a'));
        take(session.key('e'));
        check(session.text() == U"一＊𠮷" && session.text_cursor() == 2,
              "Pending strokes and wildcard insert before the accepted Unicode character");
        take(session.action(ContinuousAction::backspace));
        check(session.text() == U"一𠮷" && session.text_cursor() == 1,
              "Backspace removes only the last pending stroke");
        take(session.action(ContinuousAction::next_character));
        check(session.text() == U"你𠮷", "Insert at caret");
        take(session.action(ContinuousAction::backspace));
        check(session.text() == U"𠮷", "Delete preceding character");
        take(session.key('q'));
        check(session.query().snapshot().visible_candidates.empty(), "Unmatched strokes");
        check(std::holds_alternative<Error>(session.action(ContinuousAction::next_character)),
              "No candidate does not advance");
        take(session.action(ContinuousAction::candidates));
        take(session.action(ContinuousAction::cancel));
        check(!session.query().snapshot().strokes.empty(), "First escape closes candidates");
        take(session.action(ContinuousAction::cancel));
        check(session.text() == U"𠮷", "Second escape clears query only");
        take(session.action(ContinuousAction::cancel));
        check(!session.active(), "Third escape cancels draft");
        take(session.key('a'));
        take(session.action(ContinuousAction::candidates));
        check(session.text() == U"一", "Opening candidates keeps unaccepted strokes visible");
        check(!take(session.action(ContinuousAction::enter)).commit && session.text() == U"你",
              "Enter in candidates only selects");
        check(std::holds_alternative<Error>(session.configure(dictionary, config)),
              "Config frozen during draft");
        session.reset();
        take(session.key('a'));
        check(take(session.action(ContinuousAction::enter)).commit == U"你",
              "Direct Enter commits a candidate, never the raw stroke preview");
        session.complete(true);
        for (int i = 0; i < 256; ++i) {
            take(session.key('a'));
            take(session.action(ContinuousAction::next_character));
        }
        check(std::holds_alternative<Error>(session.key('a')) && session.text().size() == 256,
              "Bounded draft preserves content");
        std::cout << "Continuous composition core passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
