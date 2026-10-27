#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "chess/position.hpp"
#include "test_support.hpp"

namespace {

const std::string kInitialBoard =
    "rnbqkbnr\n"
    "pppppppp\n"
    "        \n"
    "        \n"
    "        \n"
    "        \n"
    "PPPPPPPP\n"
    "RNBQKBNR\n";

const std::string kAfterE2E4 =
    "rnbqkbnr\n"
    "pppppppp\n"
    "        \n"
    "        \n"
    "    P   \n"
    "        \n"
    "PPPP PPP\n"
    "RNBQKBNR\n";

const std::string kEmptyBoard =
    "        \n"
    "        \n"
    "        \n"
    "        \n"
    "        \n"
    "        \n"
    "        \n"
    "        \n";

}  // namespace

TEST_CASE("initial position has the standard collection", "[position]") {
    chess::Position position;
    position.SetupInitial();

    REQUIRE(position.PieceAt(try4_test::Sq("a1")) ==
            chess::Piece(chess::Color::White, chess::PieceType::Rook));
    REQUIRE(position.PieceAt(try4_test::Sq("b1")) ==
            chess::Piece(chess::Color::White, chess::PieceType::Knight));
    REQUIRE(position.PieceAt(try4_test::Sq("c1")) ==
            chess::Piece(chess::Color::White, chess::PieceType::Bishop));
    REQUIRE(position.PieceAt(try4_test::Sq("d1")) ==
            chess::Piece(chess::Color::White, chess::PieceType::Queen));
    REQUIRE(position.PieceAt(try4_test::Sq("e1")) ==
            chess::Piece(chess::Color::White, chess::PieceType::King));
    REQUIRE(position.PieceAt(try4_test::Sq("f1")) ==
            chess::Piece(chess::Color::White, chess::PieceType::Bishop));
    REQUIRE(position.PieceAt(try4_test::Sq("g1")) ==
            chess::Piece(chess::Color::White, chess::PieceType::Knight));
    REQUIRE(position.PieceAt(try4_test::Sq("h1")) ==
            chess::Piece(chess::Color::White, chess::PieceType::Rook));

    REQUIRE(position.PieceAt(try4_test::Sq("a2")) ==
            chess::Piece(chess::Color::White, chess::PieceType::Pawn));
    REQUIRE(position.PieceAt(try4_test::Sq("h2")) ==
            chess::Piece(chess::Color::White, chess::PieceType::Pawn));
    REQUIRE(position.PieceAt(try4_test::Sq("a7")) ==
            chess::Piece(chess::Color::Black, chess::PieceType::Pawn));
    REQUIRE(position.PieceAt(try4_test::Sq("h7")) ==
            chess::Piece(chess::Color::Black, chess::PieceType::Pawn));

    REQUIRE(position.PieceAt(try4_test::Sq("a8")) ==
            chess::Piece(chess::Color::Black, chess::PieceType::Rook));
    REQUIRE(position.PieceAt(try4_test::Sq("d8")) ==
            chess::Piece(chess::Color::Black, chess::PieceType::Queen));
    REQUIRE(position.PieceAt(try4_test::Sq("e8")) ==
            chess::Piece(chess::Color::Black, chess::PieceType::King));
    REQUIRE(position.PieceAt(try4_test::Sq("h8")) ==
            chess::Piece(chess::Color::Black, chess::PieceType::Rook));

    REQUIRE(position.PieceAt(try4_test::Sq("e4")).IsEmpty());
    REQUIRE(position.PieceAt(try4_test::Sq("d5")).IsEmpty());
}

TEST_CASE("initial position has the standard state", "[position]") {
    chess::Position position;
    position.SetupInitial();

    REQUIRE(position.SideToMove() == chess::Color::White);
    REQUIRE(position.CastlingRights() == chess::kAllCastlingRights);
    REQUIRE_FALSE(position.EnPassant().has_value());
    REQUIRE(position.HalfmoveClock() == 0);
    REQUIRE(position.FullmoveNumber() == 1);
    REQUIRE(position.ToString() == kInitialBoard);
}

TEST_CASE("clear produces an empty position", "[position]") {
    chess::Position position;
    position.SetupInitial();
    position.Clear();

    for (int index = 0; index < 64; ++index) {
        REQUIRE(position
                    .PieceAt(chess::Square(
                        static_cast<std::uint8_t>(index)))
                    .IsEmpty());
    }

    REQUIRE(position.SideToMove() == chess::Color::White);
    REQUIRE(position.CastlingRights() == chess::kNoCastlingRights);
    REQUIRE_FALSE(position.EnPassant().has_value());
    REQUIRE(position.HalfmoveClock() == 0);
    REQUIRE(position.FullmoveNumber() == 1);
    REQUIRE(position.ToString() == kEmptyBoard);
}

TEST_CASE("default and cleared positions are identical", "[position]") {
    const chess::Position default_position;
    chess::Position cleared;
    cleared.SetupInitial();
    cleared.Clear();

    REQUIRE(default_position == cleared);
    REQUIRE(default_position.ToString() == kEmptyBoard);
}

TEST_CASE("piece accessors validate squares", "[position]") {
    chess::Position position;
    position.SetupInitial();

    const chess::Square invalid(
        static_cast<std::uint8_t>(chess::Square::kInvalidIndex));
    REQUIRE(position.PieceAt(invalid).IsEmpty());
    REQUIRE_FALSE(position.SetPiece(
        invalid, chess::Piece(chess::Color::White, chess::PieceType::Knight)));
    REQUIRE(position.ToString() == kInitialBoard);

    REQUIRE(position.SetPiece(
        try4_test::Sq("e4"),
        chess::Piece(chess::Color::White, chess::PieceType::Knight)));
    REQUIRE(position.PieceAt(try4_test::Sq("e4")) ==
            chess::Piece(chess::Color::White, chess::PieceType::Knight));

    REQUIRE(position.SetPiece(try4_test::Sq("e4"), chess::Piece{}));
    REQUIRE(position.PieceAt(try4_test::Sq("e4")).IsEmpty());
}

