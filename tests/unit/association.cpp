#include <iostream>
#include <stdexcept>
#include <stroke/engine/association.hpp>
#include <stroke/engine/session.hpp>
using namespace stroke;
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class T> T take(Result<T> result) {
    if (auto error = std::get_if<Error>(&result))
        throw std::runtime_error(error->message);
    return std::get<T>(std::move(result));
}
int main() {
    try {
        auto scores =
            PhraseScores{{U"你好", 10},     {U"你好世界", 500}, {U"你們", 300}, {U"好人", 9999},
                         {U"世界和平", 50}, {U"𠮷好", 20},      {U"好𠮷", 20},  {U"你零", 0}};
        for (char32_t cp = U'一'; cp < U'一' + 20; ++cp)
            scores[std::u32string{U'多', cp}] = 100;
        auto phrases =
            take(PhraseIndex::decode(take(encode_phrases(scores, "test", std::string(40, 'a')))));
        check(phrases->next(U"").empty(), "No root suggestions");
        auto first = phrases->next(U"你");
        check(first.size() == 2 && first[0].character == U'好' && first[0].frequency_score == 500,
              "Aggregate by maximum descendant priority and ignore zero-score edges");
        check(phrases->next(U"你好").front().character == U'世',
              "Longest context wins over unrelated higher-score suffix");
        check(phrases->next(U"陌你好").front().character == U'世', "Suffix backoff");
        check(phrases->next(U"你好世界").front().character == U'和',
              "Completed word can chain into another word");
        check(phrases->next(U"未知").empty(), "Unknown context has no fabricated suggestions");
        check(phrases->next(U"𠮷").front().character == U'好', "Supplementary scalar prefix");
        Association association;
        association.configure(phrases);
        check(association.accepted(U'你').front().character == U'好',
              "Accepted character starts association");
        check(association.accepted(U'好').front().character == U'世',
              "Selected next character extends context");
        for (int i = 0; i < 100; ++i)
            association.accepted(U'你');
        check(association.context().size() == 7, "Committed history stays bounded");
        association.reset();
        check(association.context().empty(), "Focus/caret reset clears context");
        association.accepted(0xD800);
        check(association.context().empty(), "Invalid scalar clears context");
        association.configure({});
        check(association.accepted(U'你').empty(), "No lexicon fallback");
        auto personal = std::make_shared<PersonalPhrases>();
        (*personal)[U"你們"] = {5, 100, false};
        (*personal)[U"好人"] = {100, 100, false};
        (*personal)[U"錦筆劃"] = {0, 0, true};
        association.configure(phrases, personal);
        auto learned = association.accepted(U'你');
        check(learned.front().character == U'們',
              "Repeated contextual choice outranks dictionary priority");
        association.accepted(U'好');
        check(association.accepted(U'世').front().character == U'界',
              "Personal suffix cannot displace a longer usable dictionary context");
        association.reset();
        check(association.accepted(U'錦').front().character == U'筆' &&
                  association.accepted(U'筆').front().character == U'劃',
              "Manual word adds missing prefixes and chains through the whole word");
        auto automatic = std::make_shared<PersonalPhrases>();
        (*automatic)[U"錦綉"] = {2, 0, false};
        association.configure(phrases, automatic);
        check(association.accepted(U'錦').empty(),
              "Two observations do not create an automatic word");
        (*automatic)[U"錦綉"].count = 3;
        association.reset();
        check(association.accepted(U'錦').front().character == U'綉',
              "Third observation adds a new word");
        association.configure(phrases, personal, false);
        check(association.accepted(U'你').front().character == U'好',
              "Learning off restores original ordering");
        association.reset();
        check(association.accepted(U'錦').front().character == U'筆',
              "Manual words remain available with learning off");
        association.configure({}, personal, false);
        check(association.accepted(U'錦').front().character == U'筆',
              "Personal words work without the optional base lexicon");
        auto dictionary = take(IndexedDictionary::decode(take(
            encode_index({{"1", U'一', 1}}, {index_format_version, "test", "fixture"}, "test"))));
        auto config = default_keyboard_config();
        config.learning_enabled = false;
        Session session;
        take(session.configure(dictionary, config));
        take(session.suggest(phrases->next(U"多"), U"多"));
        auto snapshot = session.snapshot();
        check(snapshot.strokes.empty() && snapshot.association_prefix == U"多" &&
                  snapshot.visible_candidates.size() == 9,
              "Association UI uses paging without stroke composition");
        take(session.process(ChangePage{PageDirection::next}));
        check(session.snapshot().page_index == 1, "Association paging");
        take(session.set_candidate_pages({0, 3, 7}));
        auto selected = take(session.process(SelectCandidate{session.snapshot().revision, 0}));
        auto failed = take(session.complete_commit(selected.commit->revision, false));
        check(!failed.learned_character && failed.snapshot.association_prefix == U"多",
              "Failed insertion retains association for retry");
        auto retried = take(session.process(SelectCandidate{session.snapshot().revision, 0}));
        auto done = take(session.complete_commit(retried.commit->revision, true));
        check(!done.learned_character && done.snapshot.phase == SessionPhase::idle,
              "Associations work when learning disabled");
        take(session.suggest(phrases->next(U"你"), U"你"));
        take(session.process_key('a'));
        check(session.snapshot().association_prefix.empty() &&
                  session.snapshot().strokes == StrokeSequence{Stroke::horizontal},
              "New stroke starts ordinary query");
        session.reset();
        check(std::holds_alternative<Error>(session.suggest({{0, 1}}, U"你")),
              "Invalid suggestion rejected atomically");
        check(session.snapshot().phase == SessionPhase::idle, "Bad suggestion leaves session idle");
        std::cout << "Association prefix, priority, paging, transactions and lifecycle passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
