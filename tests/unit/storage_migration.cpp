#include "layout_store.hpp"
#include "learning_store.hpp"
#include <stroke/dictionary/index.hpp>
#include <stroke/engine/session.hpp>
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace stroke;
using namespace stroke::win;
namespace fs = std::filesystem;
template <class T> T take(Result<T> value) {
    if (auto error = std::get_if<Error>(&value)) throw std::runtime_error(error->message);
    return std::get<T>(std::move(value));
}
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::string bytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}
int wmain(int argc, wchar_t** argv) {
    try {
        // Read-only CLI probe used for comparing an MSIX child and a native broker.
        if (argc == 2 && std::wstring_view(argv[1]) == L"--probe") {
            const auto layout = take(load_layout(layout_path()));
            const auto encoded = encode_layout(layout);
            std::wcout << layout_path().wstring() << L'\n' << std::wstring(encoded.begin(), encoded.end()) << L'\n';
            return 0;
        }
        PWSTR profile{};
        check(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, KF_FLAG_NO_PACKAGE_REDIRECTION,
                                           nullptr, &profile)), "Resolve profile");
        const fs::path expected = fs::path(profile) / L".jin-stroke-ime";
        CoTaskMemFree(profile);
        check(user_storage_directory() == expected && layout_path() == expected / L"layout.dat" &&
                  learning_path() == expected / L"learning.dat", "Preferences and learning share a profile directory");
        // Create only a separate test directory, never the user's real preferences.
        const auto root = expected.parent_path() / (L".jin-stroke-ime-test-" + std::to_wstring(GetCurrentProcessId()));
        fs::create_directories(root);
        const auto legacy = root / L"legacy", shared = root / L"shared";
        Layout traditional;
        traditional.mode = LayoutMode::traditional;
        traditional.reverse_selection = true;
        traditional.association_input = true;
        take(save_layout(legacy / L"layout.dat", traditional));
        auto state = take(sync_learning(legacy / L"learning.dat"));
        state = take(sync_learning(legacy / L"learning.dat", state.generation,
                                  {{U'遠', {7, 42}}}, {{U"遠方", {4, 42, false}}}));
        state = take(add_personal_phrase(legacy / L"learning.dat", U"錦筆"));
        const auto original_layout = bytes(legacy / L"layout.dat");
        const auto original_learning = bytes(legacy / L"learning.dat");
        take(migrate_user_storage(legacy, shared));
        check(bytes(shared / L"layout.dat") == original_layout &&
                  bytes(shared / L"learning.dat") == original_learning,
              "Migration preserves exact settings and learning bytes");
        check(bytes(legacy / L"layout.dat") == original_layout &&
                  bytes(legacy / L"learning.dat") == original_learning, "Original files retained");
        check(take(load_layout(shared / L"layout.dat")) == traditional,
              "Traditional layout, reverse order and associations preserved");
        const auto learned = take(sync_learning(shared / L"learning.dat"));
        check(learned.generation == state.generation && learned.uses == state.uses && learned.phrases == state.phrases,
              "Learning generation, counts and personal words preserved");
        const auto dictionary = take(IndexedDictionary::decode(take(encode_index({{"12121", U'正', 0}},
                                        {index_format_version, "test", "fixture"}, "Migration test"))));
        Session session;
        take(session.configure(dictionary, layout_config(take(load_layout(shared / L"layout.dat")))));
        for (char key : std::string_view("qwqw")) take(session.process_key(key));
        check(session.snapshot().strokes == StrokeSequence{Stroke::horizontal, Stroke::vertical,
                  Stroke::horizontal, Stroke::vertical} && session.snapshot().total_candidates == 1,
              "Saved traditional QWQW uses horizontal/vertical and produces candidates");
        take(save_layout(shared / L"layout.dat", Layout{}));
        take(clear_learning(shared / L"learning.dat"));
        const auto changed_learning = bytes(shared / L"learning.dat");
        take(migrate_user_storage(legacy, shared));
        check(take(load_layout(shared / L"layout.dat")) == Layout{} &&
                  bytes(shared / L"learning.dat") == changed_learning,
              "Repeated migration cannot overwrite new settings or resurrect cleared learning");
        const auto empty = root / L"empty";
        take(migrate_user_storage(root / L"missing", empty));
        check(!fs::exists(empty), "Absent legacy data does not manufacture defaults");
        check(std::holds_alternative<Error>(migrate_user_storage({}, shared)), "Reject invalid migration roots");
        const auto blocked = root / L"blocked";
        fs::create_directories(blocked / L"layout.dat");
        check(std::holds_alternative<Error>(migrate_user_storage(legacy, blocked)) &&
                  !fs::exists(blocked / L"learning.dat"), "Migration failure preserves original files");
        // The observed final handle path proves that this host did not use a package overlay.
        const auto probe = shared / L"layout.dat";
        HANDLE file = CreateFileW(probe.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        check(file != INVALID_HANDLE_VALUE, "Open shared probe");
        wchar_t actual[32768]{};
        const auto length = GetFinalPathNameByHandleW(file, actual, 32768, FILE_NAME_NORMALIZED);
        CloseHandle(file);
        check(length && std::wstring(actual) == L"\\\\?\\" + probe.wstring(),
              "Shared files are not redirected to host package storage");
        PWSTR local{};
        check(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_NO_PACKAGE_REDIRECTION,
                                           nullptr, &local)), "Resolve legacy test location");
        const auto overlay = fs::path(local) / (L"StrokeIME-migration-test-" + std::to_wstring(GetCurrentProcessId()));
        CoTaskMemFree(local);
        const auto overlay_file = overlay / L"layout.dat";
        take(save_layout(overlay_file, traditional));
        file = CreateFileW(overlay_file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        check(file != INVALID_HANDLE_VALUE, "Open possible package overlay");
        const auto overlay_length = GetFinalPathNameByHandleW(file, actual, 32768, FILE_NAME_NORMALIZED);
        CloseHandle(file);
        check(overlay_length != 0, "Resolve possible package overlay");
        const bool redirected = std::wstring(actual) != L"\\\\?\\" + overlay_file.wstring();
        const auto overlay_destination = root / L"overlay-destination";
        take(migrate_user_storage(overlay, overlay_destination));
        check(redirected ? !fs::exists(overlay_destination / L"layout.dat")
                         : take(load_layout(overlay_destination / L"layout.dat")) == traditional,
              "Private overlays cannot seed the shared layout; ordinary legacy files can");
        fs::remove_all(overlay);
        fs::remove_all(root);
        std::cout << "Shared storage and migration checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
