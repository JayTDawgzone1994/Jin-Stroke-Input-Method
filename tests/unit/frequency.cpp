#include "stroke/dictionary/frequency.hpp"
#include "stroke/dictionary/index.hpp"
#include "stroke/dictionary/text.hpp"
#include "stroke/engine/session.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace stroke;
template<class T> T take(Result<T> value) {
    if (const auto* error = std::get_if<Error>(&value)) throw std::runtime_error(error->message);
    return std::get<T>(std::move(value));
}
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void write(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary); out << bytes;
    if (!out) throw std::runtime_error("Fixture write failed");
}
int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    fs::path temporary;
    try {
        const auto table = take(parse_frequency("STROKE-FREQUENCY-1\n一\t0\n木\t4294967295\n𠮷\t5\n"));
        check(table.at(U'木') == 0xFFFFFFFFU && table.at(U'𠮷') == 5, "Score bounds and Unicode");
        for (const auto* bad : {"", "STROKE-FREQUENCY-2\n木\t1\n", "STROKE-FREQUENCY-1\n",
            "STROKE-FREQUENCY-1\n木\t1\n木\t2\n", "STROKE-FREQUENCY-1\n木\t-1\n",
            "STROKE-FREQUENCY-1\n木\t4294967296\n", "STROKE-FREQUENCY-1\n木\t1x\n",
            "STROKE-FREQUENCY-1\n木頭\t1\n", "STROKE-FREQUENCY-1\n木\t\n"})
            check(std::holds_alternative<Error>(parse_frequency(bad)), "Malformed score table rejected");
        temporary = fs::temp_directory_path() / ("stroke-frequency-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(temporary / "frequency");
        const auto index = temporary / "dictionary.sidx";
        write(index, take(encode_index({{"1", U'一', 0}, {"12", U'丁', 0}, {"1234", U'木', 0},
            {"12341", U'木', 0}}, {index_format_version, "fixture", "test"}, "fixture")));
        const auto query = StrokeSequence{Stroke::horizontal};
        check(take(take(IndexedDictionary::load(index))->lookup(query))[2].frequency_score == 0, "Absent table preserves unscored dictionary");
        write(temporary / "frequency/character-score.tsv", "STROKE-FREQUENCY-1\n木\t100\n丁\t20\n");
        const auto dictionary = take(IndexedDictionary::load(index));
        Session session; take(session.configure(dictionary, Config{}));
        const auto state = take(session.process(AppendStroke{Stroke::horizontal})).snapshot;
        check(state.visible_candidates.size() == 3 && state.visible_candidates[0].character == U'一' &&
              state.visible_candidates[1].character == U'木' && state.visible_candidates[1].frequency_score == 100 &&
              state.visible_candidates[2].character == U'丁', "Overlay scores preserve exact priority and deduplicate");
        write(temporary / "frequency/character-score.tsv", "corrupt");
        check(std::holds_alternative<Error>(IndexedDictionary::load(index)), "Corrupt present table is not silently ignored");
        check(take(dictionary->lookup(query))[2].frequency_score == 100, "Loaded scores remain immutable after file changes");
        if (argc == 2) {
            const auto real = take(parse_frequency(take(read_bytes(fs::path(argv[1])))));
            check(real.at(U'行') == 34993 && real.at(U'的') == 147270 && real.at(U'一') == 34112,
                  "Actual multi-reading scores use maximum, not sum");
            check(!real.contains(U'ㄅ') && real.size() > 18000, "Single Han extraction");
        }
        fs::remove_all(temporary);
        std::cout << "Frequency parsing, overlay, immutability and score ordering passed\n";
        return 0;
    } catch (const std::exception& error) {
        if (!temporary.empty()) { std::error_code ignored; fs::remove_all(temporary, ignored); }
        std::cerr << error.what() << '\n'; return 1;
    }
}
