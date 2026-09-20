#include "stroke/build/compiler.hpp"
#include "stroke/dictionary/text.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;
using namespace stroke;
void build_frequency(const fs::path& source, const fs::path& output);
namespace {
template <typename T> T take(Result<T> result) {
    if (const auto* error = std::get_if<Error>(&result)) { throw std::runtime_error(error->message); }
    return std::get<T>(std::move(result));
}
fs::path path(std::string_view text) {
    std::u8string encoded;
    for (const unsigned char byte : text) { encoded.push_back(static_cast<char8_t>(byte)); }
    return fs::path(encoded);
}
std::string hex(std::uint32_t value) {
    std::ostringstream out;
    out << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << value;
    return out.str();
}
void write(const fs::path& file, std::string_view bytes) {
    std::ofstream out(file, std::ios::binary);
    if (!out || !out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
        throw std::runtime_error("Cannot write output file");
    }
    out.close();
    if (!out) { throw std::runtime_error("Cannot flush output file"); }
}
struct Options {
    std::map<std::string, std::string> values;
    std::vector<fs::path> overrides;
};
Options parse(const std::vector<std::string>& args, const std::vector<std::string>& allowed) {
    Options options;
    for (std::size_t i = 2; i < args.size(); i += 2) {
        if (i + 1 >= args.size() || std::find(allowed.begin(), allowed.end(), args[i]) == allowed.end()) {
            throw std::runtime_error("Unknown option or missing value: " + args[i]);
        }
        if (args[i] == "--overrides") { options.overrides.push_back(path(args[i + 1])); }
        else if (!options.values.emplace(args[i], args[i + 1]).second) {
            throw std::runtime_error("Duplicate option: " + args[i]);
        }
    }
    return options;
}
std::string required(const Options& options, const std::string& key) {
    const auto value = options.values.find(key);
    if (value == options.values.end() || value->second.empty()) { throw std::runtime_error("Required option: " + key); }
    return value->second;
}
void build_dictionary(const Options& options) {
    const auto source = path(required(options, "--source"));
    const auto output = fs::absolute(path(required(options, "--output"))).lexically_normal();
    if (fs::exists(output)) { throw std::runtime_error("Output already exists; select a new version directory"); }
    const auto scope_name = options.values.contains("--scope") ? options.values.at("--scope") : "traditional";
    if (scope_name != "traditional" && scope_name != "all") { throw std::runtime_error("Scope must be traditional or all"); }
    const auto scope = scope_name == "all" ? build::Scope::all : build::Scope::traditional;
    std::map<std::string, std::string> snapshots;
    for (const auto* name : {"codepoint-character-sequence.txt", "sequence-characters.txt", "README.md", "SOURCE.txt"}) {
        snapshots.emplace(name, take(read_bytes(source / name)));
    }
    const auto& provenance = snapshots.at("SOURCE.txt");
    std::istringstream manifest(provenance);
    std::string line, revision;
    while (std::getline(manifest, line)) {
        if (!line.empty() && line.back() == '\r') { line.pop_back(); }
        if (line.starts_with("revision=")) { revision = line.substr(9); }
    }
    if (revision.size() != 40 || revision.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
        throw std::runtime_error("SOURCE.txt requires a 40-digit revision= value");
    }
    auto data = take(build::parse_conway(snapshots.at("codepoint-character-sequence.txt")));
    take(build::verify_reference(data, snapshots.at("sequence-characters.txt")));
    const auto source_chars = data.size();
    const auto source_pairs = build::make_records(data, build::Scope::all).size();
    std::string modifications;
    for (std::size_t i = 0; i < options.overrides.size(); ++i) {
        const auto name = "override-" + std::to_string(i + 1) + ".tsv";
        auto bytes = take(read_bytes(options.overrides[i], 1024 * 1024));
        data = take(build::apply_overrides(std::move(data), bytes));
        modifications += name + " CRC32=" + hex(checksum(bytes)) + "\n";
        snapshots.emplace(name, std::move(bytes));
    }
    const auto records = build::make_records(data, scope);
    std::set<char32_t> retained;
    std::set<std::string> sequences;
    for (const auto& r : records) { retained.insert(r.character); sequences.insert(r.sequence); }
    std::string notice =
        "Conway Stroke Data\nCopyright 2021--2026 Conway (@yawnoc).\n"
        "Source: https://github.com/stroke-input/stroke-input-data\n"
        "License: Creative Commons Attribution 4.0 International (CC-BY-4.0)\n"
        "https://creativecommons.org/licenses/by/4.0/\n"
        "Original copyright/license notices and source are retained in sources/.\n"
        "Modified: expanded alternatives, filtered by declared character class, deduplicated and indexed.\n"
        "No author endorsement is implied. CRC32 identifies bytes and detects corruption; it is not authentication.\n"
        "Scope: " + scope_name + "\n"
        "Traditional policy: exclude simplified-only; retain shared and traditional-only.\n"
        "This is not certification of Taiwan standard forms or stroke order.\n"
        "Scoring: unscored (0); higher frequency_score is preferred; no frequency data imported.\n"
        "Source revision: " + revision + "\n";
    for (const auto& [name, bytes] : snapshots) {
        notice += "Input " + name + " CRC32=" + hex(checksum(bytes)) + "\n";
    }
    notice += "Override layers (in order):\n" + (modifications.empty() ? "none\n" : modifications);
    const std::string data_version = "conway-" + revision.substr(0, 12) + "-" + scope_name + "-" + hex(checksum(notice));
    const DictionaryInfo info{index_format_version, data_version, revision};
    const auto index = take(encode_index(records, info, notice));
    std::ostringstream report;
    report << "data_version\t" << data_version << "\nsource_characters\t" << source_chars
           << "\nsource_pairs\t" << source_pairs << "\nreference_verified\ttrue"
           << "\ncharacters_after_overrides\t" << data.size()
           << "\nretained_characters\t" << retained.size()
           << "\nfiltered_characters\t" << data.size() - retained.size()
           << "\nindex_pairs\t" << records.size() << "\nunique_sequences\t" << sequences.size()
           << "\nindex_bytes\t" << index.size() << "\noverride_layers\t" << options.overrides.size() << '\n';
    fs::create_directories(output.parent_path());
    auto stage = output;
    stage += ".staging";
    if (!fs::create_directory(stage)) { throw std::runtime_error("Staging directory exists; select a new output path"); }
    try {
        fs::create_directory(stage / "sources");
        for (const auto& [name, bytes] : snapshots) { write(stage / "sources" / name, bytes); }
        write(stage / "dictionary.sidx", index);
        write(stage / "NOTICE.txt", notice);
        write(stage / "report.tsv", report.str());
        take(IndexedDictionary::load(stage / "dictionary.sidx"));
        if (fs::exists(output)) { throw std::runtime_error("Output appeared during build; refusing replacement"); }
        fs::rename(stage, output);
    } catch (...) {
        // Only remove the exclusive staging sibling created by this call, never a source path.
        std::error_code ignored;
        fs::remove_all(stage, ignored);
        throw;
    }
    std::cout << report.str();
}

int run(const std::vector<std::string>& args) {
    try {
        if (args.size() == 2 && args[1] == "--version") {
            std::cout << "StrokeIME dictionary tool " << STROKE_VERSION << " / index format " << index_format_version << '\n';
            return 0;
        }
        if (args.size() == 2 && args[1] == "--help") {
            std::cout << "build --source DIR --output NEW_DIR [--scope traditional|all] [--overrides TSV ...]\n"
                         "frequency --source LIBCHEWING_DIR --output NEW_DIR\n"
                         "inspect --index FILE\nquery --index FILE --strokes DIGITS\n"
                         "Build validates the full upstream reference before filtering; never overwrites an output bundle.\n";
            return 0;
        }
        if (args.size() < 2) { throw std::runtime_error("Use --help"); }
        if (args[1] == "build") {
            build_dictionary(parse(args, {"--source", "--output", "--scope", "--overrides"}));
        } else if (args[1] == "frequency") {
            const auto options = parse(args, {"--source", "--output"});
            build_frequency(path(required(options, "--source")), path(required(options, "--output")));
        } else if (args[1] == "inspect" || args[1] == "query") {
            const auto options = parse(args, args[1] == "query" ?
                std::vector<std::string>{"--index", "--strokes"} : std::vector<std::string>{"--index"});
            const auto dictionary = take(IndexedDictionary::load(path(required(options, "--index"))));
            if (args[1] == "inspect") {
                std::cout << "version\t" << dictionary->info().data_version << "\npairs\t"
                          << dictionary->record_count() << '\n' << dictionary->notice();
            } else {
                StrokeSequence prefix;
                for (const char c : required(options, "--strokes")) {
                    if (c != '*' && (c < '1' || c > '5')) { throw std::runtime_error("Strokes must contain only 1..5 or *"); }
                    prefix.push_back(c == '*' ? Stroke::wildcard : static_cast<Stroke>(c - '0'));
                }
                const auto result = take(dictionary->lookup(prefix));
                std::cout << "candidates\t" << result.size() << '\n';
                for (const auto& candidate : result) {
                    std::cout << "U+" << hex(static_cast<std::uint32_t>(candidate.character)) << '\t'
                              << encode_utf8(candidate.character) << '\t'
                              << (candidate.exact_match ? "exact" : "prefix") << '\t'
                              << candidate.frequency_score << '\n';
                }
            }
        } else { throw std::runtime_error("Unknown command; use --help"); }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Dictionary error: " << error.what() << '\n';
        return 2;
    }
}
} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[]) {
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) {
        const auto text = fs::path(argv[i]).u8string();
        args.emplace_back(reinterpret_cast<const char*>(text.data()), text.size());
    }
    return run(args);
}
#else
int main(int argc, char* argv[]) { return run(std::vector<std::string>(argv, argv + argc)); }
#endif
