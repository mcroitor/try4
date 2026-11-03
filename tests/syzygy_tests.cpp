#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>

#include "chess/search.hpp"
#include "chess/syzygy.hpp"
#include "chess/uci.hpp"

namespace {

std::string TablebasePath() {
#ifdef _MSC_VER
    char* env = nullptr;
    std::size_t length = 0;
    const auto result = _dupenv_s(&env, &length, "TRY4_SYZYGY_PATH");
    const std::string path = result == 0 && env != nullptr ? env : "";
    std::free(env);
    if (!path.empty()) {
        return path;
    }
#else
    if (const char* env = std::getenv("TRY4_SYZYGY_PATH");
        env != nullptr && *env != '\0') {
        return env;
    }
#endif
#ifdef _WIN32
    const std::string fallback = "c:\\games\\see\\Syzygy345";
    if (std::filesystem::exists(fallback)) {
        return fallback;
    }
#endif
    return {};
}

std::string RunUciSession(const std::string& script,
                          const chess::UciOptions& options = {}) {
    std::istringstream input("isready\n" + script);
    std::ostringstream output;
    chess::UciEngine engine(options);
    engine.Run(input, output);
    return output.str();
}

struct LoadedTablebase {
    chess::SyzygyTablebase tablebase;
};

}  // namespace

TEST_CASE("Syzygy missing data fails without crashing", "[syzygy]") {
    chess::SyzygyTablebase tablebase;
    std::string error;
    REQUIRE_FALSE(tablebase.Load("", &error));
    REQUIRE(error.find("empty") != std::string::npos);
    REQUIRE_FALSE(tablebase.Ready());

    REQUIRE_FALSE(tablebase.Load("try4-no-such-tablebase-directory", &error));
    REQUIRE(error.find("no Syzygy tablebase files") != std::string::npos);
    REQUIRE_FALSE(tablebase.Ready());
}

TEST_CASE("Syzygy loads the 3-4-5 set and probes KQvK", "[syzygy]") {
    const std::string path = TablebasePath();
    if (path.empty()) {
        SKIP("set TRY4_SYZYGY_PATH to a Syzygy directory to run this test");
    }

    LoadedTablebase loaded;
    std::string error;
    REQUIRE(loaded.tablebase.Load(path, &error));
    INFO(error);
    REQUIRE(loaded.tablebase.Ready());
    REQUIRE(loaded.tablebase.MaxPieces() >= 5);

    const auto position =
        chess::Position::FromFen("4k3/8/8/8/8/8/8/4K2Q w - - 0 1", &error);
    REQUIRE(position);
    REQUIRE(loaded.tablebase.CanProbe(*position));

    int wdl = 0;
    REQUIRE(loaded.tablebase.ProbeWdl(*position, &wdl));
    REQUIRE(wdl == 2);

    const auto root = loaded.tablebase.ProbeRoot(*position);
    REQUIRE(root.has_value());
    REQUIRE(root->wdl == 2);
    REQUIRE(root->dtz > 0);
    REQUIRE(root->scoreCp == chess::kSyzygyWinScore);
    REQUIRE(position->IsLegal(root->bestMove));

    chess::Position after = *position;
    REQUIRE(after.MakeMove(root->bestMove));
    REQUIRE(loaded.tablebase.ProbeWdl(after, &wdl));
    REQUIRE(wdl <= 0);
}

TEST_CASE("Syzygy root probe applies the fifty-move clock", "[syzygy]") {
    const std::string path = TablebasePath();
    if (path.empty()) {
        SKIP("set TRY4_SYZYGY_PATH to a Syzygy directory to run this test");
    }

    chess::SyzygyTablebase tablebase;
    std::string error;
    REQUIRE(tablebase.Load(path, &error));
    INFO(error);

    // No mate in one is available here, so at halfmove 99 the DTZ-aware root
    // probe must not report a guaranteed win.
    const auto position =
        chess::Position::FromFen("4k3/8/8/8/8/8/8/4K2Q w - - 99 1", &error);
    REQUIRE(position);
    const auto root = tablebase.ProbeRoot(*position);
    REQUIRE(root.has_value());
    REQUIRE(root->wdl <= 1);
    REQUIRE(root->scoreCp <= chess::kSyzygyCursedWinScore);
    REQUIRE(position->IsLegal(root->bestMove));
}

TEST_CASE("Syzygy rejects material beyond the loaded limit", "[syzygy]") {
    const std::string path = TablebasePath();
    if (path.empty()) {
        SKIP("set TRY4_SYZYGY_PATH to a Syzygy directory to run this test");
    }

    chess::SyzygyTablebase tablebase;
    std::string error;
    REQUIRE(tablebase.Load(path, &error));
    INFO(error);

    const auto position = chess::Position::FromFen(
        "4k3/8/8/8/8/8/7p/QQ2K2Q w - - 0 1", &error);
    REQUIRE(position);
    REQUIRE_FALSE(tablebase.CanProbe(*position));
    REQUIRE_FALSE(tablebase.ProbeRoot(*position).has_value());
}

