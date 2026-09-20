#include "learning_store.hpp"
#include <windows.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace stroke;
using namespace stroke::win;
template<class T> T take(Result<T> value) {
    if (const auto* error = std::get_if<Error>(&value)) throw std::runtime_error(error->message);
    return std::get<T>(std::move(value));
}
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
LearningState retry(const std::filesystem::path& path, const std::string& generation = {}, const LearnedUses& uses = {}) {
    for (int i = 0; i < 500; ++i) {
        auto value = sync_learning(path, generation, uses);
        if (auto* state = std::get_if<LearningState>(&value)) return *state;
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
            for (int i = 0; i < 20; ++i) (void)retry(path, generation, {{U'木', {1, 100}}});
            return 0;
        }
        folder = fs::temp_directory_path() / ("stroke-learning-" + std::to_string(GetCurrentProcessId()) + "-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        const auto path = folder / "learning.dat";
        auto state = take(sync_learning(path));
        check(state.enabled && !state.generation.empty() && state.uses.empty(), "Persist default enabled state");
        auto another = take(sync_learning(path));
        check(another.generation == state.generation, "Processes share persisted generation");
        take(sync_learning(path, state.generation, {{U'木', {2, 1}}}));
        state = take(sync_learning(path, another.generation, {{U'木', {3, 2}}}));
        check(state.uses.at(U'木').count == 5 && state.uses.at(U'木').last_used == 2, "Merge increments, not snapshots");
        const auto old = state.generation;
        auto disabled = take(set_learning_enabled(path, false));
        check(!disabled.enabled && disabled.uses.at(U'木').count == 5 && disabled.generation != old, "Disable preserves history and invalidates pending writes");
        take(sync_learning(path, old, {{U'木', {20, 3}}}));
        state = take(set_learning_enabled(path, true));
        check(state.uses.at(U'木').count == 5, "Disabled writes not counted");
        const auto previous = state.generation;
        state = take(clear_learning(path));
        check(state.enabled && state.uses.empty(), "Clear preserves enabled setting");
        state = take(sync_learning(path, previous, {{U'木', {9, 3}}}));
        check(state.uses.empty(), "Obsolete pending data cannot resurrect cleared history");
        wchar_t exe[32768]{}; GetModuleFileNameW(nullptr, exe, 32768);
        PROCESS_INFORMATION children[2]{};
        for (auto& child : children) {
            std::wstring command = L"\"" + std::wstring(exe) + L"\" --worker \"" + path.wstring() + L"\"";
            STARTUPINFOW start{}; start.cb = sizeof(start);
            check(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                nullptr, nullptr, &start, &child) != FALSE, "Spawn concurrent writer");
            CloseHandle(child.hThread);
        }
        for (auto& child : children) {
            const auto waited = WaitForSingleObject(child.hProcess, 15000);
            DWORD code{}; GetExitCodeProcess(child.hProcess, &code); CloseHandle(child.hProcess);
            check(waited == WAIT_OBJECT_0 && code == 0, "Concurrent writer completed");
        }
        state = take(sync_learning(path));
        check(state.uses.at(U'木').count == 40, "Two processes preserve all 40 increments");
        auto lockpath = path; lockpath += L".lock";
        HANDLE lock = CreateFileW(lockpath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        check(lock != INVALID_HANDLE_VALUE, "Acquire test lock");
        const auto busy = sync_learning(path, state.generation, {{U'木', {1, 101}}});
        CloseHandle(lock);
        check(std::holds_alternative<Error>(busy), "Lock contention returns without blocking");
        HANDLE protect = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        check(protect != INVALID_HANDLE_VALUE, "Protect file against replacement");
        const auto failed_save = sync_learning(path, state.generation, {{U'木', {1, 101}}});
        CloseHandle(protect);
        check(std::holds_alternative<Error>(failed_save), "Failed atomic replacement reports failure");
        check(take(sync_learning(path)).uses.at(U'木').count == 40, "Failed write preserves original data");
        check(take(sync_learning(path, state.generation, {{U'木', {1, 101}}})).uses.at(U'木').count == 41,
              "Retry unacknowledged batch counts once");
        { std::ofstream broken(path, std::ios::binary); broken << "corrupt"; }
        check(std::holds_alternative<Error>(sync_learning(path)), "Corruption not silently overwritten");
        check(take(clear_learning(path)).uses.empty(), "Explicit clear repairs corruption");
        fs::remove_all(folder);
        std::cout << "Learning persistence, process concurrency, clear epochs and failures passed\n";
        return 0;
    } catch (const std::exception& error) {
        if (!folder.empty()) { std::error_code ignored; fs::remove_all(folder, ignored); }
        std::cerr << error.what() << '\n'; return 1;
    }
}
