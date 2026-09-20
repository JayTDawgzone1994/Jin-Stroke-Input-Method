#include "stroke/engine/session.hpp"
#include "stroke/dictionary/index.hpp"
#include "stroke/dictionary/text.hpp"

#include <charconv>
#include <iostream>
#include <stdexcept>

namespace {
template <typename T> T take(stroke::Result<T> result) {
    if (const auto* error = std::get_if<stroke::Error>(&result)) { throw std::runtime_error(error->message); }
    return std::get<T>(std::move(result));
}
void show(const stroke::SessionSnapshot& snapshot) {
    const char* phase = snapshot.phase == stroke::SessionPhase::idle ? "idle" :
        snapshot.phase == stroke::SessionPhase::composing ? "composing" : "pending";
    std::cout << "STATE " << phase << " revision=" << snapshot.revision << " strokes=";
    for (const auto value : snapshot.strokes) {
        if (value == stroke::Stroke::wildcard) std::cout << '*';
        else std::cout << static_cast<unsigned>(value);
    }
    std::cout << " total=" << snapshot.total_candidates << " page=" << snapshot.page_index + 1
              << " more=" << snapshot.has_next_page << '\n';
    for (std::size_t i = 0; i < snapshot.visible_candidates.size(); ++i) {
        const auto& candidate = snapshot.visible_candidates[i];
        std::cout << i + 1 << ". " << stroke::encode_utf8(candidate.character)
                  << (candidate.exact_match ? " [exact]" : " [prefix]") << '\n';
    }
}
int run(const std::filesystem::path& index) {
    using namespace stroke;
    try {
        Session session;
        take(session.configure(take(IndexedDictionary::load(index)), Config{}));
        std::optional<CommitRequest> pending;
        std::cout << "Engine console; host writes are SIMULATED, not sent to Windows.\n"
                     "Commands: digits 1..5 and * wildcard, back, cancel, next, prev, pick N, ok, fail, show, quit\n";
        show(session.snapshot());
        std::string line;
        while (std::getline(std::cin, line)) {
            if (!line.empty() && line.back() == '\r') { line.pop_back(); }
            if (line == "quit") { break; }
            if (line.empty()) { continue; }
            try {
                Update update;
                if (line == "show") { show(session.snapshot()); continue; }
                if (line == "ok" || line == "fail") {
                    if (!pending) { throw std::runtime_error("No pending commit"); }
                    const auto request = *pending;
                    update = take(session.complete_commit(request.revision, line == "ok"));
                    if (line == "ok") {
                        std::cout << "SIMULATED_COMMIT ";
                        for (const auto cp : request.text) { std::cout << encode_utf8(cp); }
                        std::cout << '\n';
                    } else { std::cout << "SIMULATED_FAILURE composition preserved\n"; }
                } else if (line == "back") { update = take(session.process(Backspace{})); }
                else if (line == "cancel") { update = take(session.process(Cancel{})); }
                else if (line == "next") { update = take(session.process(ChangePage{PageDirection::next})); }
                else if (line == "prev") { update = take(session.process(ChangePage{PageDirection::previous})); }
                else if (line.starts_with("pick ")) {
                    std::size_t number{};
                    const auto parsed = std::from_chars(line.data() + 5, line.data() + line.size(), number);
                    if (parsed.ec != std::errc{} || parsed.ptr != line.data() + line.size() || number == 0) {
                        throw std::runtime_error("pick requires a positive visible candidate number");
                    }
                    update = take(session.process(SelectCandidate{session.snapshot().revision, number - 1}));
                } else {
                    if (line.find_first_not_of("12345*") != std::string::npos) { throw std::runtime_error("Unknown command"); }
                    for (const char c : line) { update = take(session.process(AppendStroke{c == '*' ? Stroke::wildcard : static_cast<Stroke>(c - '0')})); }
                }
                if (update.commit) { pending = update.commit; std::cout << "COMMIT_REQUEST\n"; }
                else if (update.snapshot.phase != SessionPhase::pending_commit) { pending.reset(); }
                show(update.snapshot);
            } catch (const std::exception& error) { std::cout << "ERROR " << error.what() << '\n'; show(session.snapshot()); }
        }
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
}
} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[]) {
    if (argc != 2) { std::cerr << "Usage: stroke_engine_console INDEX_FILE\n"; return 2; }
    return run(std::filesystem::path(argv[1]));
}
#else
int main(int argc, char* argv[]) {
    if (argc != 2) { std::cerr << "Usage: stroke_engine_console INDEX_FILE\n"; return 2; }
    return run(std::filesystem::path(argv[1]));
}
#endif
