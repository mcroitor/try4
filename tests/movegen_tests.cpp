#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <optional>
#include <set>
#include <string>

#include "chess/movegen.hpp"
#include "chess/position.hpp"
#include "test_support.hpp"

namespace {

chess::Position EmptyPosition() {
    chess::Position position;
    position.Clear();
    position.SetSideToMove(chess::Color::White);
    return position;
}

chess::Piece White(chess::PieceType type) {
    return chess::Piece(chess::Color::White, type);
}

chess::Piece Black(chess::PieceType type) {
    return chess::Piece(chess::Color::Black, type);
}

bool HasMove(const chess::MoveList& moves, const char* from, const char* to,
             chess::MoveFlag flags = chess::MoveFlag::None,
             chess::PieceType promotion = chess::PieceType::None) {
    const chess::Move expected(try4_test::Sq(from), try4_test::Sq(to), flags,
                               promotion);
    for (const chess::Move& move : moves) {
        if (move == expected) {
            return true;
        }
    }
    return false;
}

bool HasMoveFromTo(const chess::MoveList& moves, const char* from,
                   const char* to) {
    const chess::Square from_square = try4_test::Sq(from);
    const chess::Square to_square = try4_test::Sq(to);
    for (const chess::Move& move : moves) {
        if (move.from == from_square && move.to == to_square) {
            return true;
        }
    }
    return false;
}

std::size_t CountFrom(const chess::MoveList& moves, const char* from) {
    const chess::Square square = try4_test::Sq(from);
    std::size_t count = 0;
    for (const chess::Move& move : moves) {
        if (move.from == square) {
            ++count;
        }
    }
    return count;
}

std::size_t CountFlag(const chess::MoveList& moves, chess::MoveFlag flag) {
    std::size_t count = 0;
    for (const chess::Move& move : moves) {
        if (move.HasFlag(flag)) {
            ++count;
        }
    }
    return count;
}

void RequireNoExactDuplicates(const chess::MoveList& moves) {
    std::set<std::string> keys;
    for (const chess::Move& move : moves) {
        const std::string key =
            move.ToUci() + "/" +
            std::to_string(static_cast<int>(move.flags)) + "/" +
            std::to_string(static_cast<int>(move.promotion));
        REQUIRE(keys.insert(key).second);
    }
}

}  // namespace

TEST_CASE("initial position has 20 pseudo-legal moves", "[movegen]") {
    chess::Position position;
    position.SetupInitial();
    chess::MoveList moves;
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);

    REQUIRE(moves.Size() == 20);
    REQUIRE(CountFlag(moves, chess::MoveFlag::DoublePush) == 8);
    REQUIRE(CountFlag(moves, chess::MoveFlag::Capture) == 0);
    REQUIRE_FALSE(HasMoveFromTo(moves, "e1", "g1"));
    REQUIRE_FALSE(HasMoveFromTo(moves, "e1", "c1"));
    RequireNoExactDuplicates(moves);
}

TEST_CASE("pawn pushes and double pushes set the correct flags", "[movegen]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("e2"), White(chess::PieceType::Pawn));

    chess::MoveList moves;
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);

    REQUIRE(moves.Size() == 2);
    REQUIRE(HasMove(moves, "e2", "e3"));
    REQUIRE(HasMove(moves, "e2", "e4", chess::MoveFlag::DoublePush));
}

TEST_CASE("blocked pawns cannot jump", "[movegen]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("e2"), White(chess::PieceType::Pawn));
    position.SetPiece(try4_test::Sq("e3"), White(chess::PieceType::Knight));

    chess::MoveList moves;
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);

    REQUIRE_FALSE(HasMoveFromTo(moves, "e2", "e3"));
    REQUIRE_FALSE(HasMoveFromTo(moves, "e2", "e4"));

    position.Clear();
    position.SetSideToMove(chess::Color::White);
    position.SetPiece(try4_test::Sq("e2"), White(chess::PieceType::Pawn));
    position.SetPiece(try4_test::Sq("e4"), Black(chess::PieceType::Knight));

    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE(HasMove(moves, "e2", "e3"));
    REQUIRE_FALSE(HasMoveFromTo(moves, "e2", "e4"));
}

