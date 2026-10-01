#include "learning_store.hpp"
#include "layout_store.hpp"
#include <algorithm>
#include <atomic>
#include <charconv>
#include <objbase.h>
#include <sstream>
#include <tuple>
#include <windows.h>
namespace stroke::win {
namespace {
constexpr std::uint32_t max_count = 1000000;
constexpr std::size_t max_entries = 100000, max_bytes = 8 * 1024 * 1024;
struct Handle {
    HANDLE value{INVALID_HANDLE_VALUE};
    ~Handle() {
        if (value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
};
Error unavailable(const char* text) { return {ErrorCode::unavailable, text}; }
template <class T> bool number(std::string_view text, T& value) {
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}
std::string new_generation() {
    GUID id{};
    if (FAILED(CoCreateGuid(&id)))
        return {};
    wchar_t text[40]{};
    if (!StringFromGUID2(id, text, 40))
        return {};
    std::string result;
    for (const wchar_t* p = text; *p; ++p)
        result += static_cast<char>(*p);
    return result;
}
std::u32string word_from(std::string_view text) {
    std::u32string word;
    while (!text.empty()) {
        const auto comma = text.find(',');
        std::uint32_t cp{};
        if (!number(text.substr(0, comma), cp))
            return {};
        word += static_cast<char32_t>(cp);
        if (word.size() > 8)
            return {};
        if (comma == std::string_view::npos)
            break;
        text.remove_prefix(comma + 1);
        if (text.empty())
            return {};
    }
    return valid_personal_phrase(word) ? word : std::u32string{};
}
std::string word_bytes(std::u32string_view word) {
    std::string result;
    for (auto cp : word) {
        if (!result.empty())
            result += ',';
        result += std::to_string(static_cast<std::uint32_t>(cp));
    }
    return result;
}
Result<LearningState> read(const std::filesystem::path& path) {
    Handle file{CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (file.value == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
            return LearningState{};
        return unavailable("Cannot read learning data");
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.value, &size) || size.QuadPart <= 0 || size.QuadPart > max_bytes)
        return Error{ErrorCode::invalid_data, "Invalid learning file size"};
    std::string bytes(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD count{};
    if (!ReadFile(file.value, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr) ||
        count != bytes.size())
        return unavailable("Cannot read learning data");
    if (bytes.back() != '\n')
        return Error{ErrorCode::invalid_data, "Truncated learning data"};
    std::istringstream input(bytes);
    LearningState state;
    std::string header, flag, line, extra;
    if (!std::getline(input, line))
        return Error{ErrorCode::invalid_data, "Missing learning header"};
    std::istringstream fields(line);
    if (!(fields >> header >> state.generation >> flag) || (fields >> extra) ||
        (header != "STROKE-LEARNING-1" && header != "STROKE-LEARNING-2") ||
        state.generation.size() != 38 || state.generation.front() != '{' ||
        state.generation.back() != '}' || (flag != "0" && flag != "1"))
        return Error{ErrorCode::invalid_data, "Invalid learning header"};
    state.enabled = flag == "1";
    while (std::getline(input, line)) {
        if (line.empty())
            continue;
        std::istringstream row(line);
        std::string kind = "C", key, count_text, time_text, manual = "0";
        if (header == "STROKE-LEARNING-2" && !(row >> kind))
            return Error{ErrorCode::invalid_data, "Invalid record"};
        if (!(row >> key >> count_text >> time_text))
            return Error{ErrorCode::invalid_data, "Incomplete record"};
        if (kind == "W" && !(row >> manual))
            return Error{ErrorCode::invalid_data, "Missing word flag"};
        std::uint32_t uses{};
        std::uint64_t time{};
        if ((row >> extra) || !number(count_text, uses) || uses > max_count ||
            !number(time_text, time))
            return Error{ErrorCode::invalid_data, "Invalid learning count"};
        if (kind == "C") {
            std::uint32_t cp{};
            if (!number(key, cp) || !cp || !is_unicode_scalar(static_cast<char32_t>(cp)) || !uses ||
                !state.uses.emplace(static_cast<char32_t>(cp), LearnedUse{uses, time}).second ||
                state.uses.size() > max_entries)
                return Error{ErrorCode::invalid_data, "Invalid character record"};
        } else if (kind == "W") {
            auto word = word_from(key);
            if (word.empty() || (manual != "0" && manual != "1") || (!uses && manual != "1") ||
                !state.phrases.emplace(std::move(word), PersonalPhrase{uses, time, manual == "1"})
                     .second ||
                state.phrases.size() > personal_phrase_limit)
                return Error{ErrorCode::invalid_data, "Invalid word record"};
        } else
            return Error{ErrorCode::invalid_data, "Unknown learning record"};
    }
    return state;
}
Status save(const std::filesystem::path& path, const LearningState& state) {
    std::string bytes = "STROKE-LEARNING-2 " + state.generation + (state.enabled ? " 1\n" : " 0\n");
    for (const auto& [cp, use] : state.uses)
        bytes += "C " + std::to_string(static_cast<std::uint32_t>(cp)) + ' ' +
                 std::to_string(use.count) + ' ' + std::to_string(use.last_used) + '\n';
    for (const auto& [word, use] : state.phrases)
        bytes += "W " + word_bytes(word) + ' ' + std::to_string(use.count) + ' ' +
                 std::to_string(use.last_used) + (use.manual ? " 1\n" : " 0\n");
    if (bytes.size() > max_bytes)
        return unavailable("Learning data limit exceeded");
    static std::atomic<unsigned> serial{};
    auto temporary = path;
    temporary +=
        L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++serial);
    bool written = false;
    {
        Handle file{CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                FILE_ATTRIBUTE_NORMAL, nullptr)};
        if (file.value == INVALID_HANDLE_VALUE)
            return unavailable("Cannot create learning temporary file");
        DWORD count{};
        written = WriteFile(file.value, bytes.data(), static_cast<DWORD>(bytes.size()), &count,
                            nullptr) &&
                  count == bytes.size() && FlushFileBuffers(file.value);
    }
    if (!written || !MoveFileExW(temporary.c_str(), path.c_str(),
                                 MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        return unavailable("Cannot replace learning data");
    }
    return std::monostate{};
}
enum class Operation { sync, enable, disable, clear, add_word, remove_word };
Result<LearningState> transaction(const std::filesystem::path& path, Operation operation,
                                  const std::string& generation, const LearnedUses& pending,
                                  const PersonalPhrases& pending_phrases = {},
                                  std::u32string word = {}) {
    if (path.empty())
        return unavailable("Learning path unavailable");
    if ((operation == Operation::add_word || operation == Operation::remove_word) &&
        !valid_personal_phrase(word))
        return Error{ErrorCode::invalid_argument,
                     "Personal words must contain 2 to 8 Han characters"};
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
        return unavailable("Cannot create learning directory");
    auto lock_path = path;
    lock_path += L".lock";
    Handle lock{CreateFileW(lock_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (lock.value == INVALID_HANDLE_VALUE)
        return unavailable("Learning store busy or inaccessible");
    auto loaded = read(path);
    if (std::holds_alternative<Error>(loaded) && operation != Operation::clear)
        return std::get<Error>(loaded);
    LearningState state = std::holds_alternative<LearningState>(loaded)
                              ? std::get<LearningState>(std::move(loaded))
                              : LearningState{};
    bool changed = state.generation.empty();
    if (changed)
        state.generation = new_generation();
    if (operation == Operation::clear || (operation == Operation::enable && !state.enabled) ||
        (operation == Operation::disable && state.enabled)) {
        state.generation = new_generation();
        if (operation == Operation::clear) {
            state.uses.clear();
            for (auto it = state.phrases.begin(); it != state.phrases.end();) {
                if (!it->second.manual)
                    it = state.phrases.erase(it);
                else {
                    it->second.count = 0;
                    it->second.last_used = 0;
                    ++it;
                }
            }
        } else
            state.enabled = operation == Operation::enable;
        changed = true;
    }
    if (operation == Operation::add_word &&
        (!state.phrases.contains(word) || !state.phrases.at(word).manual)) {
        if (!state.phrases.contains(word) && state.phrases.size() >= personal_phrase_limit) {
            auto victim = state.phrases.end();
            for (auto it = state.phrases.begin(); it != state.phrases.end(); ++it)
                if (!it->second.manual &&
                    (victim == state.phrases.end() ||
                     std::tie(it->second.count, it->second.last_used) <
                         std::tie(victim->second.count, victim->second.last_used)))
                    victim = it;
            if (victim == state.phrases.end())
                return unavailable("Personal dictionary is full");
            state.phrases.erase(victim);
        }
        state.phrases[word].manual = true;
        state.generation = new_generation();
        changed = true;
    }
    if (operation == Operation::remove_word && state.phrases.erase(word)) {
        state.generation = new_generation();
        changed = true;
    }
    if (state.generation.empty())
        return unavailable("Cannot create learning generation");
    if (operation == Operation::sync && state.enabled && generation == state.generation) {
        for (const auto& [cp, increment] : pending) {
            if (!is_unicode_scalar(cp) || !cp || !increment.count)
                return Error{ErrorCode::invalid_argument, "Invalid character increment"};
            if (!state.uses.contains(cp) && state.uses.size() >= max_entries)
                continue;
            auto& use = state.uses[cp];
            use.count = static_cast<std::uint32_t>(std::min<std::uint64_t>(
                max_count, static_cast<std::uint64_t>(use.count) + increment.count));
            use.last_used = std::max(use.last_used, increment.last_used);
            changed = true;
        }
        for (const auto& [key, increment] : pending_phrases) {
            if (!valid_personal_phrase(key) || !increment.count || increment.manual)
                return Error{ErrorCode::invalid_argument, "Invalid word increment"};
            if (!state.phrases.contains(key) && state.phrases.size() >= personal_phrase_limit)
                continue;
            auto& use = state.phrases[key];
            use.count = static_cast<std::uint32_t>(std::min<std::uint64_t>(
                max_count, static_cast<std::uint64_t>(use.count) + increment.count));
            use.last_used = std::max(use.last_used, increment.last_used);
            changed = true;
        }
    }
    if (changed) {
        const auto saved = save(path, state);
        if (auto error = std::get_if<Error>(&saved))
            return *error;
    }
    return state;
}
} // namespace
std::filesystem::path learning_path() {
    const auto path = layout_path();
    return path.empty() ? std::filesystem::path{} : path.parent_path() / L"learning.dat";
}
Result<LearningState> sync_learning(const std::filesystem::path& path,
                                    const std::string& generation, const LearnedUses& pending,
                                    const PersonalPhrases& phrases) {
    return transaction(path, Operation::sync, generation, pending, phrases);
}
Result<LearningState> set_learning_enabled(const std::filesystem::path& path, bool enabled) {
    return transaction(path, enabled ? Operation::enable : Operation::disable, {}, {});
}
Result<LearningState> clear_learning(const std::filesystem::path& path) {
    return transaction(path, Operation::clear, {}, {});
}
Result<LearningState> add_personal_phrase(const std::filesystem::path& path, std::u32string word) {
    return transaction(path, Operation::add_word, {}, {}, {}, std::move(word));
}
Result<LearningState> remove_personal_phrase(const std::filesystem::path& path,
                                             std::u32string word) {
    return transaction(path, Operation::remove_word, {}, {}, {}, std::move(word));
}
} // namespace stroke::win
