#include <catch2/catch_test_macros.hpp>

#include <string>

#include "chess/position.hpp"
#include "test_support.hpp"

namespace {

chess::Position FromFen(const char* fen) {
    std::string error;
    const auto position = chess::Position::FromFen(fen, &error);
    INFO("FEN: " << fen);
    INFO("error: " << error);
    REQUIRE(position.has_value());
    return *position;
}

void Play(chess::Position* position, const char* from, const char* to) {
    REQUIRE(position->MakeMove(try4_test::Move(from, to)));
}

}  // namespace

TEST_CASE("Fool's mate is checkmate for the side to move", "[result]") {
    chess::Position position;
    position.SetupInitial();

    Play(&position, "f2", "f3");
    Play(&position, "e7", "e5");
    Play(&position, "g2", "g4");
    Play(&position, "d8", "h4");

    REQUIRE(position.SideToMove() == chess::Color::White);
    REQUIRE(position.InCheck(chess::Color::White));
    REQUIRE(position.LegalMoves().empty());
    REQUIRE(position.Result() == chess::GameResult::Checkmate);
}

TEST_CASE("Scholar's mate is checkmate", "[result]") {
    chess::Position position;
    position.SetupInitial();

    Play(&position, "e2", "e4");
    Play(&position, "e7", "e5");
    Play(&position, "f1", "c4");
    Play(&position, "b8", "c6");
    Play(&position, "d1", "h5");
    Play(&position, "g8", "f6");
    Play(&position, "h5", "f7");

    REQUIRE(position.SideToMove() == chess::Color::Black);
    REQUIRE(position.InCheck(chess::Color::Black));
    REQUIRE(position.Result() == chess::GameResult::Checkmate);
}

TEST_CASE("back-rank mate is detected", "[result]") {
    chess::Position position =
        FromFen("6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1");

    REQUIRE(position.MakeMove(try4_test::Move("a1", "a8")));
    REQUIRE(position.SideToMove() == chess::Color::Black);
    REQUIRE(position.InCheck(chess::Color::Black));
    REQUIRE(position.LegalMoves().empty());
    REQUIRE(position.Result() == chess::GameResult::Checkmate);
}

TEST_CASE("known stalemate position is not checkmate", "[result]") {
    chess::Position position =
        FromFen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");

    REQUIRE(position.SideToMove() == chess::Color::Black);
    REQUIRE_FALSE(position.InCheck(chess::Color::Black));
    REQUIRE(position.LegalMoves().empty());
    REQUIRE(position.Result() == chess::GameResult::Stalemate);
}

TEST_CASE("check with an escape is ongoing", "[result]") {
    chess::Position position =
        FromFen("4k3/8/8/8/8/8/4r3/4K3 w - - 0 1");

    REQUIRE(position.SideToMove() == chess::Color::White);
    REQUIRE(position.InCheck(chess::Color::White));
    REQUIRE_FALSE(position.LegalMoves().empty());
    REQUIRE(position.Result() == chess::GameResult::Ongoing);
}

TEST_CASE("ordinary positions are ongoing", "[result]") {
    chess::Position position;
    position.SetupInitial();

    REQUIRE_FALSE(position.InCheck(chess::Color::White));
    REQUIRE(position.Result() == chess::GameResult::Ongoing);

    REQUIRE(position.MakeMove(try4_test::Move("e2", "e4")));
    REQUIRE(position.SideToMove() == chess::Color::Black);
    REQUIRE(position.Result() == chess::GameResult::Ongoing);
}
