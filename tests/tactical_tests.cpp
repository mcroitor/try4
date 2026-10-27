#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "chess/search.hpp"

namespace {

constexpr const char* kDefaultDataPath =
    "tests/data/tactical/positions.tsv";

struct TacticalCase {
    std::string id;
    std::string phase;
    std::string fen;
    int depth = 0;
    std::string expectations;
    std::string avoid;
    std::string note;
};

std::string DataPath() {
#ifdef TRY4_TACTICAL_DATA_DIR
    return std::string(TRY4_TACTICAL_DATA_DIR) + "/positions.tsv";
#else
    return kDefaultDataPath;
#endif
}

std::string Trim(std::string value) {
    while (!value.empty() &&
           (value.back() == '\r' || value.back() == ' ' ||
            value.back() == '\t')) {
        value.pop_back();
    }
    std::size_t begin = 0;
    while (begin < value.size() && (value[begin] == ' ' || value[begin] == '\t')) {
        ++begin;
    }
    return value.substr(begin);
}

std::vector<std::string> Split(std::string_view value, char delimiter) {
    std::vector<std::string> fields;
    std::size_t begin = 0;
    while (begin <= value.size()) {
        const std::size_t end = value.find(delimiter, begin);
        fields.emplace_back(value.substr(
            begin, end == std::string_view::npos ? value.size() - begin
                                                 : end - begin));
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    return fields;
}

std::vector<TacticalCase> LoadCases() {
    std::ifstream input(DataPath());
    REQUIRE(input.good());

    std::vector<TacticalCase> cases;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        line = Trim(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }

        const std::vector<std::string> fields = Split(line, '\t');
        INFO("tactical data line " << line_number);
        REQUIRE(fields.size() == 7);
        cases.push_back(TacticalCase{Trim(fields[0]), Trim(fields[1]),
                                     Trim(fields[2]),
                                     std::stoi(Trim(fields[3])), Trim(fields[4]),
                                     Trim(fields[5]), Trim(fields[6])});
    }
    return cases;
}

}  // namespace

TEST_CASE("curated tactical corpus passes deterministically", "[tactical]") {
    const std::vector<TacticalCase> cases = LoadCases();
    REQUIRE_FALSE(cases.empty());

    bool has_opening = false;
    bool has_middlegame = false;
    bool has_endgame = false;
    for (const TacticalCase& tactical : cases) {
        has_opening = has_opening || tactical.phase == "opening";
        has_middlegame = has_middlegame || tactical.phase == "middlegame";
        has_endgame = has_endgame || tactical.phase == "endgame";
    }
    CHECK(has_opening);
    CHECK(has_middlegame);
    CHECK(has_endgame);

    int passed = 0;
    for (const TacticalCase& tactical : cases) {
        INFO("tactical case: " << tactical.id);
        INFO("phase: " << tactical.phase);
        INFO("fen: " << tactical.fen);
        INFO("note: " << tactical.note);

        bool ok = true;
        const auto check = [&ok](bool condition, const char* message) {
            if (!condition) {
                ok = false;
                INFO(message);
                CHECK(condition);
            }
        };

        std::string error;
        const auto parsed = chess::Position::FromFen(tactical.fen, &error);
        check(parsed.has_value(), "FEN must parse");
        if (!parsed.has_value()) {
            continue;
        }
        const chess::Position& position = *parsed;
        if (tactical.depth <= 0) {
            ok = false;
            CHECK(tactical.depth > 0);
            continue;
        }

        chess::SearchLimits limits;
        limits.depth = tactical.depth;
        std::atomic_bool stop{false};
        const auto begin = std::chrono::steady_clock::now();
        const chess::SearchInfo info =
            chess::Searcher().Search(position, limits, stop);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - begin)
                                 .count();

        // A second fresh search catches ordering, TT, and iteration-order
        // nondeterminism without involving wall-clock limits.
        std::atomic_bool second_stop{false};
        const chess::SearchInfo second =
            chess::Searcher().Search(position, limits, second_stop);
        check(info.depth == second.depth, "repeated search depth is stable");
        check(info.scoreCp == second.scoreCp, "repeated search score is stable");
        check(info.nodes == second.nodes, "repeated search node count is stable");
        check(info.bestMove.has_value() == second.bestMove.has_value(),
              "repeated search best-move presence is stable");
        if (info.bestMove.has_value() && second.bestMove.has_value()) {
            check(info.bestMove->ToUci() == second.bestMove->ToUci(),
                  "repeated search best move is stable");
        }

        check(info.depth > 0, "search completed at least one depth");
        check(info.bestMove.has_value(), "search returned a best move");
        if (info.bestMove.has_value()) {
            check(position.IsLegal(*info.bestMove),
                  "best move must be legal in the corpus position");
        }

        chess::Position replay = position;
        for (const chess::Move& move : info.pv) {
            if (!replay.IsLegal(move)) {
                ok = false;
                CHECK(false);
                INFO("illegal PV move: " << move.ToUci());
                break;
            }
            replay.MakeMove(move);
        }

        for (const std::string& token : Split(tactical.expectations, ';')) {
            if (token.rfind("mate:", 0) == 0) {
                const int plies = std::stoi(token.substr(5));
                const bool exact_mate =
                    info.scoreCp >= chess::kMateThreshold &&
                    chess::kMateScore - info.scoreCp == plies;
                check(exact_mate, "expected exact mate score");
                if (!exact_mate) {
                    INFO("expected mate in " << plies << " plies, got score "
                                             << info.scoreCp);
                }
            } else if (token.rfind("best:", 0) == 0) {
                const std::string expected = token.substr(5);
                const bool best_matches =
                    info.bestMove.has_value() &&
                    info.bestMove->ToUci() == expected;
                check(best_matches, "expected best move");
                if (!best_matches) {
                    INFO("expected best move " << expected << ", got "
                                               << (info.bestMove
                                                       ? info.bestMove->ToUci()
                                                       : "none"));
                }
            } else if (token == "positive") {
                check(info.scoreCp > 0, "expected a positive score");
            } else if (token.rfind("score:", 0) == 0) {
                const int expected = std::stoi(token.substr(6));
                check(info.scoreCp == expected, "expected exact score");
            } else {
                ok = false;
                CHECK(false);
                INFO("unknown tactical expectation: " << token);
            }
        }

        if (tactical.avoid != "-" && !tactical.avoid.empty()) {
            const bool avoids =
                info.bestMove.has_value() &&
                info.bestMove->ToUci() != tactical.avoid;
            check(avoids, "search must avoid the documented trap move");
            if (!avoids) {
                INFO("trap move " << tactical.avoid << " was selected");
            }
        }

        // The runner intentionally keeps a per-position record in its output;
        // docs/TACTICAL.md mirrors a current run for reviewers.
        std::cout << "tactical " << tactical.id << " (" << tactical.phase
                  << "): " << elapsed << " ms, depth=" << info.depth
                  << ", nodes=" << info.nodes
                  << ", best="
                  << (info.bestMove ? info.bestMove->ToUci() : "none")
                  << '\n';
        if (ok) {
            ++passed;
        }
    }

    const int total = static_cast<int>(cases.size());
    std::cout << "tactical pass rate: " << passed << '/' << total << '\n'
              << std::flush;
    CHECK(passed == total);
    REQUIRE(passed == total);
}
