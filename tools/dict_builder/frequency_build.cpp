#include "stroke/dictionary/frequency.hpp"
#include "stroke/dictionary/text.hpp"
#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {
template<class T> T take(stroke::Result<T> value) {
    if (const auto* error = std::get_if<stroke::Error>(&value)) throw std::runtime_error(error->message);
    return std::get<T>(std::move(value));
}
void write(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary);
    if (!out || !out.write(bytes.data(), static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Cannot write frequency artifact");
    out.close();
    if (!out) throw std::runtime_error("Cannot flush frequency artifact");
}
}

void build_frequency(const std::filesystem::path& source, const std::filesystem::path& output) {
    using namespace stroke;
    namespace fs = std::filesystem;
    if (fs::exists(output)) throw std::runtime_error("Frequency output already exists");
    const auto bytes = take(read_bytes(source / "tsi.csv"));
    if (bytes.find("# dc:license,LGPL-2.1-or-later,") == std::string::npos)
        throw std::runtime_error("Expected upstream data license missing");
    FrequencyScores scores;
    std::istringstream input(bytes);
    std::string line;
    std::size_t single_rows{}, phrase_rows{};
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line.starts_with('#')) continue;
        const auto comma = line.find(',');
        const auto second = comma == std::string::npos ? comma : line.find(',', comma + 1);
        if (comma == std::string::npos || second == std::string::npos || second + 1 == line.size() ||
            line.find(',', second + 1) != std::string::npos || line.find('"') != std::string::npos)
            throw std::runtime_error("Unsupported upstream CSV row; expected three unquoted fields");
        const auto word = take(decode_utf8(std::string_view(line).substr(0, comma)));
        const auto number = std::string_view(line).substr(comma + 1, second - comma - 1);
        std::uint32_t value{};
        const auto [last, error] = std::from_chars(number.data(), number.data() + number.size(), value);
        if (word.empty() || error != std::errc{} || last != number.data() + number.size())
            throw std::runtime_error("Invalid upstream word or score");
        if (word.size() != 1) { ++phrase_rows; continue; }
        ++single_rows;
        // Preserve only Han ranges covered by the stroke dictionary's potential repertoire.
        const auto cp = word.front();
        const bool han = (cp >= 0x3400 && cp <= 0x4DBF) || (cp >= 0x4E00 && cp <= 0x9FFF) ||
            (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0x20000 && cp <= 0x323AF);
        if (!han) continue;
        auto [entry, inserted] = scores.emplace(cp, value);
        if (!inserted) entry->second = std::max(entry->second, value);
    }
    std::string table = "STROKE-FREQUENCY-1\n";
    for (const auto& [cp, score] : scores) table += encode_utf8(cp) + '\t' + std::to_string(score) + '\n';
    (void)take(parse_frequency(table));
    std::map<std::string, std::string> snapshots;
    for (const auto* name : {"tsi.csv", "word.csv", "README.upstream.md", "COPYING.LGPL-2.1.txt", "provenance.json", "source.zip"})
        snapshots.emplace(name, take(read_bytes(source / name)));
    const auto staging = fs::path(output.native() + fs::path(".staging").native());
    if (fs::exists(staging)) throw std::runtime_error("Frequency staging already exists");
    try {
        fs::create_directories(staging / "sources");
        write(staging / "character-score.tsv", table);
        for (const auto& [name, contents] : snapshots) write(staging / "sources" / name, contents);
        write(staging / "NOTICE.txt",
            "Derived from libchewing-data tsi.csv. Copyright (c) 2025 libchewing Core Team.\n"
            "License: LGPL-2.1-or-later; full text in sources/COPYING.LGPL-2.1.txt.\n"
            "Source: https://github.com/chewing/libchewing-data\n"
            "Current upstream: https://codeberg.org/chewing/libchewing-data\n"
            "Pinned revision and original download hashes: sources/provenance.json.\n"
            "Modified 2026-09-20: select single Han characters; merge readings by maximum score;\n"
            "sort by Unicode; preserve zero scores. Scores are IME priorities, not raw counts.\n"
            "Derived table remains LGPL-2.1-or-later. Conway stroke data is separate.\n"
            "Rebuild with stroke_dict_builder frequency --source sources --output NEW_DIRECTORY.\n");
        write(staging / "report.tsv", "single_rows\t" + std::to_string(single_rows) +
            "\nphrase_rows\t" + std::to_string(phrase_rows) + "\nunique_han\t" + std::to_string(scores.size()) + '\n');
        fs::rename(staging, output);
    } catch (...) {
        std::error_code ignored;
        fs::remove_all(staging, ignored);
        throw;
    }
}
