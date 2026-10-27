#include "chess/configuration.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace chess {
namespace {

constexpr const char* kWeightNames[] = {
    "mobility_mg", "mobility_eg", "bishop_pair_mg", "bishop_pair_eg",
    "rook_open_mg", "rook_open_eg", "rook_semi_open_mg", "rook_semi_open_eg",
    "doubled_pawn_mg", "doubled_pawn_eg", "isolated_pawn_mg", "isolated_pawn_eg",
    "shield_one_mg", "shield_two_mg", "shield_missing_mg",
    "shield_one_eg", "shield_two_eg", "shield_missing_eg",
    "king_attacker_mg", "king_attacker_eg", "tempo", "pst_mg_percent", "pst_eg_percent"
};
static_assert(std::size(kWeightNames) == static_cast<std::size_t>(EvalWeight::Count));

std::string Trim(const std::string& s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    return begin == std::string::npos ? "" : s.substr(begin, s.find_last_not_of(" \t\r\n") - begin + 1);
}

bool Integer(const std::string& s, int* value, int minimum, int maximum) {
    const auto result = std::from_chars(s.data(), s.data() + s.size(), *value);
    return !s.empty() && result.ec == std::errc{} && result.ptr == s.data() + s.size() &&
           *value >= minimum && *value <= maximum;
}

bool Boolean(const std::string& s, bool* value) {
    if (s != "true" && s != "false") { return false; }
    *value = s == "true";
    return true;
}

bool Utf8(const std::string& text) {
    for (std::size_t i = 0; i < text.size();) {
        const auto c = static_cast<unsigned char>(text[i++]);
        if (c == 0) { return false; }
        if (c < 128) { continue; }
        int remaining = 0;
        unsigned value = 0;
        unsigned minimum = 0;
        if (c >= 0xc2 && c <= 0xdf) { remaining = 1; value = c & 31; minimum = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { remaining = 2; value = c & 15; minimum = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { remaining = 3; value = c & 7; minimum = 0x10000; }
        else { return false; }
        for (int j = 0; j < remaining; ++j) {
            if (i >= text.size()) { return false; }
            const auto next = static_cast<unsigned char>(text[i++]);
            if ((next & 0xc0) != 0x80) { return false; }
            value = (value << 6) | (next & 63);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) { return false; }
    }
    return true;
}

template<std::size_t N>
bool Array(const std::string& text, std::array<int, N>* values, int maximum) {
    std::size_t start = 0;
    for (std::size_t i = 0; i < N; ++i) {
        const auto end = text.find(',', start);
        if ((i + 1 == N) != (end == std::string::npos)) { return false; }
        if (!Integer(Trim(text.substr(start, end == std::string::npos ? end : end - start)),
                     &(*values)[i], 0, maximum)) { return false; }
        start = end == std::string::npos ? text.size() : end + 1;
    }
    return true;
}

bool Assign(UciOptions* options, const std::string& section,
            const std::string& key, const std::string& value) {
    auto& o = *options;
    if (section == "uci") {
        if (key == "book_file") { o.book_path = value; return true; }
        if (key == "own_book") { return Boolean(value, &o.book_enabled); }
        if (key == "hash_mb") {
            int number = 0;
            if (!Integer(value, &number, 1, 131072)) { return false; }
            o.hash_mb = static_cast<std::size_t>(number); return true;
        }
        if (key == "move_overhead_ms") { return Integer(value, &o.move_overhead_ms, 0, 5000); }
        if (key == "threads") { return Integer(value, &o.threads, 1, 1); }
        if (key == "book_max_ply") { return Integer(value, &o.book_max_ply, 0, 10000); }
    } else if (section == "evaluation") {
        auto& p = o.evaluation;
        if (key == "material_mg") { return Array(value, &p.material_mg, 10000); }
        if (key == "material_eg") { return Array(value, &p.material_eg, 10000); }
        if (key == "passed_mg") { return Array(value, &p.passed_mg, 1000); }
        if (key == "passed_eg") { return Array(value, &p.passed_eg, 1000); }
        for (std::size_t i = 0; i < std::size(kWeightNames); ++i) {
            if (key == kWeightNames[i]) { return Integer(value, &p.weights[i], 0, 1000); }
        }
    } else if (section == "search") {
        if (key == "forcing_checks") { return Boolean(value, &o.forcing.checks); }
        if (key == "forcing_max_plies") { return Integer(value, &o.forcing.max_plies, 0, 120); }
        if (key == "forcing_hard_plies") { return Integer(value, &o.forcing.hard_plies, 1, 120); }
    }
    return false;
}

std::filesystem::path ExecutablePath(const char* argument, std::error_code* error) {
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length > 0 && length < buffer.size()) {
        buffer.resize(length); return std::filesystem::path(buffer);
    }
#else
    const auto path = std::filesystem::read_symlink("/proc/self/exe", *error);
    if (!*error) { return path; }
#endif
    error->clear();
    return std::filesystem::absolute(argument, *error);
}

}  // namespace

std::optional<UciOptions> ParseConfiguration(std::istream& input,
    const std::string& source, std::string* error) {
    UciOptions result;
    std::string line, section;
    std::set<std::string> seen;
    std::size_t number = 0;
    if (error) { error->clear(); }
    const auto fail = [&](const std::string& message) -> std::optional<UciOptions> {
        if (error) { *error = source + ":" + std::to_string(number) + ": " + message; }
        return std::nullopt;
    };
    while (std::getline(input, line)) {
        ++number;
        if (number == 1 && line.compare(0, 3, "\xef\xbb\xbf") == 0) { line.erase(0, 3); }
        if (line.size() > 65536 || !Utf8(line)) { return fail("invalid UTF-8, NUL or oversized line"); }
        line = Trim(line);
        if (line.empty() || line.front() == '#' || line.front() == ';') { continue; }
        if (line.front() == '[' && line.back() == ']') {
            section = Trim(line.substr(1, line.size() - 2));
            if (section != "uci" && section != "evaluation" && section != "search") { return fail("unknown section " + section); }
            continue;
        }
        const auto equals = line.find('=');
        if (equals == std::string::npos || section.empty()) { return fail("expected section and key=value"); }
        const auto key = Trim(line.substr(0, equals));
        const auto value = Trim(line.substr(equals + 1));
        if (!seen.insert(section + "." + key).second) { return fail("duplicate key " + section + "." + key); }
        if (section == "uci" && key == "threads") {
            int number = 0;
            if (!Integer(value, &number, 1, 1)) {
                return fail("uci.threads=" + value +
                            " is not supported; only one search thread is implemented");
            }
        }
        if (!Assign(&result, section, key, value)) { return fail("unknown key or invalid value: " + section + "." + key); }
        if (!ValidateEvaluation(result.evaluation)) { return fail("invalid evaluation key " + key + ": king/empty values must be zero"); }
    }
    if (!input.eof()) { return fail("read failure"); }
    if (result.forcing.max_plies > result.forcing.hard_plies) { return fail("search.forcing_max_plies exceeds forcing_hard_plies"); }
    return result;
}

std::optional<UciOptions> LoadConfiguration(const std::filesystem::path& path, std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error) { *error = "cannot open configuration: " + path.u8string(); }
        return std::nullopt;
    }
    auto options = ParseConfiguration(file, path.u8string(), error);
    if (options && !options->book_path.empty()) {
        const auto book = std::filesystem::u8path(options->book_path);
        if (book.is_relative()) {
            options->book_path = (std::filesystem::absolute(path).parent_path() / book).lexically_normal().u8string();
        }
    }
    return options;
}

