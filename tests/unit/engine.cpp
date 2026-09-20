#include "stroke/engine/session.hpp"
#include "stroke/dictionary/index.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
int failures{};
void check(bool condition, const char* label) {
    if (!condition) { std::cerr << "FAIL: " << label << '\n'; ++failures; }
}
template <typename T> bool failed(const stroke::Result<T>& result) { return std::holds_alternative<stroke::Error>(result); }
template <typename T> T take(stroke::Result<T> result) {
    if (const auto* error = std::get_if<stroke::Error>(&result)) { throw std::runtime_error(error->message); }
    return std::get<T>(std::move(result));
}
class ControlledDictionary final : public stroke::IDictionary {
public:
    std::vector<stroke::Candidate> matches;
    bool fail{};
    mutable std::size_t calls{};
    stroke::DictionaryInfo info() const override { return {1, "fixture", "test"}; }
    stroke::Result<std::vector<stroke::Candidate>> lookup(std::span<const stroke::Stroke>) const override {
        ++calls;
        if (fail) { return stroke::Error{stroke::ErrorCode::io_error, "Test backend failure"}; }
        return matches;
    }
};
void behavioral_tests() {
    using namespace stroke;
    const auto dictionary = take(IndexedDictionary::decode(take(encode_index({
        {"1", U'一', 100}, {"12", U'丁', 20}, {"1234", U'木', 10},
        {"121", U'土', 20}, {"121251", U'𠮷', 5}, {"12341", U'木', 10}},
        {index_format_version, "test", "fixture"}, "Synthetic engine fixture"))));
    Config config;
    config.page_size = 2;
    config.bindings = {{'w', Stroke::horizontal}, {'u', Stroke::horizontal}};
    Session session;
    check(failed(session.process(AppendStroke{Stroke::horizontal})), "Cannot query unconfigured session");
    take(session.configure(dictionary, config));
    check(session.handles_key('w') && !session.handles_key('q'), "Read-only key handling probe");
    const auto idle = session.snapshot().revision;
    check(!take(session.process_key('q')).consumed && session.snapshot().revision == idle, "Unbound key passes through unchanged");
    check(!take(session.process(Backspace{})).consumed, "Idle backspace belongs to host");
    check(!take(session.process(Cancel{})).consumed, "Idle cancel belongs to host");
    check(!take(session.process(ChangePage{PageDirection::next})).consumed, "Idle navigation belongs to host");
    auto first = take(session.process_key('w'));
    check(first.snapshot.total_candidates == 5 && first.snapshot.visible_candidates.size() == 2, "Prefix results deduplicate and page");
    check(first.snapshot.visible_candidates[0].character == U'一' && first.snapshot.visible_candidates[1].character == U'丁', "Exact before descending score");
    check(first.snapshot.has_next_page && first.snapshot.page_index == 0, "First page state");
    check(failed(session.configure(dictionary, config)), "Configuration held stable during composition");
    auto second = take(session.process(ChangePage{PageDirection::next}));
    check(second.snapshot.visible_candidates[0].character == U'土' && second.snapshot.visible_candidates[1].character == U'木', "Descending score then codepoint order");
    check(failed(session.process(SelectCandidate{first.snapshot.revision, 0})), "Old page selection rejected");
    auto last = take(session.process(ChangePage{PageDirection::next}));
    check(last.snapshot.visible_candidates.size() == 1 && !last.snapshot.has_next_page, "Partial final page");
    check(take(session.process(ChangePage{PageDirection::next})).snapshot.revision == last.snapshot.revision, "Boundary navigation is consumed without mutation");
    check(take(session.process(ChangePage{PageDirection::previous})).snapshot.page_index == 1, "Previous page");
    auto refined = take(session.process(AppendStroke{Stroke::vertical}));
    check(refined.snapshot.page_index == 0 && refined.snapshot.visible_candidates[0].character == U'丁', "New stroke resets pagination and updates exact match");
    check(take(session.process(Backspace{})).snapshot.strokes.size() == 1, "Backspace reruns shorter query");
    take(session.process(Backspace{}));
    check(session.snapshot().phase == SessionPhase::idle && session.snapshot().total_candidates == 0, "Deleting last stroke clears all state");

    for (const char c : std::string("121251")) { take(session.process(AppendStroke{static_cast<Stroke>(c - '0')})); }
    auto view = session.snapshot();
    check(view.visible_candidates.size() == 1 && view.visible_candidates[0].character == U'𠮷', "Supplementary-plane candidate");
    auto pending = take(session.process(SelectCandidate{view.revision, 0}));
    check(pending.commit && pending.commit->text == U"𠮷" && pending.snapshot.phase == SessionPhase::pending_commit, "Selection requests a commit without clearing input");
    check(pending.commit->revision == pending.snapshot.revision, "Commit token matches pending revision");
    check(failed(session.process(Backspace{})), "Input frozen while commit is pending");
    check(failed(session.complete_commit(view.revision, true)), "Wrong commit acknowledgement rejected");
    auto restored = take(session.complete_commit(pending.commit->revision, false));
    check(restored.snapshot.phase == SessionPhase::composing && restored.snapshot.strokes == view.strokes &&
          restored.snapshot.visible_candidates == view.visible_candidates, "Failed host write preserves composition");
    check(!restored.commit && restored.snapshot.revision != view.revision, "Failed commit cannot reuse stale selection");
    check(failed(session.complete_commit(pending.commit->revision, true)), "Duplicate acknowledgement rejected");
    pending = take(session.process(SelectCandidate{restored.snapshot.revision, 0}));
    const auto commit_revision = pending.commit->revision;
    check(take(session.complete_commit(commit_revision, true)).snapshot.phase == SessionPhase::idle, "Only successful host write clears composition");
    check(failed(session.complete_commit(commit_revision, true)), "Success cannot be acknowledged twice");

    take(session.process_key('u'));
    view = session.snapshot();
    pending = take(session.process(SelectCandidate{view.revision, 0}));
    take(session.process(Cancel{}));
    check(failed(session.complete_commit(pending.commit->revision, true)), "Cancel invalidates pending callback");
    take(session.process(AppendStroke{Stroke::turn}));
    check(session.snapshot().total_candidates == 0 && session.snapshot().phase == SessionPhase::composing, "No matches remain editable");
    check(failed(session.process(SelectCandidate{session.snapshot().revision, 0})), "Cannot select empty results");
    take(session.process(Backspace{}));
    take(session.process_key('w'));
    auto invalid = session.snapshot();
    check(failed(session.process(AppendStroke{static_cast<Stroke>(0)})) && session.snapshot().revision == invalid.revision, "Invalid stroke leaves state intact");
    check(failed(session.process(ChangePage{static_cast<PageDirection>(9)})), "Invalid direction rejected");
    check(failed(session.process(SelectCandidate{session.snapshot().revision, 99})), "Out-of-range candidate rejected");
    session.reset();
    Config bad = config; bad.page_size = 0;
    check(failed(session.configure(dictionary, bad)), "Invalid new configuration rejected");
    check(session.handles_key('w'), "Failed configuration leaves prior key map intact");
    bad = config; bad.learning_enabled = true;
    check(!failed(session.configure(dictionary, bad)), "Learning can be enabled");
    check(failed(session.configure(nullptr, config)), "Null backend rejected");

    Session isolated;
    take(isolated.configure(dictionary, config));
    take(session.process_key('w'));
    check(isolated.snapshot().strokes.empty(), "Two contexts do not share state");
    auto detached = session.snapshot(); detached.visible_candidates.clear();
    check(!session.snapshot().visible_candidates.empty(), "UI snapshot is detached");
}
void backend_tests() {
    using namespace stroke;
    auto backend = std::make_shared<ControlledDictionary>();
    backend->matches = {{U'木', 50, false}, {U'一', 30, true}, {U'木', 2, true}, {U'丁', 30, true}};
    Session session;
    take(session.configure(backend, Config{}));
    take(session.process(AppendStroke{Stroke::horizontal}));
    auto before = session.snapshot();
    check(before.total_candidates == 3 && before.visible_candidates[0].character == U'木', "Duplicate backend entries merge rank and exact flags");
    check(before.visible_candidates[0].frequency_score == 50, "Duplicate character keeps highest score");
    std::reverse(backend->matches.begin(), backend->matches.end());
    session.reset();
    take(session.process(AppendStroke{Stroke::horizontal}));
    check(session.snapshot().visible_candidates == before.visible_candidates, "Backend order cannot alter candidate order");
    before = session.snapshot();
    backend->fail = true;
    check(failed(session.process(AppendStroke{Stroke::vertical})), "Backend failure surfaced");
    check(session.snapshot().revision == before.revision && session.snapshot().strokes == before.strokes &&
          session.snapshot().visible_candidates == before.visible_candidates, "Backend failure preserves all visible state");
    const auto calls = backend->calls;
    take(session.process(Backspace{}));
    check(session.snapshot().phase == SessionPhase::idle && backend->calls == calls, "Can clear final stroke even when backend fails");
    backend->fail = false;
    backend->matches = {{U'一', 0, true}, {U'丁', 0, false}, {U'木', 0xFFFFFFFFU, false},
                        {U'土', 20, false}};
    take(session.process(AppendStroke{Stroke::horizontal}));
    const auto boundary = session.snapshot().visible_candidates;
    check(boundary.size() == 4 && boundary[0].character == U'一' &&
          boundary[1].character == U'木' && boundary[2].character == U'土' &&
          boundary[3].character == U'丁', "Exact priority retained; descending scores include zero and UINT32_MAX");
    session.reset();
    backend->matches = {{static_cast<char32_t>(0xD800), 0, true}};
    check(failed(session.process(AppendStroke{Stroke::horizontal})) && session.snapshot().phase == SessionPhase::idle, "Malformed backend Unicode rejected before publication");
    backend->matches.clear();
    for (std::size_t i = 0; i < max_stroke_sequence_length; ++i) { take(session.process(AppendStroke{Stroke::horizontal})); }
    before = session.snapshot();
    check(failed(session.process(AppendStroke{Stroke::horizontal})) && session.snapshot().revision == before.revision, "Input length capped without mutation");
    take(session.process(Backspace{}));
    check(session.snapshot().strokes.size() == max_stroke_sequence_length - 1, "Can recover after reaching input limit");
}
} // namespace

int main() {
    try { behavioral_tests(); backend_tests(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    if (failures == 0) { std::cout << "Engine behavior and failure paths passed\n"; }
    return failures == 0 ? 0 : 1;
}
