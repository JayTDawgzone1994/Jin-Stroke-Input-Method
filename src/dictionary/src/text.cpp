#include "stroke/dictionary/text.hpp"

#include <fstream>

namespace stroke {

Result<std::u32string> decode_utf8(std::string_view text) {
    std::u32string result;
    for (std::size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i++]);
        char32_t cp{};
        unsigned extra{};
        char32_t minimum{};
        if (first < 0x80) { cp = first; }
        else if (first >= 0xC2 && first <= 0xDF) { cp = first & 0x1F; extra = 1; minimum = 0x80; }
        else if (first >= 0xE0 && first <= 0xEF) { cp = first & 0x0F; extra = 2; minimum = 0x800; }
        else if (first >= 0xF0 && first <= 0xF4) { cp = first & 0x07; extra = 3; minimum = 0x10000; }
        else { return Error{ErrorCode::invalid_data, "Invalid UTF-8 leading byte"}; }
        if (extra > text.size() - i) { return Error{ErrorCode::invalid_data, "Truncated UTF-8"}; }
        for (unsigned n = 0; n < extra; ++n) {
            const auto byte = static_cast<unsigned char>(text[i++]);
            if ((byte & 0xC0) != 0x80) { return Error{ErrorCode::invalid_data, "Invalid UTF-8 continuation"}; }
            cp = (cp << 6) | (byte & 0x3F);
        }
        if (cp < minimum || !is_unicode_scalar(cp)) { return Error{ErrorCode::invalid_data, "Invalid Unicode scalar"}; }
        result.push_back(cp);
    }
    return result;
}

std::string encode_utf8(char32_t cp) {
    if (!is_unicode_scalar(cp)) { return {}; }
    std::string out;
    if (cp < 0x80) { out.push_back(static_cast<char>(cp)); }
    else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

Result<std::string> read_bytes(const std::filesystem::path& path, std::size_t limit) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) { return Error{ErrorCode::io_error, "Cannot open input file"}; }
    const auto end = input.tellg();
    if (end < 0 || static_cast<std::uintmax_t>(end) > limit) {
        return Error{ErrorCode::invalid_data, "Input exceeds size limit"};
    }
    std::string bytes(static_cast<std::size_t>(end), '\0');
    input.seekg(0);
    if (!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
        return Error{ErrorCode::io_error, "Cannot read complete input file"};
    }
    return bytes;
}

} // namespace stroke
