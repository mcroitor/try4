#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>

#include "chess/position.hpp"
#include "test_support.hpp"

namespace {

chess::Position EmptyPosition() {
    chess::Position position;
    position.Clear();
    return position;
}

chess::Piece White(chess::PieceType type) {
    return chess::Piece(chess::Color::White, type);
}

chess::Piece Black(chess::PieceType type) {
    return chess::Piece(chess::Color::Black, type);
}

}  // namespace

TEST_CASE("pawns attack diagonally for both colors", "[attack]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("e4"), White(chess::PieceType::Pawn));
    position.SetPiece(try4_test::Sq("e5"), Black(chess::PieceType::Pawn));

    REQUIRE(position.IsSquareAttacked(try4_test::Sq("d5"),
                                      chess::Color::White));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("f5"),
                                      chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("e5"),
                                            chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("d4"),
                                            chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("d5"),
                                            chess::Color::Black));

    REQUIRE(position.IsSquareAttacked(try4_test::Sq("d4"),
                                      chess::Color::Black));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("f4"),
                                      chess::Color::Black));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("e4"),
                                            chess::Color::Black));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("d4"),
                                            chess::Color::White));
}

TEST_CASE("pawn attacks respect board edges", "[attack]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("a2"), White(chess::PieceType::Pawn));
    position.SetPiece(try4_test::Sq("h2"), White(chess::PieceType::Pawn));
    position.SetPiece(try4_test::Sq("a7"), Black(chess::PieceType::Pawn));
    position.SetPiece(try4_test::Sq("h7"), Black(chess::PieceType::Pawn));

    REQUIRE(position.IsSquareAttacked(try4_test::Sq("b3"),
                                      chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("a3"),
                                            chess::Color::White));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("g3"),
                                      chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("h3"),
                                            chess::Color::White));

    REQUIRE(position.IsSquareAttacked(try4_test::Sq("b6"),
                                      chess::Color::Black));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("a6"),
                                            chess::Color::Black));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("g6"),
                                      chess::Color::Black));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("h6"),
                                            chess::Color::Black));
}

TEST_CASE("knights attack all eight offsets and respect edges", "[attack]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("d4"), White(chess::PieceType::Knight));

    constexpr const char* kTargets[] = {"b3", "b5", "c2", "c6",
                                        "e2", "e6", "f3", "f5"};
    for (const char* target : kTargets) {
        INFO("target: " << target);
        REQUIRE(position.IsSquareAttacked(try4_test::Sq(target),
                                          chess::Color::White));
        REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq(target),
                                                chess::Color::Black));
    }

    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("d5"),
                                            chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("e4"),
                                            chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("a1"),
                                            chess::Color::White));

    position.Clear();
    position.SetPiece(try4_test::Sq("a1"), Black(chess::PieceType::Knight));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("b3"),
                                      chess::Color::Black));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("c2"),
                                      chess::Color::Black));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("c3"),
                                            chess::Color::Black));

    position.Clear();
    position.SetPiece(try4_test::Sq("h8"), White(chess::PieceType::Knight));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("g6"),
                                      chess::Color::White));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("f7"),
                                      chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("g8"),
                                            chess::Color::White));
}

TEST_CASE("kings attack adjacent squares and respect edges", "[attack]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("e1"), White(chess::PieceType::King));

    constexpr const char* kTargets[] = {"d1", "d2", "e2", "f1", "f2"};
    for (const char* target : kTargets) {
        REQUIRE(position.IsSquareAttacked(try4_test::Sq(target),
                                          chess::Color::White));
    }
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("e3"),
                                            chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("d3"),
                                            chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("e1"),
                                            chess::Color::White));

    position.Clear();
    position.SetPiece(try4_test::Sq("a1"),
                      Black(chess::PieceType::King));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("a2"),
                                      chess::Color::Black));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("b1"),
                                      chess::Color::Black));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("b2"),
                                      chess::Color::Black));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("c1"),
                                            chess::Color::Black));

    position.Clear();
    position.SetPiece(try4_test::Sq("h8"), White(chess::PieceType::King));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("g7"),
                                      chess::Color::White));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("g8"),
                                      chess::Color::White));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("h7"),
                                      chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("f8"),
                                            chess::Color::White));
}

TEST_CASE("bishops slide diagonally until blocked", "[attack]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("c1"), White(chess::PieceType::Bishop));

    constexpr const char* kTargets[] = {"b2", "a3", "d2", "e3",
                                        "f4", "g5", "h6"};
    for (const char* target : kTargets) {
        REQUIRE(position.IsSquareAttacked(try4_test::Sq(target),
                                          chess::Color::White));
    }
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("d3"),
                                            chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("b1"),
                                            chess::Color::White));

    position.SetPiece(try4_test::Sq("e3"), White(chess::PieceType::Knight));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("d2"),
                                      chess::Color::White));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("e3"),
                                      chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("f4"),
                                            chess::Color::White));

    position.Clear();
    position.SetPiece(try4_test::Sq("h1"), White(chess::PieceType::Bishop));
    constexpr const char* kDiagonal[] = {"g2", "f3", "e4", "d5",
                                         "c6", "b7", "a8"};
    for (const char* target : kDiagonal) {
        REQUIRE(position.IsSquareAttacked(try4_test::Sq(target),
                                          chess::Color::White));
    }
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("h2"),
                                            chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("b1"),
                                            chess::Color::White));
}

