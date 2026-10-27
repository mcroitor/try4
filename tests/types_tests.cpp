#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "chess/types.hpp"
#include "test_support.hpp"

TEST_CASE("all 64 squares round-trip through algebraic notation", "[square]") {
    for (int index = 0; index < 64; ++index) {
        const chess::Square square(static_cast<std::uint8_t>(index));

        REQUIRE(square.IsValid());
        REQUIRE(square.Index() == index);
        REQUIRE(square.File() == index % 8);
        REQUIRE(square.Rank() == index / 8);

        const std::string text = square.ToString();
        REQUIRE(text.size() == 2);

        const auto parsed = chess::Square::Parse(text);
        REQUIRE(parsed.has_value());
        REQUIRE(*parsed == square);
        REQUIRE(chess::Square::FromAlgebraic(text) == parsed);

        std::string upper = text;
        upper[0] = static_cast<char>(
            std::toupper(static_cast<unsigned char>(upper[0])));
        REQUIRE(chess::Square::Parse(upper) == parsed);
    }
}

TEST_CASE("square boundaries and invalid values", "[square]") {
    REQUIRE(try4_test::Sq("a1").Index() == 0);
    REQUIRE(try4_test::Sq("h1").Index() == 7);
    REQUIRE(try4_test::Sq("a8").Index() == 56);
    REQUIRE(try4_test::Sq("h8").Index() == 63);

    const chess::Square invalid_default;
    REQUIRE_FALSE(invalid_default.IsValid());
    REQUIRE(invalid_default.ToString().empty());

    const chess::Square invalid_high(
        static_cast<std::uint8_t>(chess::Square::kInvalidIndex));
    REQUIRE_FALSE(invalid_high.IsValid());
    REQUIRE(invalid_high.ToString().empty());
}

TEST_CASE("invalid square strings are rejected", "[square]") {
    const std::vector<std::string_view> invalid = {
        "",       "a",      "a1x",   "1a",    "a0",   "i1",
        "a9",     "h9",     " a1",   "a1 ",   "A0",   "H9",
        "a1\n",   "--",     "aa",    "11",
    };

    for (const std::string_view text : invalid) {
        INFO("input: " << text);
        REQUIRE_FALSE(chess::Square::Parse(text).has_value());
    }
}

TEST_CASE("squares compare by board index", "[square]") {
    const chess::Square a1 = try4_test::Sq("a1");
    const chess::Square b1 = try4_test::Sq("b1");
    const chess::Square a2 = try4_test::Sq("a2");

    REQUIRE(a1 == a1);
    REQUIRE(a1 != b1);
    REQUIRE(a1 < b1);
    REQUIRE(b1 < a2);
    REQUIRE_FALSE(a2 < a1);
}

TEST_CASE("colors and piece types have the expected values", "[piece]") {
    REQUIRE(chess::Opposite(chess::Color::White) == chess::Color::Black);
    REQUIRE(chess::Opposite(chess::Color::Black) == chess::Color::White);

    REQUIRE(chess::PieceTypeToChar(chess::PieceType::None) == ' ');
    REQUIRE(chess::PieceTypeToChar(chess::PieceType::Pawn) == 'P');
    REQUIRE(chess::PieceTypeToChar(chess::PieceType::Knight) == 'N');
    REQUIRE(chess::PieceTypeToChar(chess::PieceType::Bishop) == 'B');
    REQUIRE(chess::PieceTypeToChar(chess::PieceType::Rook) == 'R');
    REQUIRE(chess::PieceTypeToChar(chess::PieceType::Queen) == 'Q');
    REQUIRE(chess::PieceTypeToChar(chess::PieceType::King) == 'K');
}

TEST_CASE("default piece is empty", "[piece]") {
    const chess::Piece piece;

    REQUIRE(piece.IsEmpty());
    REQUIRE(piece.Type() == chess::PieceType::None);
    REQUIRE(piece.ToString() == " ");
}

