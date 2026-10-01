#include "stroke/dictionary/phrases.hpp"
#include "stroke/dictionary/text.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
namespace {
template <class T> T take(stroke::Result<T> result) {
    if (auto error = std::get_if<stroke::Error>(&result))
        throw std::runtime_error(error->message);
    return std::get<T>(std::move(result));
}
void write(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary);
    if (!out || !out.write(bytes.data(), static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Cannot write phrase output");
    out.close();
    if (!out)
        throw std::runtime_error("Cannot flush phrase output");
}
} // namespace
void build_phrases(const std::filesystem::path& source, const std::filesystem::path& stroke_file,
                   const std::filesystem::path& output) {
    using namespace stroke;
    namespace fs = std::filesystem;
    if (fs::exists(output))
        throw std::runtime_error("Phrase output exists; choose a new directory");
    auto dictionary = take(IndexedDictionary::load(stroke_file));
    std::set<char32_t> supported;
    for (int i = 1; i <= 5; ++i) {
        const Stroke key = static_cast<Stroke>(i);
        for (const auto& candidate : take(dictionary->lookup(std::span(&key, 1))))
            supported.insert(candidate.character);
    }
    const auto bytes = take(read_bytes(source / "tsi.csv"));
    if (bytes.find("# dc:license,LGPL-2.1-or-later,") == std::string::npos)
        throw std::runtime_error("Required upstream phrase license missing");
    const auto provenance = take(read_bytes(source / "provenance.json"));
    const auto key = provenance.find("\"commit\"");
    const auto colon = key == std::string::npos ? key : provenance.find(':', key + 8);
    const auto quote = colon == std::string::npos ? colon : provenance.find('"', colon + 1);
    const auto end = quote == std::string::npos ? quote : provenance.find('"', quote + 1);
    if (end == std::string::npos)
        throw std::runtime_error("Upstream revision missing");
    const auto revision = provenance.substr(quote + 1, end - quote - 1);
    PhraseScores terms;
    std::istringstream input(bytes);
    std::string line;
    std::size_t rows{}, single{}, length{}, unsupported{}, duplicates{};
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line.starts_with('#'))
            continue;
        ++rows;
        auto first = line.find(',');
        auto second = first == std::string::npos ? first : line.find(',', first + 1);
        if (first == std::string::npos || second == std::string::npos ||
            second + 1 == line.size() || line.find(',', second + 1) != std::string::npos ||
            line.find('"') != std::string::npos)
            throw std::runtime_error("Unsupported phrase CSV row");
        auto text = take(decode_utf8(std::string_view(line).substr(0, first)));
        std::uint32_t score{};
        auto number = std::string_view(line).substr(first + 1, second - first - 1);
        const auto [tail, error] =
            std::from_chars(number.data(), number.data() + number.size(), score);
        if (text.empty() || error != std::errc{} || tail != number.data() + number.size())
            throw std::runtime_error("Invalid phrase or score");
        if (text.size() == 1) {
            ++single;
            continue;
        }
        if (text.size() > max_phrase_length) {
            ++length;
            continue;
        }
        if (!std::all_of(text.begin(), text.end(),
                         [&](auto cp) { return supported.contains(cp); })) {
            ++unsupported;
            continue;
        }
        const auto [entry, inserted] = terms.emplace(std::move(text), score);
        if (!inserted) {
            ++duplicates;
            entry->second = std::max(entry->second, score);
        }
    }
    const auto index = take(encode_phrases(terms, dictionary->info().data_version, revision));
    (void)take(PhraseIndex::decode(index));
    std::map<std::string, std::string> originals;
    for (auto name : {"tsi.csv", "provenance.json", "COPYING.LGPL-2.1.txt", "source.zip"})
        originals.emplace(name, take(read_bytes(source / name)));
    auto stage = output;
    stage += ".staging";
    fs::create_directories(output.parent_path());
    if (!fs::create_directory(stage))
        throw std::runtime_error("Phrase staging exists");
    try {
        fs::create_directory(stage / "sources");
        write(stage / "phrases.pidx", index);
        for (const auto& [name, value] : originals)
            write(stage / "sources" / name, value);
        write(stage / "NOTICE.txt",
              "libchewing-data phrase priorities, Copyright (c) 2025 libchewing Core Team.\n"
              "License: LGPL-2.1-or-later; derived phrase index retains this license.\n"
              "Source: https://codeberg.org/chewing/libchewing-data\n"
              "Pinned revision: " +
                  revision +
                  "\n"
                  "Modified: omit pronunciations; retain 2..8 Unicode-scalar phrases with all "
                  "characters\n"
                  "present in the supplied stroke dictionary; deduplicate readings by maximum "
                  "score;\n"
                  "sort by Unicode and add version metadata and CRC32. Priorities are not raw "
                  "counts.\n"
                  "Original data, provenance, license and source archive are in sources/.\n"
                  "Conversion source is ../frequency/converter-source.zip in installed dictionary "
                  "bundles.\n"
                  "Rebuild: stroke_dict_builder phrases --source sources --index STROKE_INDEX "
                  "--output NEW_DIR\n");
        const std::string report =
            "rows\t" + std::to_string(rows) + "\nsingle_rows\t" + std::to_string(single) +
            "\nlength_filtered\t" + std::to_string(length) + "\nunsupported_rows\t" +
            std::to_string(unsupported) + "\nmerged_readings\t" + std::to_string(duplicates) +
            "\nphrases\t" + std::to_string(terms.size()) + "\nbytes\t" +
            std::to_string(index.size()) + "\nstroke_version\t" + dictionary->info().data_version +
            "\n";
        write(stage / "report.tsv", report);
        if (fs::exists(output))
            throw std::runtime_error("Output appeared during build");
        fs::rename(stage, output);
    } catch (...) {
        std::error_code ignored;
        fs::remove_all(stage, ignored);
        throw;
    }
}