TEST_CASE("pawns capture diagonally and never forward", "[movegen]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("e4"), White(chess::PieceType::Pawn));
    position.SetPiece(try4_test::Sq("d5"), Black(chess::PieceType::Knight));
    position.SetPiece(try4_test::Sq("f5"), Black(chess::PieceType::Rook));

    chess::MoveList moves;
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);

    REQUIRE(HasMove(moves, "e4", "d5", chess::MoveFlag::Capture));
    REQUIRE(HasMove(moves, "e4", "f5", chess::MoveFlag::Capture));
    REQUIRE(HasMove(moves, "e4", "e5"));

    position.Clear();
    position.SetSideToMove(chess::Color::White);
    position.SetPiece(try4_test::Sq("e4"), White(chess::PieceType::Pawn));
    position.SetPiece(try4_test::Sq("e5"), Black(chess::PieceType::Knight));

    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE(moves.Empty());
}

TEST_CASE("promotions generate all four pieces for quiet and capture moves",
          "[movegen]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("e7"), White(chess::PieceType::Pawn));
    position.SetPiece(try4_test::Sq("d8"), Black(chess::PieceType::Rook));
    position.SetPiece(try4_test::Sq("f8"), Black(chess::PieceType::Rook));

    chess::MoveList moves;
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);

    REQUIRE(moves.Size() == 12);
    REQUIRE(CountFrom(moves, "e7") == 12);
    REQUIRE(CountFlag(moves, chess::MoveFlag::Promotion) == 12);
    REQUIRE(CountFlag(moves, chess::MoveFlag::Capture) == 8);

    constexpr chess::PieceType kPieces[] = {
        chess::PieceType::Knight,
        chess::PieceType::Bishop,
        chess::PieceType::Rook,
        chess::PieceType::Queen,
    };
    for (const chess::PieceType piece : kPieces) {
        REQUIRE(HasMove(moves, "e7", "e8", chess::MoveFlag::Promotion, piece));
        REQUIRE(HasMove(moves, "e7", "d8",
                        chess::MoveFlag::Capture | chess::MoveFlag::Promotion,
                        piece));
        REQUIRE(HasMove(moves, "e7", "f8",
                        chess::MoveFlag::Capture | chess::MoveFlag::Promotion,
                        piece));
    }
    RequireNoExactDuplicates(moves);
}

TEST_CASE("black pawns promote on the first rank", "[movegen]") {
    chess::Position position = EmptyPosition();
    position.SetSideToMove(chess::Color::Black);
    position.SetPiece(try4_test::Sq("e2"), Black(chess::PieceType::Pawn));
    position.SetPiece(try4_test::Sq("d1"), White(chess::PieceType::Rook));
    position.SetPiece(try4_test::Sq("f1"), White(chess::PieceType::Rook));

    chess::MoveList moves;
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);

    REQUIRE(moves.Size() == 12);
    REQUIRE(CountFlag(moves, chess::MoveFlag::Promotion) == 12);
    REQUIRE(CountFlag(moves, chess::MoveFlag::Capture) == 8);
    REQUIRE(HasMove(moves, "e2", "e1", chess::MoveFlag::Promotion,
                    chess::PieceType::Queen));
    REQUIRE(HasMove(moves, "e2", "d1",
                    chess::MoveFlag::Capture | chess::MoveFlag::Promotion,
                    chess::PieceType::Knight));
}

