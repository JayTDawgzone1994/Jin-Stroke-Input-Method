#include <chrono>
#include <iostream>
#include <stdexcept>
#include <stroke/dictionary/text.hpp>
#include <stroke/engine/continuous.hpp>
#include <stroke/engine/phrase_rank.hpp>
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
const std::string revision(40, 'a');
std::string file(std::string body) {
    return "STROKE-PHRASES-1\ntest\n" + revision + '\n' + std::to_string(checksum(body)) + '\n' +
           body;
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    try {
        const auto phrases = take(PhraseIndex::decode(
            take(encode_phrases({{U"你好", 1000}, {U"世界", 500}}, "test", revision))));
        check(phrases->size() == 2, "Phrase count");
        const auto dictionary = take(IndexedDictionary::decode(
            take(encode_index({{"1", U'女', 100},
                               {"1", U'你', 90},
                               {"1", U'妳', 80},
                               {"2", U'子', 100},
                               {"2", U'好', 90},
                               {"3", U'世', 100},
                               {"4", U'界', 100},
                               {"5", U'𠮷', 100}},
                              {index_format_version, "test", "test"}, "fixture"))));
        ContinuousSession session;
        take(session.configure(dictionary, default_keyboard_config(), {}, phrases));
        auto append = [&](char key) {
            take(session.key(key));
            take(session.action(ContinuousAction::next_character));
        };
        append('a');
        check(session.text() == U"女", "First character uses single fallback");
        take(session.key('s'));
        check(session.text() == U"女丨" && session.text_cursor() == 2,
              "Pending strokes do not yet replace draft characters through phrase prediction");
        take(session.action(ContinuousAction::next_character));
        check(session.text() == U"你好", "Phrase can correct both provisional characters");
        take(session.action(ContinuousAction::backspace));
        check(session.text() == U"女",
              "Removing the final phrase character restores single fallback");
        append('s');
        take(session.action(ContinuousAction::left));
        take(session.action(ContinuousAction::candidates));
        check(session.query().snapshot().visible_candidates.front().character == U'好',
              "Reopened query shows contextual character first");
        take(session.select(1));
        check(session.text() == U"女子",
              "Manual replacement is locked; other character falls back");
        append('d');
        append('q');
        check(session.text() == U"女子世界", "Locked character survives later phrase prediction");
        auto commit = take(session.action(ContinuousAction::enter));
        check(commit.commit == U"女子世界", "Commit uses predicted text");
        session.complete(false);
        check(session.text() == U"女子世界" && session.draft()[1].locked,
              "Failed commit preserves choices and locks");
        session.reset();
        take(session.key('a'));
        take(session.select(0));
        append('s');
        check(session.text() == U"女子", "Explicit number selection blocks incompatible phrase");
        session.reset();
        append('a');
        append('w');
        check(session.text() == U"女𠮷",
              "Unknown combination and supplementary character fallback");
        session.reset();
        append('a');
        append('s');
        take(session.action(ContinuousAction::left));
        append('w');
        check(session.text() == U"女𠮷子", "Middle insertion invalidates old phrase");
        take(session.action(ContinuousAction::backspace));
        check(session.text() == U"你好", "Deleting inserted character restores phrase");
        auto high = take(PhraseIndex::decode(
            take(encode_phrases({{U"妳好", 4000}, {U"你好", 1}}, "test", revision))));
        session.reset();
        take(session.configure(dictionary, default_keyboard_config(), {}, high));
        append('a');
        append('s');
        check(session.text() == U"妳好", "Higher phrase score wins over lower-priority variant");
        session.reset();
        take(session.configure(dictionary, default_keyboard_config()));
        append('a');
        append('s');
        check(session.text() == U"女子", "Optional lexicon absent preserves legacy behavior");
        auto different = take(
            PhraseIndex::decode(take(encode_phrases({{U"你好", 100}}, "different", revision))));
        session.reset();
        check(std::holds_alternative<Error>(
                  session.configure(dictionary, default_keyboard_config(), {}, different)),
              "Incompatible stroke version rejected");
        for (const auto& body :
             {std::string("你好\t1\n你好\t2\n"), std::string("你好\t4294967296\n"),
              std::string("你\t1\n"), std::string("你好\t1"), std::string("你好\t-1\n"),
              std::string("\xff\t1\n"), std::string{}})
            check(std::holds_alternative<Error>(PhraseIndex::decode(file(body))),
                  "Malformed lexicon rejected");
        auto corrupt = take(encode_phrases({{U"你好", 100}}, "test", revision));
        corrupt.back() = 'x';
        check(std::holds_alternative<Error>(PhraseIndex::decode(corrupt)),
              "Checksum mismatch rejected");
        check(std::holds_alternative<Error>(PhraseIndex::decode("STROKE-PHRASES-0\n")),
              "Only current phrase format accepted");
        const auto supplementary =
            take(PhraseIndex::decode(take(encode_phrases({{U"𠮷好", 100}}, "test", revision))));
        check(supplementary->matches({{U'𠮷'}, {U'好'}}, 0).size() == 1,
              "Unicode scalar phrase traversal");
        const auto overlap = take(PhraseIndex::decode(
            take(encode_phrases({{U"你好", 1}, {U"好世", 100000}}, "test", revision))));
        check(predict_phrases(*overlap,
                              {{{U'女'}, {U'你'}}, {{U'子'}, {U'好'}}, {{U'是'}, {U'世'}}}) ==
                  U"女好世",
              "Overlapping words retain all positions");
        if (argc == 2) {
            const auto real = take(PhraseIndex::load(std::filesystem::path(argv[1])));
            check(real->size() > 100000, "Real phrase coverage");
            const auto begin = std::chrono::steady_clock::now();
            std::vector<std::vector<Candidate>> broad(
                64, {{U'女'}, {U'你'}, {U'子'}, {U'好'}, {U'世'}, {U'界'}});
            for (int i = 0; i < 10; ++i)
                check(predict_phrases(*real, broad).size() == 64, "Long input remains bounded");
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - begin)
                                     .count();
            std::vector<Candidate> alternatives;
            const std::u32string common =
                U"一人是有我的不了在大這中來上國個到說們為子和你地出道也時年得就那要下以生會自著去"
                U"之過家學對可她裡後小麼心多天而能好都然沒日於起還發成事只作當想看文無開手用主行方"
                U"又如前所本見經頭面公同三已老從動兩長知民樣現分將外其把因些實回間正最";
            for (std::size_t i = 0; i < phrase_choice_limit; ++i)
                alternatives.push_back(Candidate{common[i]});
            const auto wide_begin = std::chrono::steady_clock::now();
            for (int i = 0; i < 3; ++i)
                check(predict_phrases(*real, std::vector<std::vector<Candidate>>(256, alternatives))
                              .size() == 256,
                      "Maximum composition with 64 alternatives per character remains bounded");
            const auto wide_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - wide_begin)
                                     .count();
            std::cout << "prediction_3x256_64choices_ms=" << wide_ms << '\n';
            std::cout << "real_phrases=" << real->size() << " prediction_10x64_ms=" << elapsed
                      << '\n';
        }
        std::cout << "Phrase codec and continuous prediction passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
