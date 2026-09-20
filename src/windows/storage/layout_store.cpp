#include "layout_store.hpp"
#include <windows.h>
#include <shlobj.h>
#include <atomic>

namespace stroke::win {
std::filesystem::path layout_path() {
    PWSTR directory{};
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &directory))) return {};
    std::filesystem::path result;
    try { result = std::filesystem::path(directory) / L"StrokeIME" / L"layout.dat"; }
    catch (...) { CoTaskMemFree(directory); throw; }
    CoTaskMemFree(directory);
    return result;
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
    char bytes[31]{}; DWORD read{};
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