TEST_CASE("knights respect edges and occupancy", "[movegen]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("a1"), White(chess::PieceType::Knight));

    chess::MoveList moves;
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE(moves.Size() == 2);
    REQUIRE(HasMove(moves, "a1", "b3"));
    REQUIRE(HasMove(moves, "a1", "c2"));

    position.Clear();
    position.SetSideToMove(chess::Color::White);
    position.SetPiece(try4_test::Sq("h8"), White(chess::PieceType::Knight));
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE(moves.Size() == 2);
    REQUIRE(HasMove(moves, "h8", "f7"));
    REQUIRE(HasMove(moves, "h8", "g6"));

    position.Clear();
    position.SetSideToMove(chess::Color::White);
    position.SetPiece(try4_test::Sq("d4"), White(chess::PieceType::Knight));
    position.SetPiece(try4_test::Sq("e6"), White(chess::PieceType::Pawn));
    position.SetPiece(try4_test::Sq("f5"), Black(chess::PieceType::Pawn));
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE_FALSE(HasMoveFromTo(moves, "d4", "e6"));
    REQUIRE(HasMove(moves, "d4", "f5", chess::MoveFlag::Capture));
}

TEST_CASE("kings respect edges and occupancy", "[movegen]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("a1"), White(chess::PieceType::King));

    chess::MoveList moves;
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE(moves.Size() == 3);
    REQUIRE(HasMove(moves, "a1", "a2"));
    REQUIRE(HasMove(moves, "a1", "b1"));
    REQUIRE(HasMove(moves, "a1", "b2"));

    position.Clear();
    position.SetSideToMove(chess::Color::White);
    position.SetPiece(try4_test::Sq("h8"), White(chess::PieceType::King));
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE(moves.Size() == 3);
    REQUIRE(HasMove(moves, "h8", "g7"));
    REQUIRE(HasMove(moves, "h8", "g8"));
    REQUIRE(HasMove(moves, "h8", "h7"));

    position.Clear();
    position.SetSideToMove(chess::Color::White);
    position.SetPiece(try4_test::Sq("e1"), White(chess::PieceType::King));
    position.SetPiece(try4_test::Sq("e2"), White(chess::PieceType::Pawn));
    position.SetPiece(try4_test::Sq("d2"), Black(chess::PieceType::Pawn));
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE_FALSE(HasMoveFromTo(moves, "e1", "e2"));
    REQUIRE(HasMove(moves, "e1", "d2", chess::MoveFlag::Capture));
}

TEST_CASE("bishops slide until a blocker", "[movegen]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("d4"), White(chess::PieceType::Bishop));

    chess::MoveList moves;
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE(moves.Size() == 13);
    REQUIRE(CountFrom(moves, "d4") == 13);

    position.Clear();
    position.SetSideToMove(chess::Color::White);
    position.SetPiece(try4_test::Sq("c1"), White(chess::PieceType::Bishop));
    position.SetPiece(try4_test::Sq("e3"), White(chess::PieceType::Knight));
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE(CountFrom(moves, "c1") == 3);
    REQUIRE(HasMove(moves, "c1", "b2"));
    REQUIRE(HasMove(moves, "c1", "a3"));
    REQUIRE(HasMove(moves, "c1", "d2"));
    REQUIRE_FALSE(HasMoveFromTo(moves, "c1", "e3"));
    REQUIRE_FALSE(HasMoveFromTo(moves, "c1", "f4"));

    position.Clear();
    position.SetSideToMove(chess::Color::White);
    position.SetPiece(try4_test::Sq("c1"), White(chess::PieceType::Bishop));
    position.SetPiece(try4_test::Sq("e3"), Black(chess::PieceType::Knight));
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE(CountFrom(moves, "c1") == 4);
    REQUIRE(HasMove(moves, "c1", "e3", chess::MoveFlag::Capture));
    REQUIRE_FALSE(HasMoveFromTo(moves, "c1", "f4"));
}