TEST_CASE("piece changes render on the correct square", "[position]") {
    chess::Position position;
    position.SetupInitial();

    REQUIRE(position.SetPiece(try4_test::Sq("e2"), chess::Piece{}));
    REQUIRE(position.SetPiece(
        try4_test::Sq("e4"),
        chess::Piece(chess::Color::White, chess::PieceType::Pawn)));
    REQUIRE(position.ToString() == kAfterE2E4);
}

TEST_CASE("state setters update and validate state", "[position]") {
    chess::Position position;
    position.SetupInitial();

    position.SetSideToMove(chess::Color::Black);
    position.SetCastlingRights(static_cast<chess::CastlingRights>(
        chess::ToMask(chess::CastlingRight::WhiteKingSide) |
        chess::ToMask(chess::CastlingRight::BlackQueenSide)));
    position.SetEnPassant(try4_test::Sq("e3"));
    position.SetHalfmoveClock(17);
    position.SetFullmoveNumber(42);

    REQUIRE(position.SideToMove() == chess::Color::Black);
    REQUIRE(position.CastlingRights() ==
            (chess::ToMask(chess::CastlingRight::WhiteKingSide) |
             chess::ToMask(chess::CastlingRight::BlackQueenSide)));
    REQUIRE(position.EnPassant() == try4_test::Sq("e3"));
    REQUIRE(position.HalfmoveClock() == 17);
    REQUIRE(position.FullmoveNumber() == 42);

    position.SetHalfmoveClock(-1);
    position.SetFullmoveNumber(0);
    position.SetEnPassant(chess::Square(
        static_cast<std::uint8_t>(chess::Square::kInvalidIndex)));

    REQUIRE(position.HalfmoveClock() == 17);
    REQUIRE(position.FullmoveNumber() == 42);
    REQUIRE(position.EnPassant() == try4_test::Sq("e3"));
}

TEST_CASE("castling rights behave as independent bits", "[position]") {
    chess::CastlingRights rights = chess::kNoCastlingRights;

    REQUIRE_FALSE(chess::HasCastlingRight(
        rights, chess::CastlingRight::WhiteKingSide));

    rights = chess::WithCastlingRight(
        rights, chess::CastlingRight::WhiteKingSide);
    REQUIRE(chess::HasCastlingRight(rights,
                                    chess::CastlingRight::WhiteKingSide));
    REQUIRE_FALSE(chess::HasCastlingRight(
        rights, chess::CastlingRight::WhiteQueenSide));

    rights = chess::WithCastlingRight(
        rights, chess::CastlingRight::BlackQueenSide);
    REQUIRE(chess::HasCastlingRight(rights,
                                    chess::CastlingRight::BlackQueenSide));

    rights = chess::WithoutCastlingRight(
        rights, chess::CastlingRight::WhiteKingSide);
    REQUIRE_FALSE(chess::HasCastlingRight(
        rights, chess::CastlingRight::WhiteKingSide));
    REQUIRE(chess::HasCastlingRight(rights,
                                    chess::CastlingRight::BlackQueenSide));
}

TEST_CASE("castling rights report missing home pieces", "[position]") {
    chess::Position position;
    position.SetupInitial();
    REQUIRE(position.CastlingRightsConsistent());

    position.SetPiece(try4_test::Sq("h1"), chess::Piece{});
    REQUIRE_FALSE(position.CastlingRightsConsistent());
    position.SetCastlingRights(chess::WithoutCastlingRight(
        position.CastlingRights(), chess::CastlingRight::WhiteKingSide));
    REQUIRE(position.CastlingRightsConsistent());

    position.SetPiece(try4_test::Sq("e1"), chess::Piece{});
    REQUIRE_FALSE(position.CastlingRightsConsistent());

    position.SetCastlingRights(chess::kNoCastlingRights);
    REQUIRE(position.CastlingRightsConsistent());
}

TEST_CASE("copying a position produces independent state", "[position]") {
    chess::Position original;
    original.SetupInitial();

    chess::Position copy = original;
    REQUIRE(copy == original);

    copy.SetPiece(
        try4_test::Sq("e4"),
        chess::Piece(chess::Color::White, chess::PieceType::Knight));
    copy.SetSideToMove(chess::Color::Black);
    copy.SetCastlingRights(chess::kNoCastlingRights);
    copy.SetEnPassant(try4_test::Sq("e3"));
    copy.SetHalfmoveClock(9);
    copy.SetFullmoveNumber(4);

    REQUIRE(original.PieceAt(try4_test::Sq("e4")).IsEmpty());
    REQUIRE(original.SideToMove() == chess::Color::White);
    REQUIRE(original.CastlingRights() == chess::kAllCastlingRights);
    REQUIRE_FALSE(original.EnPassant().has_value());
    REQUIRE(original.HalfmoveClock() == 0);
    REQUIRE(original.FullmoveNumber() == 1);
    REQUIRE(original.ToString() == kInitialBoard);

    REQUIRE(copy != original);
    REQUIRE(copy.PieceAt(try4_test::Sq("e4")) ==
            chess::Piece(chess::Color::White, chess::PieceType::Knight));
    REQUIRE(copy.SideToMove() == chess::Color::Black);
    REQUIRE(copy.CastlingRights() == chess::kNoCastlingRights);
    REQUIRE(copy.EnPassant() == try4_test::Sq("e3"));
    REQUIRE(copy.HalfmoveClock() == 9);
    REQUIRE(copy.FullmoveNumber() == 4);
}
