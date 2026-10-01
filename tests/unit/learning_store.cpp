#include "learning_store.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <windows.h>
using namespace stroke;
using namespace stroke::win;
template <class T> T take(Result<T> value) {
    if (const auto* error = std::get_if<Error>(&value))
        throw std::runtime_error(error->message);
    return std::get<T>(std::move(value));
}
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
LearningState retry(const std::filesystem::path& path, const std::string& generation = {},
                    const LearnedUses& uses = {}, const PersonalPhrases& phrases = {}) {
    for (int i = 0; i < 500; ++i) {
        auto value = sync_learning(path, generation, uses, phrases);
        if (auto* state = std::get_if<LearningState>(&value))
            return *state;
        Sleep(5);
    }
    throw std::runtime_error("Learning store remained busy");
}
int wmain(int argc, wchar_t** argv) {
    namespace fs = std::filesystem;
    fs::path folder;
    try {
        if (argc == 3 && std::wstring_view(argv[1]) == L"--worker") {
            const fs::path path(argv[2]);
            const auto generation = retry(path).generation;
            for (int i = 0; i < 20; ++i)
                (void)retry(path, generation, {{U'木', {1, 100}}}, {{U"木木", {1, 100, false}}});
            return 0;
        }
        folder = fs::temp_directory_path() /
                 ("stroke-learning-" + std::to_string(GetCurrentProcessId()) + "-" +
                  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        const auto path = folder / "learning.dat";
        auto state = take(sync_learning(path));
        check(state.enabled && !state.generation.empty() && state.uses.empty(),
              "Persist default enabled state");
        auto another = take(sync_learning(path));
        check(another.generation == state.generation, "Processes share persisted generation");
        take(sync_learning(path, state.generation, {{U'木', {2, 1}}}));
        state = take(sync_learning(path, another.generation, {{U'木', {3, 2}}}));
        check(state.uses.at(U'木').count == 5 && state.uses.at(U'木').last_used == 2,
              "Merge increments, not snapshots");
        const auto old = state.generation;
        auto disabled = take(set_learning_enabled(path, false));
        check(!disabled.enabled && disabled.uses.at(U'木').count == 5 && disabled.generation != old,
              "Disable preserves history and invalidates pending writes");
        take(sync_learning(path, old, {{U'木', {20, 3}}}));
        state = take(set_learning_enabled(path, true));
        check(state.uses.at(U'木').count == 5, "Disabled writes not counted");
        const auto previous = state.generation;
        state = take(clear_learning(path));
        check(state.enabled && state.uses.empty(), "Clear preserves enabled setting");
        state = take(sync_learning(path, previous, {{U'木', {9, 3}}}));
        check(state.uses.empty(), "Obsolete pending data cannot resurrect cleared history");
        wchar_t exe[32768]{};
        GetModuleFileNameW(nullptr, exe, 32768);
        PROCESS_INFORMATION children[2]{};
        for (auto& child : children) {
            std::wstring command =
                L"\"" + std::wstring(exe) + L"\" --worker \"" + path.wstring() + L"\"";
            STARTUPINFOW start{};
            start.cb = sizeof(start);
            check(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                                 nullptr, nullptr, &start, &child) != FALSE,
                  "Spawn concurrent writer");
            CloseHandle(child.hThread);
        }
        for (auto& child : children) {
            const auto waited = WaitForSingleObject(child.hProcess, 15000);
            DWORD code{};
            GetExitCodeProcess(child.hProcess, &code);
            CloseHandle(child.hProcess);
            check(waited == WAIT_OBJECT_0 && code == 0, "Concurrent writer completed");
        }
        state = take(sync_learning(path));
        check(state.uses.at(U'木').count == 40, "Two processes preserve all 40 increments");
        check(state.phrases.at(U"木木").count == 40,
              "Concurrent contextual counts are merged without loss");
        state = take(add_personal_phrase(path, U"錦筆劃"));
        check(state.phrases.at(U"錦筆劃").manual && state.phrases.at(U"錦筆劃").count == 0,
              "Manual words persist without an observation");
        state = take(sync_learning(path, state.generation, {}, {{U"錦筆劃", {2, 110, false}}}));
        check(state.phrases.at(U"錦筆劃").count == 2 && state.phrases.at(U"錦筆劃").manual,
              "Learning preserves manual flag");
        const auto before_remove = state.generation;
        state = take(remove_personal_phrase(path, U"錦筆劃"));
        state = take(sync_learning(path, before_remove, {}, {{U"錦筆劃", {20, 120, false}}}));
        check(!state.phrases.contains(U"錦筆劃"), "Old process cannot resurrect removed word");
        check(std::holds_alternative<Error>(add_personal_phrase(path, U"你")) &&
                  std::holds_alternative<Error>(add_personal_phrase(path, U"名字A")),
              "Reject invalid manual words");
        auto lockpath = path;
        lockpath += L".lock";
        HANDLE lock = CreateFileW(lockpath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                  OPEN_EXISTING, 0, nullptr);
        check(lock != INVALID_HANDLE_VALUE, "Acquire test lock");
        const auto busy = sync_learning(path, state.generation, {{U'木', {1, 101}}});
        CloseHandle(lock);
        check(std::holds_alternative<Error>(busy), "Lock contention returns without blocking");
        HANDLE protect = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                     OPEN_EXISTING, 0, nullptr);
        check(protect != INVALID_HANDLE_VALUE, "Protect file against replacement");
        const auto failed_save = sync_learning(path, state.generation, {{U'木', {1, 101}}},
                                               {{U"木木", {1, 101, false}}});
        CloseHandle(protect);
        check(std::holds_alternative<Error>(failed_save),
              "Failed atomic replacement reports failure");
        check(take(sync_learning(path)).uses.at(U'木').count == 40,
              "Failed write preserves original data");
        check(take(sync_learning(path, state.generation, {{U'木', {1, 101}}},
                                 {{U"木木", {1, 101, false}}}))
                      .uses.at(U'木')
                      .count == 41,
              "Retry unacknowledged batch counts once");
        check(take(sync_learning(path)).phrases.at(U"木木").count == 41,
              "Atomic replacement retry also counts the phrase once");
        state = take(add_personal_phrase(path, U"𠮷錦"));
        const auto before_clear = state.generation;
        state = take(clear_learning(path));
        check(state.uses.empty() && !state.phrases.contains(U"木木") &&
                  state.phrases.at(U"𠮷錦").manual && state.phrases.at(U"𠮷錦").count == 0,
              "Clear deletes automatic learning but preserves manual supplementary word");
        state = take(sync_learning(path, before_clear, {}, {{U"木木", {9, 130, false}}}));
        check(!state.phrases.contains(U"木木"),
              "Clear generation also rejects stale phrase batches");
        {
            std::ofstream old_file(path, std::ios::binary);
            old_file << "STROKE-LEARNING-1 " << state.generation << " 0\n26408 7 123\n";
        }
        state = take(sync_learning(path));
        check(!state.enabled && state.uses.at(U'木').count == 7 && state.phrases.empty(),
              "Existing single-character history survives migration");
        state = take(set_learning_enabled(path, true));
        {
            std::ifstream migrated(path);
            std::string header;
            migrated >> header;
            check(header == "STROKE-LEARNING-2", "Changes persist the current format");
        }
        for (const auto& entry : {"W 20320,22909 1 0 2\n", "W 20320,65 3 0 0\n",
                                  "W 20320,22909 0 0 0\n", "W 20320,22909 1000001 0 0\n",
                                  "W 20320,22909 1 0 0 extra\n", "W 20320,22909 1 0 0"}) {
            {
                std::ofstream invalid(path, std::ios::binary);
                invalid << "STROKE-LEARNING-2 " << state.generation << " 1\n" << entry;
            }
            check(std::holds_alternative<Error>(sync_learning(path)),
                  "Malformed personal word data is rejected");
            state = take(clear_learning(path));
        }
        {
            std::ofstream broken(path, std::ios::binary);
            broken << "corrupt";
        }
        check(std::holds_alternative<Error>(sync_learning(path)),
              "Corruption not silently overwritten");
        check(take(clear_learning(path)).uses.empty(), "Explicit clear repairs corruption");
        fs::remove_all(folder);
        std::cout
            << "Learning persistence, process concurrency, clear epochs and failures passed\n";
        return 0;
    } catch (const std::exception& error) {
        if (!folder.empty()) {
            std::error_code ignored;
            fs::remove_all(folder, ignored);
        }
        std::cerr << error.what() << '\n';
        return 1;
    }
}