TEST_CASE("rooks slide until a blocker", "[movegen]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("d4"), White(chess::PieceType::Rook));

    chess::MoveList moves;
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE(moves.Size() == 14);
    REQUIRE(CountFrom(moves, "d4") == 14);

    position.Clear();
    position.SetSideToMove(chess::Color::White);
    position.SetPiece(try4_test::Sq("a1"), White(chess::PieceType::Rook));
    position.SetPiece(try4_test::Sq("a4"), White(chess::PieceType::Pawn));
    position.SetPiece(try4_test::Sq("d1"), Black(chess::PieceType::Pawn));
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE(CountFrom(moves, "a1") == 5);
    REQUIRE(HasMove(moves, "a1", "a2"));
    REQUIRE(HasMove(moves, "a1", "a3"));
    REQUIRE_FALSE(HasMoveFromTo(moves, "a1", "a4"));
    REQUIRE(HasMove(moves, "a1", "d1", chess::MoveFlag::Capture));
    REQUIRE_FALSE(HasMoveFromTo(moves, "a1", "e1"));
}

TEST_CASE("queens combine both ray families without duplicates", "[movegen]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("d4"), White(chess::PieceType::Queen));

    chess::MoveList moves;
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE(moves.Size() == 27);
    REQUIRE(CountFrom(moves, "d4") == 27);
    RequireNoExactDuplicates(moves);

    position.Clear();
    position.SetSideToMove(chess::Color::White);
    position.SetPiece(try4_test::Sq("a1"), White(chess::PieceType::Queen));
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE(moves.Size() == 21);
    REQUIRE_FALSE(HasMoveFromTo(moves, "a1", "b8"));
}

TEST_CASE("sliding pieces do not wrap at board edges", "[movegen]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("h1"), White(chess::PieceType::Bishop));

    chess::MoveList moves;
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE(moves.Size() == 7);
    REQUIRE(HasMove(moves, "h1", "g2"));
    REQUIRE(HasMove(moves, "h1", "a8"));
    REQUIRE_FALSE(HasMoveFromTo(moves, "h1", "h2"));
    REQUIRE_FALSE(HasMoveFromTo(moves, "h1", "a1"));

    position.Clear();
    position.SetSideToMove(chess::Color::White);
    position.SetPiece(try4_test::Sq("a8"), White(chess::PieceType::Rook));
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE(moves.Size() == 14);
    REQUIRE(HasMove(moves, "a8", "h8"));
    REQUIRE(HasMove(moves, "a8", "a1"));
    REQUIRE_FALSE(HasMoveFromTo(moves, "a8", "h1"));
}

TEST_CASE("castling is generated for both white sides", "[movegen]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("e1"), White(chess::PieceType::King));
    position.SetPiece(try4_test::Sq("a1"), White(chess::PieceType::Rook));
    position.SetPiece(try4_test::Sq("h1"), White(chess::PieceType::Rook));
    position.SetPiece(try4_test::Sq("e8"), Black(chess::PieceType::King));
    position.SetCastlingRights(chess::kAllCastlingRights);

    chess::MoveList moves;
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);

    REQUIRE(HasMove(moves, "e1", "g1", chess::MoveFlag::CastleKing));
    REQUIRE(HasMove(moves, "e1", "c1", chess::MoveFlag::CastleQueen));
    REQUIRE(CountFrom(moves, "e1") == 7);
    RequireNoExactDuplicates(moves);
}

TEST_CASE("castling is generated for both black sides", "[movegen]") {
    chess::Position position = EmptyPosition();
    position.SetSideToMove(chess::Color::Black);
    position.SetPiece(try4_test::Sq("e8"), Black(chess::PieceType::King));
    position.SetPiece(try4_test::Sq("a8"), Black(chess::PieceType::Rook));
    position.SetPiece(try4_test::Sq("h8"), Black(chess::PieceType::Rook));
    position.SetPiece(try4_test::Sq("e1"), White(chess::PieceType::King));
    position.SetCastlingRights(chess::kAllCastlingRights);

    chess::MoveList moves;
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);

    REQUIRE(HasMove(moves, "e8", "g8", chess::MoveFlag::CastleKing));
    REQUIRE(HasMove(moves, "e8", "c8", chess::MoveFlag::CastleQueen));
    REQUIRE(CountFrom(moves, "e8") == 7);
}