TEST_CASE("rooks slide orthogonally until blocked", "[attack]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("a1"), White(chess::PieceType::Rook));

    for (int rank = 2; rank <= 8; ++rank) {
        const std::string target = std::string("a") +
                                   static_cast<char>('0' + rank);
        REQUIRE(position.IsSquareAttacked(try4_test::Sq(target),
                                          chess::Color::White));
    }
    for (char file = 'b'; file <= 'h'; ++file) {
        const std::string target = std::string(1, file) + "1";
        REQUIRE(position.IsSquareAttacked(try4_test::Sq(target),
                                          chess::Color::White));
    }
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("b2"),
                                            chess::Color::White));

    position.SetPiece(try4_test::Sq("a4"), White(chess::PieceType::Pawn));
    position.SetPiece(try4_test::Sq("d1"), Black(chess::PieceType::Pawn));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("a3"),
                                      chess::Color::White));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("a4"),
                                      chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("a5"),
                                            chess::Color::White));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("c1"),
                                      chess::Color::White));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("d1"),
                                      chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("e1"),
                                            chess::Color::White));
}

TEST_CASE("queens combine both ray families", "[attack]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("d4"), White(chess::PieceType::Queen));

    REQUIRE(position.IsSquareAttacked(try4_test::Sq("d8"),
                                      chess::Color::White));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("d1"),
                                      chess::Color::White));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("a4"),
                                      chess::Color::White));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("h4"),
                                      chess::Color::White));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("a1"),
                                      chess::Color::White));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("h8"),
                                      chess::Color::White));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("a7"),
                                      chess::Color::White));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("g1"),
                                      chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("e2"),
                                            chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("b3"),
                                            chess::Color::White));

    position.Clear();
    position.SetPiece(try4_test::Sq("h8"), White(chess::PieceType::Queen));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("a8"),
                                      chess::Color::White));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("h1"),
                                      chess::Color::White));
    REQUIRE(position.IsSquareAttacked(try4_test::Sq("a1"),
                                      chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("a7"),
                                            chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(try4_test::Sq("g1"),
                                            chess::Color::White));
}

TEST_CASE("attack queries ignore the target occupant and invalid squares",
          "[attack]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("a1"), White(chess::PieceType::Rook));
    position.SetPiece(try4_test::Sq("a4"), White(chess::PieceType::Pawn));

    REQUIRE(position.IsSquareAttacked(try4_test::Sq("a4"),
                                      chess::Color::White));
    REQUIRE_FALSE(position.IsSquareAttacked(
        chess::Square(static_cast<std::uint8_t>(chess::Square::kInvalidIndex)),
        chess::Color::White));
}

TEST_CASE("InCheck detects attacks on the king", "[check]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("e1"), White(chess::PieceType::King));
    position.SetPiece(try4_test::Sq("h8"), Black(chess::PieceType::King));
    position.SetPiece(try4_test::Sq("e8"), Black(chess::PieceType::Rook));

    REQUIRE(position.InCheck(chess::Color::White));
    REQUIRE_FALSE(position.InCheck(chess::Color::Black));

    position.SetPiece(try4_test::Sq("e4"), White(chess::PieceType::Pawn));
    REQUIRE_FALSE(position.InCheck(chess::Color::White));

    position.Clear();
    position.SetPiece(try4_test::Sq("e1"), White(chess::PieceType::King));
    position.SetPiece(try4_test::Sq("h8"), Black(chess::PieceType::King));
    position.SetPiece(try4_test::Sq("b4"), Black(chess::PieceType::Bishop));
    REQUIRE(position.InCheck(chess::Color::White));

    position.Clear();
    position.SetPiece(try4_test::Sq("e1"), White(chess::PieceType::King));
    position.SetPiece(try4_test::Sq("h8"), Black(chess::PieceType::King));
    position.SetPiece(try4_test::Sq("f3"), Black(chess::PieceType::Knight));
    REQUIRE(position.InCheck(chess::Color::White));

    position.Clear();
    position.SetPiece(try4_test::Sq("e1"), White(chess::PieceType::King));
    position.SetPiece(try4_test::Sq("h8"), Black(chess::PieceType::King));
    position.SetPiece(try4_test::Sq("d2"), Black(chess::PieceType::Pawn));
    REQUIRE(position.InCheck(chess::Color::White));

    position.Clear();
    position.SetPiece(try4_test::Sq("e1"), White(chess::PieceType::King));
    position.SetPiece(try4_test::Sq("h8"), Black(chess::PieceType::King));
    position.SetPiece(try4_test::Sq("h4"), Black(chess::PieceType::Queen));
    REQUIRE(position.InCheck(chess::Color::White));
}

TEST_CASE("InCheck evaluates the requested side", "[check]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("e8"), Black(chess::PieceType::King));
    position.SetPiece(try4_test::Sq("a1"), White(chess::PieceType::King));
    position.SetPiece(try4_test::Sq("e1"), White(chess::PieceType::Rook));

    REQUIRE_FALSE(position.InCheck(chess::Color::White));
    REQUIRE(position.InCheck(chess::Color::Black));
}
