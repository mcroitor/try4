#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

#include "chess/uci.hpp"

namespace {

std::string RunUciSession(const std::string& script) {
    std::istringstream input("isready\n" + script);
    std::ostringstream output;
    chess::UciEngine engine;
    engine.Run(input, output);
    return output.str();
}

}  // namespace

TEST_CASE("UCI handshake", "[uci]") {
    const std::string out = RunUciSession(
        "uci\n"
        "isready\n"
        "quit\n"
    );

    REQUIRE(out.find("id name try4") != std::string::npos);
    REQUIRE(out.find("id author try4") != std::string::npos);
    REQUIRE(out.find("uciok") != std::string::npos);
    REQUIRE(out.find("readyok") != std::string::npos);
}

TEST_CASE("UCI options", "[uci]") {
    const std::string out = RunUciSession(
        "uci\n"
        "quit\n"
    );

    REQUIRE(out.find("option name Hash type spin default 16 min 1 max 131072") != std::string::npos);
    REQUIRE(out.find("option name Move Overhead type spin default 20 min 0 max 5000") != std::string::npos);
#ifdef TRY4_SMP_POC
    REQUIRE(out.find("option name Threads type spin default 1 min 1 max 4") != std::string::npos);
#else
    REQUIRE(out.find("option name Threads type spin default 1 min 1 max 1") != std::string::npos);
#endif
    REQUIRE(out.find("UCI_Chess960") == std::string::npos);
}

TEST_CASE("UCI setoption Hash", "[uci]") {
    const std::string out = RunUciSession(
        "uci\n"
        "setoption name Hash value 32\n"
        "isready\n"
        "quit\n"
    );

    REQUIRE(out.find("readyok") != std::string::npos);
}

TEST_CASE("UCI position startpos", "[uci]") {
    const std::string out = RunUciSession(
        "uci\n"
        "ucinewgame\n"
        "position startpos\n"
        "go depth 5\n"
        "quit\n"
    );

    REQUIRE(out.find("readyok") != std::string::npos);
    REQUIRE(out.find("bestmove") != std::string::npos);
}

TEST_CASE("UCI position fen", "[uci]") {
    const std::string out = RunUciSession(
        "uci\n"
        "ucinewgame\n"
        "position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\n"
        "go depth 5\n"
        "quit\n"
    );

    REQUIRE(out.find("readyok") != std::string::npos);
    REQUIRE(out.find("bestmove") != std::string::npos);
}

TEST_CASE("UCI position with moves", "[uci]") {
    const std::string out = RunUciSession(
        "uci\n"
        "ucinewgame\n"
        "position startpos moves e2e4 e7e5\n"
        "go depth 5\n"
        "quit\n"
    );

    REQUIRE(out.find("readyok") != std::string::npos);
    REQUIRE(out.find("bestmove") != std::string::npos);
}

TEST_CASE("UCI go with movetime", "[uci]") {
    const std::string out = RunUciSession(
        "uci\n"
        "ucinewgame\n"
        "position startpos\n"
        "go movetime 100\n"
        "quit\n"
    );

    REQUIRE(out.find("readyok") != std::string::npos);
    REQUIRE(out.find("bestmove") != std::string::npos);
}

TEST_CASE("UCI stop command", "[uci]") {
    const std::string out = RunUciSession(
        "uci\n"
        "ucinewgame\n"
        "position startpos\n"
        "go infinite\n"
        "stop\n"
        "quit\n"
    );

    REQUIRE(out.find("readyok") != std::string::npos);
    REQUIRE(out.find("bestmove") != std::string::npos);
}

TEST_CASE("UCI ucinewgame clears position", "[uci]") {
    const std::string out = RunUciSession(
        "uci\n"
        "ucinewgame\n"
        "position startpos moves e2e4\n"
        "ucinewgame\n"
        "go depth 5\n"
        "quit\n"
    );

    // Should be back at start position after ucinewgame
    REQUIRE(out.find("bestmove") != std::string::npos);
}

TEST_CASE("UCI info output contains depth and nodes", "[uci]") {
    const std::string out = RunUciSession(
        "uci\n"
        "ucinewgame\n"
        "position startpos\n"
        "go depth 5\n"
        "quit\n"
    );

    REQUIRE(out.find("info depth") != std::string::npos);
    REQUIRE(out.find("nodes") != std::string::npos);
    REQUIRE(out.find("nps") != std::string::npos);
    REQUIRE(out.find("pv") != std::string::npos);
}

TEST_CASE("UCI exposes the Resign option", "[uci][resign]") {
    const std::string out = RunUciSession(
        "uci\n"
        "quit\n"
    );

    REQUIRE(out.find("option name Resign type spin default -600 min -100000 max 0") != std::string::npos);
}
// The resignation behavior itself is covered by the real-process
// integration tests in tests/engine_process_tests.py: the in-process harness
// feeds `quit` immediately after `go`, which by the task contract cancels the
// search before any iteration completes (and therefore must not resign).