TEST_CASE("castling requires rights, home rooks, and an empty path",
          "[movegen]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("e1"), White(chess::PieceType::King));
    position.SetPiece(try4_test::Sq("a1"), White(chess::PieceType::Rook));
    position.SetPiece(try4_test::Sq("h1"), White(chess::PieceType::Rook));
    position.SetSideToMove(chess::Color::White);

    chess::MoveList moves;
    position.SetCastlingRights(chess::kNoCastlingRights);
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE_FALSE(HasMoveFromTo(moves, "e1", "g1"));
    REQUIRE_FALSE(HasMoveFromTo(moves, "e1", "c1"));

    position.SetCastlingRights(chess::kAllCastlingRights);
    position.SetPiece(try4_test::Sq("f1"), White(chess::PieceType::Knight));
    position.SetPiece(try4_test::Sq("b1"), White(chess::PieceType::Bishop));
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE_FALSE(HasMoveFromTo(moves, "e1", "g1"));
    REQUIRE_FALSE(HasMoveFromTo(moves, "e1", "c1"));

    position.SetPiece(try4_test::Sq("f1"), chess::Piece{});
    position.SetPiece(try4_test::Sq("b1"), chess::Piece{});
    position.SetPiece(try4_test::Sq("h1"), chess::Piece{});
    REQUIRE_FALSE(position.CastlingRightsConsistent());
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE_FALSE(HasMoveFromTo(moves, "e1", "g1"));
    REQUIRE(HasMove(moves, "e1", "c1", chess::MoveFlag::CastleQueen));

    position.SetPiece(try4_test::Sq("a1"), chess::Piece{});
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE_FALSE(HasMoveFromTo(moves, "e1", "c1"));
}

TEST_CASE("en passant is generated for both colors", "[movegen]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("e5"), White(chess::PieceType::Pawn));
    position.SetPiece(try4_test::Sq("d5"), Black(chess::PieceType::Pawn));
    position.SetEnPassant(try4_test::Sq("d6"));

    chess::MoveList moves;
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);

    REQUIRE(HasMove(moves, "e5", "d6",
                    chess::MoveFlag::Capture | chess::MoveFlag::EnPassant));
    REQUIRE(HasMove(moves, "e5", "e6"));
    REQUIRE(CountFrom(moves, "e5") == 2);

    position.Clear();
    position.SetSideToMove(chess::Color::Black);
    position.SetPiece(try4_test::Sq("e4"), Black(chess::PieceType::Pawn));
    position.SetPiece(try4_test::Sq("d4"), White(chess::PieceType::Pawn));
    position.SetEnPassant(try4_test::Sq("d3"));
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);

    REQUIRE(HasMove(moves, "e4", "d3",
                    chess::MoveFlag::Capture | chess::MoveFlag::EnPassant));
    REQUIRE(HasMove(moves, "e4", "e3"));
    REQUIRE(CountFrom(moves, "e4") == 2);
}

TEST_CASE("en passant requires a target, adjacent pawn, and empty target",
          "[movegen]") {
    chess::Position position = EmptyPosition();
    position.SetPiece(try4_test::Sq("e5"), White(chess::PieceType::Pawn));
    position.SetPiece(try4_test::Sq("d5"), Black(chess::PieceType::Pawn));

    chess::MoveList moves;
    position.SetEnPassant(std::nullopt);
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE_FALSE(HasMoveFromTo(moves, "e5", "d6"));

    position.SetEnPassant(try4_test::Sq("d6"));
    position.SetPiece(try4_test::Sq("d5"), chess::Piece{});
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE_FALSE(HasMoveFromTo(moves, "e5", "d6"));

    position.SetPiece(try4_test::Sq("d5"), Black(chess::PieceType::Pawn));
    position.SetPiece(try4_test::Sq("d6"), Black(chess::PieceType::Knight));
    chess::MoveGenerator::GeneratePseudoLegal(position, &moves);
    REQUIRE_FALSE(HasMove(moves, "e5", "d6",
                          chess::MoveFlag::Capture |
                              chess::MoveFlag::EnPassant));
    REQUIRE(HasMove(moves, "e5", "d6", chess::MoveFlag::Capture));
}
