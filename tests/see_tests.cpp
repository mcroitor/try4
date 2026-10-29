#include <catch2/catch_test_macros.hpp>

#include <string>

#include "chess/evaluation.hpp"
#include "chess/see.hpp"
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

}  // namespace

// Task 034 requirement 4: SEE must be tested against known capture
// sequences. Every case below is hand-derived.
TEST_CASE("SEE evaluates known capture sequences", "[see]") {
    const chess::EvaluationParameters parameters;
    // Rook takes an undefended knight: +320.
    CHECK(chess::See(FromFen("4k3/8/8/3n4/8/8/8/3RK3 w - - 0 1"),
                     try4_test::Move("d1", "d5", chess::MoveFlag::Capture),
                     parameters) == 320);

    // Queen takes an undefended rook: +500.
    CHECK(chess::See(FromFen("4k3/8/8/3r4/8/8/8/3QK3 w - - 0 1"),
                     try4_test::Move("d1", "d5", chess::MoveFlag::Capture),
                     parameters) == 500);

    // Rook takes a pawn defended by the black rook on d6: -500 + 100 = -400.
    CHECK(chess::See(FromFen("4k3/8/3r4/3p4/8/8/8/3RK3 w - - 0 1"),
                     try4_test::Move("d1", "d5", chess::MoveFlag::Capture),
                     parameters) == -400);

    // Bishop takes a black bishop that the black c3 pawn defends: 0.
    CHECK(chess::See(FromFen("4k3/8/8/8/8/2p5/1b6/2B1K3 w - - 0 1"),
                     try4_test::Move("c1", "b2", chess::MoveFlag::Capture),
                     parameters) == 0);

    // A pawn takes a knight the black d7 bishop defends: +320 - 100 = +220.
    CHECK(chess::See(FromFen("4k3/3b4/4n3/3P4/8/8/8/4K3 w - - 0 1"),
                     try4_test::Move("d5", "e6", chess::MoveFlag::Capture),
                     parameters) == 220);

    // X-ray chain: the white b2-rook takes the black queen on b6; the black
    // b8-rook recaptures; the stacked b1-rook is freed when b2 empties and
    // recaptures: +900 - 500 + 500 = +900.
    CHECK(chess::See(FromFen("1r4k1/8/1q6/8/8/8/1R6/1R4K1 w - - 0 1"),
                     try4_test::Move("b2", "b6", chess::MoveFlag::Capture),
                     parameters) == 900);
}

TEST_CASE("SEE rejects non-captures and promotions", "[see]") {
    const chess::EvaluationParameters parameters;
    // A rook to an empty square is not an exchange.
    CHECK(chess::See(FromFen("4k3/8/8/8/8/8/8/3RK3 w - - 0 1"),
                     try4_test::Move("d1", "d2"), parameters) == 0);
    // A promotion capture is out of SEE scope; callers never prune those.
    CHECK(chess::See(FromFen("3b3k/2P5/8/8/8/8/8/4K3 w - - 0 1"),
                     try4_test::Move("c7", "d8",
                                     chess::MoveFlag::Capture |
                                         chess::MoveFlag::Promotion,
                                     chess::PieceType::Queen),
                     parameters) == 0);
}