TEST_CASE("pieces render with the expected case", "[piece]") {
    REQUIRE(chess::Piece(chess::Color::White, chess::PieceType::Pawn)
                .ToString() == "P");
    REQUIRE(chess::Piece(chess::Color::White, chess::PieceType::Knight)
                .ToString() == "N");
    REQUIRE(chess::Piece(chess::Color::White, chess::PieceType::Bishop)
                .ToString() == "B");
    REQUIRE(chess::Piece(chess::Color::White, chess::PieceType::Rook)
                .ToString() == "R");
    REQUIRE(chess::Piece(chess::Color::White, chess::PieceType::Queen)
                .ToString() == "Q");
    REQUIRE(chess::Piece(chess::Color::White, chess::PieceType::King)
                .ToString() == "K");

    REQUIRE(chess::Piece(chess::Color::Black, chess::PieceType::Pawn)
                .ToString() == "p");
    REQUIRE(chess::Piece(chess::Color::Black, chess::PieceType::Knight)
                .ToString() == "n");
    REQUIRE(chess::Piece(chess::Color::Black, chess::PieceType::Bishop)
                .ToString() == "b");
    REQUIRE(chess::Piece(chess::Color::Black, chess::PieceType::Rook)
                .ToString() == "r");
    REQUIRE(chess::Piece(chess::Color::Black, chess::PieceType::Queen)
                .ToString() == "q");
    REQUIRE(chess::Piece(chess::Color::Black, chess::PieceType::King)
                .ToString() == "k");
}

TEST_CASE("piece equality compares color and type", "[piece]") {
    const chess::Piece white_pawn(chess::Color::White,
                                  chess::PieceType::Pawn);
    const chess::Piece other_white_pawn(chess::Color::White,
                                        chess::PieceType::Pawn);
    const chess::Piece black_pawn(chess::Color::Black,
                                  chess::PieceType::Pawn);
    const chess::Piece white_knight(chess::Color::White,
                                    chess::PieceType::Knight);

    REQUIRE(white_pawn == other_white_pawn);
    REQUIRE(white_pawn != black_pawn);
    REQUIRE(white_pawn != white_knight);
    REQUIRE(chess::Piece{} == chess::Piece{});
}

TEST_CASE("piece characters parse by case", "[piece]") {
    REQUIRE(chess::PieceFromChar('P') ==
            chess::Piece(chess::Color::White, chess::PieceType::Pawn));
    REQUIRE(chess::PieceFromChar('n') ==
            chess::Piece(chess::Color::Black, chess::PieceType::Knight));
    REQUIRE(chess::PieceFromChar('K') ==
            chess::Piece(chess::Color::White, chess::PieceType::King));
    REQUIRE(chess::PieceFromChar('q') ==
            chess::Piece(chess::Color::Black, chess::PieceType::Queen));

    REQUIRE_FALSE(chess::PieceFromChar(' ').has_value());
    REQUIRE_FALSE(chess::PieceFromChar('x').has_value());
    REQUIRE_FALSE(chess::PieceFromChar('-').has_value());
}

TEST_CASE("move flags are independent bits", "[move]") {
    const std::array<chess::MoveFlag, 6> flags = {
        chess::MoveFlag::Capture,
        chess::MoveFlag::DoublePush,
        chess::MoveFlag::EnPassant,
        chess::MoveFlag::CastleKing,
        chess::MoveFlag::CastleQueen,
        chess::MoveFlag::Promotion,
    };

    for (int mask = 0; mask < 64; ++mask) {
        const auto value = static_cast<chess::MoveFlag>(mask);
        for (std::size_t i = 0; i < flags.size(); ++i) {
            const int bit = 1 << i;
            REQUIRE(chess::HasFlag(value, flags[i]) ==
                    ((mask & bit) != 0));
        }
    }

    for (std::size_t i = 0; i < flags.size(); ++i) {
        const chess::PieceType promotion =
            flags[i] == chess::MoveFlag::Promotion
                ? chess::PieceType::Queen
                : chess::PieceType::None;
        const chess::Move move(try4_test::Sq("a1"), try4_test::Sq("a2"),
                               flags[i], promotion);

        REQUIRE(move.HasFlag(flags[i]));
        for (std::size_t j = 0; j < flags.size(); ++j) {
            if (i != j) {
                REQUIRE_FALSE(move.HasFlag(flags[j]));
            }
        }
    }

    const chess::MoveFlag combined =
        chess::MoveFlag::Capture | chess::MoveFlag::Promotion;
    REQUIRE(chess::HasFlag(combined, chess::MoveFlag::Capture));
    REQUIRE(chess::HasFlag(combined, chess::MoveFlag::Promotion));
    REQUIRE_FALSE(chess::HasFlag(combined, chess::MoveFlag::EnPassant));

    chess::MoveFlag mutable_flags = chess::MoveFlag::Capture;
    mutable_flags |= chess::MoveFlag::EnPassant;
    REQUIRE(chess::HasFlag(mutable_flags, chess::MoveFlag::Capture));
    REQUIRE(chess::HasFlag(mutable_flags, chess::MoveFlag::EnPassant));
}

