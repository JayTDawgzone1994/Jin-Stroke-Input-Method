#include "learning_store.hpp"
#include "layout_store.hpp"
#include <windows.h>
#include <objbase.h>
#include <algorithm>
#include <atomic>
#include <charconv>
#include <sstream>

namespace stroke::win {
namespace {
constexpr std::uint32_t max_count = 1000000;
constexpr std::size_t max_entries = 100000, max_bytes = 4 * 1024 * 1024;
struct Handle {
    HANDLE value{INVALID_HANDLE_VALUE};
    ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
Error unavailable(const char* text) { return {ErrorCode::unavailable, text}; }
template<class T> bool number(const std::string& text, T& value) {
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}
std::string new_generation() {
    GUID id{};
    if (FAILED(CoCreateGuid(&id))) return {};
    wchar_t text[40]{};
    if (!StringFromGUID2(id, text, 40)) return {};
    std::string result;
    for (const wchar_t* p = text; *p; ++p) result += static_cast<char>(*p);
    return result;
}
Result<LearningState> read(const std::filesystem::path& path) {
    Handle file{CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (file.value == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) return LearningState{};
        return unavailable("Cannot read learning data");
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.value, &size) || size.QuadPart <= 0 || size.QuadPart > max_bytes)
        return Error{ErrorCode::invalid_data, "Invalid learning file size"};
    std::string bytes(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD count{};
    if (!ReadFile(file.value, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr) || count != bytes.size())
        return unavailable("Cannot read learning data");
    std::istringstream input(bytes);
    LearningState state;
    std::string header, flag;
    if (!(input >> header >> state.generation >> flag) || header != "STROKE-LEARNING-1" ||
        state.generation.size() != 38 || state.generation.front() != '{' || state.generation.back() != '}' ||
        (flag != "0" && flag != "1")) return Error{ErrorCode::invalid_data, "Invalid learning header"};
    state.enabled = flag == "1";
    std::string cp_text, count_text, time_text;
    while (input >> cp_text) {
        std::uint32_t cp{}; LearnedUse use;
        if (!(input >> count_text >> time_text) || !number(cp_text, cp) || !number(count_text, use.count) ||
            !number(time_text, use.last_used) || !is_unicode_scalar(static_cast<char32_t>(cp)) || cp == 0 ||
            !use.count || use.count > max_count || !state.uses.emplace(static_cast<char32_t>(cp), use).second ||
            state.uses.size() > max_entries) return Error{ErrorCode::invalid_data, "Invalid learning entry"};
    }
    return state;
}
Status save(const std::filesystem::path& path, const LearningState& state) {
    std::string bytes = "STROKE-LEARNING-1 " + state.generation + (state.enabled ? " 1\n" : " 0\n");
    for (const auto& [cp, use] : state.uses)
        bytes += std::to_string(static_cast<std::uint32_t>(cp)) + ' ' + std::to_string(use.count) + ' ' +
            std::to_string(use.last_used) + '\n';
    if (bytes.size() > max_bytes) return unavailable("Learning data limit exceeded");
    static std::atomic<unsigned> serial{};
    auto temporary = path;
    temporary += L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++serial);
    bool written = false;
    {
        Handle file{CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
        if (file.value == INVALID_HANDLE_VALUE) return unavailable("Cannot create learning temporary file");
        DWORD count{};
        written = WriteFile(file.value, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr) &&
            count == bytes.size() && FlushFileBuffers(file.value);
    }
    if (!written || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        return unavailable("Cannot replace learning data");
    }
    return std::monostate{};
}
enum class Operation { sync, enable, disable, clear };
Result<LearningState> transaction(const std::filesystem::path& path, Operation operation,
                                 const std::string& generation, const LearnedUses& pending) {
    if (path.empty()) return unavailable("Learning path unavailable");
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return unavailable("Cannot create learning directory");
    auto lock_path = path; lock_path += L".lock";
    Handle lock{CreateFileW(lock_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (lock.value == INVALID_HANDLE_VALUE) return unavailable("Learning store busy or inaccessible");
    auto loaded = read(path);
    // Explicit clear can repair a corrupt file; normal sync never overwrites corruption.
    if (std::holds_alternative<Error>(loaded) && operation != Operation::clear) return std::get<Error>(loaded);
    LearningState state = std::holds_alternative<LearningState>(loaded) ?
        std::get<LearningState>(std::move(loaded)) : LearningState{};
    bool changed = state.generation.empty();
    if (changed) state.generation = new_generation();
    if (operation == Operation::clear ||
        (operation == Operation::enable && !state.enabled) || (operation == Operation::disable && state.enabled)) {
        state.generation = new_generation();
        if (operation == Operation::clear) state.uses.clear();
        else state.enabled = operation == Operation::enable;
        changed = true;
    }
    if (state.generation.empty()) return unavailable("Cannot create learning generation");
    if (operation == Operation::sync && state.enabled && generation == state.generation) {
        for (const auto& [cp, increment] : pending) {
            if (!is_unicode_scalar(cp) || cp == 0 || !increment.count)
                return Error{ErrorCode::invalid_argument, "Invalid learning increment"};
            if (!state.uses.contains(cp) && state.uses.size() >= max_entries) continue;
            auto& use = state.uses[cp];
            use.count = static_cast<std::uint32_t>(std::min<std::uint64_t>(max_count,
                static_cast<std::uint64_t>(use.count) + increment.count));
            use.last_used = std::max(use.last_used, increment.last_used);
            changed = true;
        }
    }
    if (changed) {
        const auto saved = save(path, state);
        if (const auto* error = std::get_if<Error>(&saved)) return *error;
    }
    return state;
}
}
std::filesystem::path learning_path() {
    const auto path = layout_path();
    return path.empty() ? std::filesystem::path{} : path.parent_path() / L"learning.dat";
}
Result<LearningState> sync_learning(const std::filesystem::path& path, const std::string& generation, const LearnedUses& pending) {
    return transaction(path, Operation::sync, generation, pending);
}
Result<LearningState> set_learning_enabled(const std::filesystem::path& path, bool enabled) {
    return transaction(path, enabled ? Operation::enable : Operation::disable, {}, {});
}
Result<LearningState> clear_learning(const std::filesystem::path& path) {
    return transaction(path, Operation::clear, {}, {});
}
}