TEST_CASE("Syzygy probes 4- and 5-piece material", "[syzygy]") {
    const std::string path = TablebasePath();
    if (path.empty()) {
        SKIP("set TRY4_SYZYGY_PATH to a Syzygy directory to run this test");
    }

    chess::SyzygyTablebase tablebase;
    std::string error;
    REQUIRE(tablebase.Load(path, &error));
    INFO(error);

    const std::pair<const char*, int> cases[] = {
        // KQvKP: the side to move wins.
        {"8/4K3/8/1p6/8/k7/8/3Q4 w - - 0 1", 2},
        // KQvKP: the side to move is lost in the queen endgame.
        {"3k4/8/2p5/8/8/8/K7/3Q4 b - - 0 1", -2},
        // KPvKP: a passed-pawn race can be a decisive tablebase win.
        {"2k5/8/8/8/4K3/3pP3/8/8 b - - 0 1", 2},
        // KRvKP: the side to move is lost.
        {"2k5/4p3/8/8/7K/2R5/8/8 b - - 0 1", -2},
    };
    for (const auto& [fen, expected] : cases) {
        const auto position = chess::Position::FromFen(fen, &error);
        REQUIRE(position);
        REQUIRE(tablebase.CanProbe(*position));

        int wdl = 0;
        REQUIRE(tablebase.ProbeWdl(*position, &wdl));
        REQUIRE(wdl == expected);

        const auto root = tablebase.ProbeRoot(*position);
        REQUIRE(root.has_value());
        REQUIRE(root->wdl == expected);
        REQUIRE(position->IsLegal(root->bestMove));
        if (expected == 2) {
            REQUIRE(root->scoreCp == chess::kSyzygyWinScore);
            REQUIRE(root->dtz > 0);
        } else {
            REQUIRE(root->scoreCp == chess::kSyzygyLossScore);
            REQUIRE(root->dtz < 0);
        }
    }
}

TEST_CASE("Syzygy search uses a root tablebase result", "[syzygy][search]") {
    const std::string path = TablebasePath();
    if (path.empty()) {
        SKIP("set TRY4_SYZYGY_PATH to a Syzygy directory to run this test");
    }

    chess::SyzygyTablebase tablebase;
    std::string error;
    REQUIRE(tablebase.Load(path, &error));
    INFO(error);

    const auto position =
        chess::Position::FromFen("4k3/8/8/8/8/8/8/4K2Q w - - 0 1", &error);
    REQUIRE(position);

    chess::SearchLimits limits;
    limits.depth = 4;
    limits.tablebase = &tablebase;
    std::atomic_bool stop{false};
    chess::Searcher searcher(8);
    const chess::SearchInfo info = searcher.Search(*position, limits, stop);

    REQUIRE(info.tablebase);
    REQUIRE(info.depth == 0);
    REQUIRE(info.bestMove.has_value());
    REQUIRE(position->IsLegal(*info.bestMove));
    REQUIRE(info.scoreCp == chess::kSyzygyWinScore);
    REQUIRE(info.pv.size() == 1);
}

TEST_CASE("UCI advertises and uses SyzygyPath", "[syzygy][uci]") {
    const std::string path = TablebasePath();
    if (path.empty()) {
        SKIP("set TRY4_SYZYGY_PATH to a Syzygy directory to run this test");
    }

    chess::UciOptions options;
    options.syzygy_path = path;
    const std::string out = RunUciSession(
        "uci\n"
        "position fen 4k3/8/8/8/8/8/8/4K2Q w - - 0 1\n"
        "go depth 4\n"
        "quit\n",
        options);

    REQUIRE(out.find("option name SyzygyPath type string default ") !=
            std::string::npos);
    REQUIRE(out.find("info string Syzygy loaded: 5-piece tables") !=
            std::string::npos);
    REQUIRE(out.find("info string Syzygy WDL win") != std::string::npos);
    REQUIRE(out.find("bestmove ") != std::string::npos);
    REQUIRE(out.find("bestmove 0000") == std::string::npos);
}

TEST_CASE("UCI reloads SyzygyPath through setoption", "[syzygy][uci]") {
    const std::string path = TablebasePath();
    if (path.empty()) {
        SKIP("set TRY4_SYZYGY_PATH to a Syzygy directory to run this test");
    }

    std::ostringstream script;
    script << "uci\n"
           << "setoption name SyzygyPath value " << path << "\n"
           << "position fen 4k3/8/8/8/8/8/8/4K2Q w - - 0 1\n"
           << "go depth 4\n"
           << "setoption name SyzygyPath value try4-no-such-dir\n"
           << "quit\n";
    const std::string out = RunUciSession(script.str());

    REQUIRE(out.find("info string Syzygy loaded: 5-piece tables") !=
            std::string::npos);
    REQUIRE(out.find("info string Syzygy WDL win") != std::string::npos);
    REQUIRE(out.find("info string Syzygy load failed:") != std::string::npos);
}

TEST_CASE("UCI warns when SyzygyPath has no tables", "[syzygy][uci]") {
    chess::UciOptions options;
    options.syzygy_path = "try4-no-such-tablebase-directory";
    const std::string out = RunUciSession("uci\nquit\n", options);
    REQUIRE(out.find("info string Syzygy load failed:") != std::string::npos);
    REQUIRE(out.find("uciok") != std::string::npos);
}