TEST_CASE("ordinary moves format as UCI", "[move]") {
    REQUIRE(try4_test::Move("e2", "e4").ToUci() == "e2e4");
    REQUIRE(try4_test::Move("e4", "d5", chess::MoveFlag::Capture).ToUci() ==
            "e4d5");
    REQUIRE(try4_test::Move("e2", "e4", chess::MoveFlag::DoublePush)
                .ToUci() == "e2e4");
    REQUIRE(try4_test::Move("e5", "d6", chess::MoveFlag::EnPassant)
                .ToUci() == "e5d6");
    REQUIRE(try4_test::Move("e1", "g1", chess::MoveFlag::CastleKing)
                .ToUci() == "e1g1");
    REQUIRE(try4_test::Move("e1", "c1", chess::MoveFlag::CastleQueen)
                .ToUci() == "e1c1");
}

TEST_CASE("promotions format with the promotion piece", "[move]") {
    const std::array<std::pair<chess::PieceType, const char*>, 4> promotions = {
        std::make_pair(chess::PieceType::Knight, "e7e8n"),
        std::make_pair(chess::PieceType::Bishop, "e7e8b"),
        std::make_pair(chess::PieceType::Rook, "e7e8r"),
        std::make_pair(chess::PieceType::Queen, "e7e8q"),
    };

    for (const auto& promotion : promotions) {
        const chess::Move move(try4_test::Sq("e7"), try4_test::Sq("e8"),
                               chess::MoveFlag::Promotion, promotion.first);
        REQUIRE(move.IsValid());
        REQUIRE(move.ToUci() == promotion.second);
    }

    const chess::Move capture_promotion(
        try4_test::Sq("e7"), try4_test::Sq("d8"),
        chess::MoveFlag::Capture | chess::MoveFlag::Promotion,
        chess::PieceType::Queen);
    REQUIRE(capture_promotion.IsValid());
    REQUIRE(capture_promotion.ToUci() == "e7d8q");
}

TEST_CASE("promotion state is normalized and validated", "[move]") {
    const chess::Move no_flag(try4_test::Sq("e2"), try4_test::Sq("e4"),
                              chess::MoveFlag::None, chess::PieceType::Queen);
    REQUIRE(no_flag.promotion == chess::PieceType::None);
    REQUIRE(no_flag.IsValid());

    const chess::Move invalid_promotion(
        try4_test::Sq("e7"), try4_test::Sq("e8"),
        chess::MoveFlag::Promotion, chess::PieceType::Pawn);
    REQUIRE_FALSE(invalid_promotion.IsValid());
    REQUIRE(invalid_promotion.ToUci().empty());
}

TEST_CASE("null moves are distinct from board moves", "[move]") {
    const chess::Move null_move;
    REQUIRE(null_move.IsNull());
    REQUIRE_FALSE(null_move.IsValid());
    REQUIRE(null_move.ToUci() == "0000");

    const chess::Move a1a1(try4_test::Sq("a1"), try4_test::Sq("a1"));
    REQUIRE_FALSE(a1a1.IsNull());
    REQUIRE_FALSE(a1a1.IsValid());
    REQUIRE(a1a1.ToUci().empty());

    const chess::Move ordinary = try4_test::Move("a1", "a2");
    REQUIRE_FALSE(ordinary.IsNull());
    REQUIRE(ordinary.IsValid());
    REQUIRE(ordinary.ToUci() == "a1a2");
}

TEST_CASE("move equality compares all fields", "[move]") {
    const chess::Move move = try4_test::Move(
        "e7", "e8", chess::MoveFlag::Promotion, chess::PieceType::Queen);
    const chess::Move same = try4_test::Move(
        "e7", "e8", chess::MoveFlag::Promotion, chess::PieceType::Queen);
    const chess::Move different_piece = try4_test::Move(
        "e7", "e8", chess::MoveFlag::Promotion, chess::PieceType::Rook);

    REQUIRE(move == same);
    REQUIRE(move != different_piece);
}
