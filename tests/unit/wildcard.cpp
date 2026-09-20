#include <stroke/dictionary/index.hpp>
#include <stroke/engine/session.hpp>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace stroke;
template<class T> T take(Result<T> result) {
    if (auto* error = std::get_if<Error>(&result)) throw std::runtime_error(error->message);
    return std::get<T>(std::move(result));
}
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
StrokeSequence query(std::string_view text) {
    StrokeSequence sequence;
    for (char c : text) sequence.push_back(c == '*' ? Stroke::wildcard : static_cast<Stroke>(c - '0'));
    return sequence;
}
int main(int argc, char** argv) {
    try {
        const std::vector<IndexRecord> records{{"1",U'一',1},{"12",U'十',2},{"1234",U'木',3},
            {"134",U'大',4},{"251",U'口',5},{"13",U'十',2},{"111",U'三',6},{"512",U'𠮷',7}};
        auto dictionary = take(IndexedDictionary::decode(take(encode_index(records,{index_format_version,"test","fixture"},"test"))));
        // Exhaustively compare every 1..4-token pattern with a simple independent oracle.
        for (std::size_t length=1; length<=4; ++length) {
            std::size_t count=1; for (std::size_t i=0;i<length;++i) count*=6;
            for (std::size_t pattern=0;pattern<count;++pattern) {
                auto remaining=pattern;
                StrokeSequence tokens;
                for (std::size_t i=0;i<length;++i) { tokens.push_back(static_cast<Stroke>(remaining%6+1)); remaining/=6; }
                std::map<char32_t,bool> expected, actual;
                for (const auto& record : records) {
                    if (record.sequence.size()<length) continue;
                    bool matches=true;
                    for (std::size_t i=0;i<length;++i)
                        if (tokens[i]!=Stroke::wildcard && static_cast<unsigned>(tokens[i])!=static_cast<unsigned>(record.sequence[i]-'0')) matches=false;
                    if (matches) expected[record.character] = expected[record.character] || record.sequence.size()==length;
                }
                for (const auto& candidate : take(dictionary->lookup(tokens))) actual.emplace(candidate.character,candidate.exact_match);
                check(actual==expected,"Wildcard oracle mismatch");
            }
        }
        check(take(dictionary->lookup(query("1*34"))).size()==1,"Wildcard consumes exactly one stroke");
        check(take(dictionary->lookup(query("1*4"))).front().character==U'大',"No variable-length wildcard");
        check(take(dictionary->lookup(StrokeSequence(128,Stroke::wildcard))).empty(),"Long wildcard query terminates");
        check(std::holds_alternative<Error>(dictionary->lookup(StrokeSequence(129,Stroke::wildcard))),"Length limit retained");
        check(std::holds_alternative<Error>(dictionary->lookup(query("7"))),"Invalid token rejected");
        check(std::holds_alternative<Error>(encode_index({{"1*",U'一',0}},{index_format_version,"test","test"},"test")),"Wildcard cannot enter stored index");
        auto config=default_keyboard_config();
        take(validate(config));
        const std::string left="qweasd",right="uiojkl";
        const Stroke expected[]{Stroke::dot,Stroke::turn,Stroke::wildcard,Stroke::horizontal,Stroke::vertical,Stroke::left_falling};
        for (std::size_t i=0;i<6;++i) {
            check(map_key(config,left[i])==expected[i] && map_key(config,right[i])==expected[i],"Default keyboard mapping");
        }
        Session session; take(session.configure(dictionary,config));
        take(session.process_key('a')); take(session.process_key('o')); take(session.process_key('d'));
        auto update=take(session.process_key('u'));
        check(update.snapshot.strokes==query("1*34") && update.snapshot.visible_candidates.front().character==U'木',"Mixed hands and wildcard compose");
        auto selected=take(session.process(SelectCandidate{update.snapshot.revision,0}));
        check(selected.commit && selected.commit->text==U"木","Wildcard selection commits character");
        take(session.complete_commit(selected.commit->revision,false));
        check(session.snapshot().strokes==query("1*34"),"Failed commit preserves wildcard");
        take(session.process(Backspace{})); take(session.process(Backspace{})); take(session.process(Backspace{}));
        check(session.snapshot().strokes==query("1"),"Backspace removes wildcard");
        take(session.process(Cancel{}));
        take(session.process_key('e'));
        check(session.snapshot().strokes==query("*"),"Leading wildcard accepted");
        take(session.process(Cancel{}));
        if (argc==2) {
            auto real=take(IndexedDictionary::load(argv[1]));
            const auto start=std::chrono::steady_clock::now();
            for (int repeat=0;repeat<5;++repeat) {
                check(!take(real->lookup(query("*"))).empty(),"Full dictionary leading wildcard");
                (void)take(real->lookup(StrokeSequence(32,Stroke::wildcard)));
                (void)take(real->lookup(StrokeSequence(128,Stroke::wildcard)));
            }
            std::cout << "15 full-dictionary wildcard queries: "
                << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count() << " ms\n";
        }
        std::cout << "Wildcard, storage, layout and session checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