std::optional<UciOptions> StartupConfiguration(int argc, char** argv, std::string* error) {
    if (argc == 3 && std::string(argv[1]) == "--config") {
        return LoadConfiguration(std::filesystem::u8path(argv[2]), error);
    }
    if (argc != 1) {
        if (error) { *error = "usage: try4_uci [--config PATH | --version]"; }
        return std::nullopt;
    }
    std::error_code code;
    const auto path = ExecutablePath(argv[0], &code).parent_path() / "try4.ini";
    if (!code && !std::filesystem::exists(path, code) && !code) { return UciOptions{}; }
    if (code) {
        if (error) { *error = "configuration discovery failed: " + code.message(); }
        return std::nullopt;
    }
    return LoadConfiguration(path, error);
}

std::string ConfigurationSnapshot(const UciOptions& o) {
    std::ostringstream out;
    out << "[uci]\nhash_mb=" << o.hash_mb << "\nmove_overhead_ms=" << o.move_overhead_ms
        << "\nthreads=" << o.threads << "\nown_book=" << (o.book_enabled ? "true" : "false")
        << "\nbook_file=" << o.book_path << "\nbook_max_ply=" << o.book_max_ply << '\n';
    out << "[evaluation]\n";
    const auto array = [&out](const char* name, const auto& values) {
        out << name << '=';
        for (std::size_t i = 0; i < values.size(); ++i) { if (i) { out << ','; } out << values[i]; }
        out << '\n';
    };
    array("material_mg", o.evaluation.material_mg); array("material_eg", o.evaluation.material_eg);
    array("passed_mg", o.evaluation.passed_mg); array("passed_eg", o.evaluation.passed_eg);
    for (std::size_t i = 0; i < std::size(kWeightNames); ++i) { out << kWeightNames[i] << '=' << o.evaluation.weights[i] << '\n'; }
    out << "[search]\nforcing_checks=" << (o.forcing.checks ? "true" : "false")
        << "\nforcing_max_plies=" << o.forcing.max_plies << "\nforcing_hard_plies=" << o.forcing.hard_plies << '\n';
    return out.str();
}

}  // namespace chess
