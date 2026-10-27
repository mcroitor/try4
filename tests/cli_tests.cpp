#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

#include "chess/cli.hpp"

namespace {

struct SessionResult {
    int code = 0;
    std::string output;
};

SessionResult RunSession(const std::string& script) {
    std::istringstream input(script);
    std::ostringstream output;
    chess::Cli cli(input, output);
    const int code = cli.Run();
    return {code, output.str()};
}

}  // namespace

TEST_CASE("CLI runs a scripted move and inspection session", "[cli]") {
    const SessionResult session = RunSession(
        "startpos\n"
        "move e2e4\n"
        "move e7e5\n"
        "undo\n"
        "move e2e5\n"
        "result\n"
        "moves\n"
        "fen 7k/5Q2/6K1/8/8/8/8/8 b - - 0 1\n"
        "result\n"
        "perft 1\n"
        "undo\n"
        "quit\n");

    REQUIRE(session.code == 0);
    REQUIRE(session.output.find("ok e2e4 e4\n") != std::string::npos);
    REQUIRE(session.output.find("ok e7e5 e5\n") != std::string::npos);
    REQUIRE(session.output.find("error: illegal or ambiguous move\n") !=
            std::string::npos);
    REQUIRE(session.output.find("\nongoing\n") != std::string::npos);
    REQUIRE(session.output.find("e7e5 e5\n") != std::string::npos);
    REQUIRE(session.output.find("stalemate\n") != std::string::npos);
    REQUIRE(session.output.find("perft 1 0\n") != std::string::npos);
    REQUIRE(session.output.find("error: no moves to undo\n") !=
            std::string::npos);
}

TEST_CASE("CLI accepts SAN and prints a board", "[cli]") {
    const SessionResult session = RunSession(
        "move Nf3\n"
        "move Nf6\n"
        "board\n"
        "quit\n");

    REQUIRE(session.code == 0);
    REQUIRE(session.output.find("ok g1f3 Nf3\n") != std::string::npos);
    REQUIRE(session.output.find("ok g8f6 Nf6\n") != std::string::npos);
    REQUIRE(session.output.find("rnbqkb r") != std::string::npos);
}

TEST_CASE("CLI reports errors without terminating", "[cli]") {
    const SessionResult session = RunSession(
        "fen not-a-fen\n"
        "result\n"
        "perft nope\n"
        "move e2e5\n"
        "move e2e4q\n"
        "bogus\n"
        "quit\n");

    REQUIRE(session.code == 0);
    REQUIRE(session.output.find("error: FEN must contain six") !=
            std::string::npos);
    REQUIRE(session.output.find("ongoing\n") != std::string::npos);
    REQUIRE(session.output.find("error: perft depth must be") !=
            std::string::npos);
    REQUIRE(session.output.find("error: illegal or ambiguous move\n") !=
            std::string::npos);
    REQUIRE(session.output.find("error: unknown command 'bogus'\n") !=
            std::string::npos);
}

TEST_CASE("CLI perft uses the current position", "[cli]") {
    const SessionResult session = RunSession(
        "startpos\n"
        "perft 2\n"
        "perft 3\n"
        "quit\n");

    REQUIRE(session.code == 0);
    REQUIRE(session.output.find("perft 2 400\n") != std::string::npos);
    REQUIRE(session.output.find("perft 3 8902\n") != std::string::npos);
}
