#include "layout_store.hpp"
#include <windows.h>
#include <shlobj.h>
#include <atomic>
#include <array>
#include <algorithm>

namespace stroke::win {
namespace {
std::filesystem::path known_folder(REFKNOWNFOLDERID folder) {
    PWSTR directory{};
    if (FAILED(SHGetKnownFolderPath(folder, KF_FLAG_NO_PACKAGE_REDIRECTION, nullptr, &directory))) return {};
    std::filesystem::path result;
    try { result = std::filesystem::path(directory); }
    catch (...) { CoTaskMemFree(directory); throw; }
    CoTaskMemFree(directory);
    return result;
}
struct File {
    HANDLE value{INVALID_HANDLE_VALUE};
    ~File() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
Status migrate_file(const std::filesystem::path& source, const std::filesystem::path& target) {
    const auto attributes = GetFileAttributesW(target.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES)
        return attributes & FILE_ATTRIBUTE_DIRECTORY
            ? Status{Error{ErrorCode::unavailable, "Settings destination is a directory"}}
            : Status{std::monostate{}};
    const auto target_error = GetLastError();
    if (target_error != ERROR_FILE_NOT_FOUND && target_error != ERROR_PATH_NOT_FOUND)
        return Error{ErrorCode::unavailable, "Cannot inspect shared settings"};
    File input{CreateFileW(source.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (input.value == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return std::monostate{};
        return Error{ErrorCode::unavailable, "Cannot read previous settings"};
    }
    // A packaged host may see an obsolete private overlay at the same apparent path.
    // Only the ordinary desktop's original file may seed shared settings. Keep every
    // original file (including private overlays) intact; never merge divergent epochs.
    std::array<wchar_t, 32768> final{}, absolute{};
    const auto final_count = GetFinalPathNameByHandleW(input.value, final.data(),
                                                      static_cast<DWORD>(final.size()), FILE_NAME_NORMALIZED);
    const auto absolute_count = GetFullPathNameW(source.c_str(), static_cast<DWORD>(absolute.size()),
                                                 absolute.data(), nullptr);
    if (!final_count || final_count >= final.size() || !absolute_count || absolute_count >= absolute.size())
        return Error{ErrorCode::unavailable, "Cannot resolve previous settings"};
    std::wstring_view actual(final.data(), final_count);
    if (actual.starts_with(L"\\\\?\\")) actual.remove_prefix(4);
    if (_wcsicmp(std::wstring(actual).c_str(), absolute.data()) != 0) return std::monostate{};
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(input.value, &size) || size.QuadPart <= 0 || size.QuadPart > 8 * 1024 * 1024)
        return Error{ErrorCode::invalid_data, "Invalid previous settings size"};
    std::error_code ec;
    std::filesystem::create_directories(target.parent_path(), ec);
    if (ec) return Error{ErrorCode::unavailable, "Cannot create shared settings directory"};
    static std::atomic<unsigned> serial{};
    auto temporary = target;
    temporary += L".migrate-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++serial);
    bool copied = true;
    {
        File output{CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
        if (output.value == INVALID_HANDLE_VALUE) return Error{ErrorCode::unavailable, "Cannot migrate settings"};
        std::array<char, 65536> bytes{};
        LONGLONG total{};
        while (total < size.QuadPart) {
            DWORD read{}, written{};
            const auto wanted = static_cast<DWORD>(std::min<LONGLONG>(static_cast<LONGLONG>(bytes.size()), size.QuadPart - total));
            if (!ReadFile(input.value, bytes.data(), wanted, &read, nullptr) || read != wanted ||
                !WriteFile(output.value, bytes.data(), read, &written, nullptr) || written != read) {
                copied = false; break;
            }
            total += read;
        }
        if (copied) copied = FlushFileBuffers(output.value) != FALSE;
    }
    // No REPLACE_EXISTING: another process or a user's new settings always win.
    if (copied && MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH)) return std::monostate{};
    const auto error = GetLastError();
    DeleteFileW(temporary.c_str());
    if (copied && (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS)) return std::monostate{};
    return Error{ErrorCode::unavailable, "Cannot migrate settings"};
}
} // namespace
std::filesystem::path user_storage_directory() {
    // NO_PACKAGE_REDIRECTION only controls known-folder lookup; it cannot disable
    // MSIX file overlays under AppData (including LocalLow). Keep this IME's
    // cross-application state outside AppData and inside the current user's profile.
    const auto profile = known_folder(FOLDERID_Profile);
    return profile.empty() ? profile : profile / L".jin-stroke-ime";
}
std::filesystem::path layout_path() {
    const auto directory = user_storage_directory();
    return directory.empty() ? directory : directory / L"layout.dat";
}
Status migrate_user_storage(const std::filesystem::path& legacy, const std::filesystem::path& shared) {
    if (legacy.empty() || shared.empty() || legacy == shared)
        return Error{ErrorCode::invalid_argument, "Invalid settings migration paths"};
    for (const auto* name : {L"layout.dat", L"learning.dat"}) {
        const auto result = migrate_file(legacy / name, shared / name);
        if (auto error = std::get_if<Error>(&result)) return *error;
    }
    return std::monostate{};
}
Status migrate_user_storage() {
    const auto legacy = known_folder(FOLDERID_LocalAppData);
    return migrate_user_storage(legacy.empty() ? legacy : legacy / L"StrokeIME", user_storage_directory());
}
Result<Layout> load_layout(const std::filesystem::path& path) {
    if (path.empty()) return Error{ErrorCode::unavailable, "Settings path unavailable"};
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return Layout{};
        return Error{ErrorCode::unavailable, "Cannot read settings"};
    }
    char bytes[33]{}; DWORD read{};
    const BOOL success = ReadFile(file, bytes, sizeof(bytes), &read, nullptr);
    CloseHandle(file);
    if (!success) return Error{ErrorCode::unavailable, "Cannot read settings"};
    return decode_layout(std::string_view(bytes, read));
}
Status save_layout(const std::filesystem::path& path, const Layout& layout) {
    if (path.empty() || !valid_layout(layout)) return Error{ErrorCode::invalid_argument, "Invalid settings"};
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return Error{ErrorCode::unavailable, "Cannot create settings directory"};
    static std::atomic<unsigned> serial{};
    auto temporary = path;
    temporary += L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++serial);
    const auto text = encode_layout(layout);
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return Error{ErrorCode::unavailable, "Cannot create settings"};
    DWORD written{};
    const bool success = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr)
        && written == text.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if (!success || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        return Error{ErrorCode::unavailable, "Cannot save settings"};
    }
    return std::monostate{};
}
}
