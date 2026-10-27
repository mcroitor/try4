#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

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

bool HasMove(const std::vector<chess::Move>& moves, const char* from,
             const char* to,
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

bool HasMoveFromTo(const std::vector<chess::Move>& moves, const char* from,
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

}  // namespace

TEST_CASE("start position has 20 legal moves", "[legal]") {
    chess::Position position;
    position.SetupInitial();

    const std::vector<chess::Move> moves = position.LegalMoves();
    REQUIRE(moves.size() == 20);
    REQUIRE(position.HistoryDepth() == 0);

    for (const chess::Move& move : moves) {
        REQUIRE(move.IsValid());
    }
}

TEST_CASE("make and undo restore a quiet move exactly", "[legal]") {
    chess::Position position;
    position.SetupInitial();
    const std::string before = position.ToFen();

    REQUIRE(position.MakeMove(try4_test::Move("e2", "e4")));
    REQUIRE(position.ToFen() ==
            "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
    REQUIRE(position.HistoryDepth() == 1);

    REQUIRE(position.UndoMove());
    REQUIRE(position.ToFen() == before);
    REQUIRE(position.HistoryDepth() == 0);
    REQUIRE_FALSE(position.UndoMove());
}

TEST_CASE("black moves increment the fullmove number", "[legal]") {
    chess::Position position;
    position.SetupInitial();

    REQUIRE(position.MakeMove(try4_test::Move("e2", "e4")));
    REQUIRE(position.MakeMove(try4_test::Move("e7", "e5")));
    REQUIRE(position.ToFen() ==
            "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2");
    REQUIRE(position.FullmoveNumber() == 2);

    REQUIRE(position.UndoMove());
    REQUIRE(position.SideToMove() == chess::Color::Black);
    REQUIRE(position.FullmoveNumber() == 1);
    REQUIRE(position.UndoMove());
    REQUIRE(position.SideToMove() == chess::Color::White);
}

TEST_CASE("castling moves the rook and restores exactly", "[legal]") {
    chess::Position white = FromFen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    const std::string before = white.ToFen();

    REQUIRE(white.MakeMove(
        try4_test::Move("e1", "g1", chess::MoveFlag::CastleKing)));
    REQUIRE(white.PieceAt(try4_test::Sq("g1")) ==
            chess::Piece(chess::Color::White, chess::PieceType::King));
    REQUIRE(white.PieceAt(try4_test::Sq("f1")) ==
            chess::Piece(chess::Color::White, chess::PieceType::Rook));
    REQUIRE(white.PieceAt(try4_test::Sq("e1")).IsEmpty());
    REQUIRE(white.PieceAt(try4_test::Sq("h1")).IsEmpty());
    REQUIRE(white.CastlingRights() ==
            (chess::ToMask(chess::CastlingRight::BlackKingSide) |
             chess::ToMask(chess::CastlingRight::BlackQueenSide)));

    REQUIRE(white.UndoMove());
    REQUIRE(white.ToFen() == before);

    chess::Position black = FromFen("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
    const std::string black_before = black.ToFen();
    REQUIRE(black.MakeMove(
        try4_test::Move("e8", "c8", chess::MoveFlag::CastleQueen)));
    REQUIRE(black.PieceAt(try4_test::Sq("c8")) ==
            chess::Piece(chess::Color::Black, chess::PieceType::King));
    REQUIRE(black.PieceAt(try4_test::Sq("d8")) ==
            chess::Piece(chess::Color::Black, chess::PieceType::Rook));
    REQUIRE(black.UndoMove());
    REQUIRE(black.ToFen() == black_before);
}

TEST_CASE("en passant removes the pawn behind the target", "[legal]") {
    chess::Position position =
        FromFen("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    const std::string before = position.ToFen();

    REQUIRE(position.MakeMove(try4_test::Move(
        "e5", "d6",
        chess::MoveFlag::Capture | chess::MoveFlag::EnPassant)));
    REQUIRE(position.PieceAt(try4_test::Sq("d6")) ==
            chess::Piece(chess::Color::White, chess::PieceType::Pawn));
    REQUIRE(position.PieceAt(try4_test::Sq("d5")).IsEmpty());
    REQUIRE(position.PieceAt(try4_test::Sq("e5")).IsEmpty());
    REQUIRE_FALSE(position.EnPassant().has_value());
    REQUIRE(position.HalfmoveClock() == 0);

    REQUIRE(position.UndoMove());
    REQUIRE(position.ToFen() == before);
}

TEST_CASE("promotion replaces the pawn and restores it", "[legal]") {
    chess::Position position = FromFen("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    const std::string before = position.ToFen();

    REQUIRE(position.MakeMove(try4_test::Move(
        "a7", "a8", chess::MoveFlag::Promotion, chess::PieceType::Queen)));
    REQUIRE(position.PieceAt(try4_test::Sq("a8")) ==
            chess::Piece(chess::Color::White, chess::PieceType::Queen));
    REQUIRE(position.PieceAt(try4_test::Sq("a7")).IsEmpty());
    REQUIRE(position.HalfmoveClock() == 0);

    REQUIRE(position.UndoMove());
    REQUIRE(position.ToFen() == before);
    REQUIRE(position.PieceAt(try4_test::Sq("a7")) ==
            chess::Piece(chess::Color::White, chess::PieceType::Pawn));
}

TEST_CASE("captures update the halfmove clock and restore material",
          "[legal]") {
    chess::Position position =
        FromFen("4k3/8/8/8/8/8/3r4/3RK3 w - - 17 9");
    const std::string before = position.ToFen();

    REQUIRE(position.MakeMove(
        try4_test::Move("d1", "d2", chess::MoveFlag::Capture)));
    REQUIRE(position.HalfmoveClock() == 0);
    REQUIRE(position.FullmoveNumber() == 9);
    REQUIRE(position.PieceAt(try4_test::Sq("d2")) ==
            chess::Piece(chess::Color::White, chess::PieceType::Rook));
    REQUIRE(position.PieceAt(try4_test::Sq("d1")).IsEmpty());

    REQUIRE(position.UndoMove());
    REQUIRE(position.ToFen() == before);
}

TEST_CASE("illegal moves are rejected without changing state", "[legal]") {
    chess::Position position;
    position.SetupInitial();
    const std::string before = position.ToFen();

    REQUIRE_FALSE(position.MakeMove(try4_test::Move("e2", "e5")));
    REQUIRE_FALSE(position.MakeMove(try4_test::Move("e7", "e5")));
    REQUIRE_FALSE(position.MakeMove(chess::Move{}));
    REQUIRE(position.ToFen() == before);
    REQUIRE(position.HistoryDepth() == 0);
}

TEST_CASE("castling through check is illegal", "[legal]") {
    chess::Position through_check =
        FromFen("4kr2/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    const std::vector<chess::Move> through =
        through_check.LegalMoves();
    REQUIRE_FALSE(HasMove(through, "e1", "g1", chess::MoveFlag::CastleKing));
    REQUIRE(HasMove(through, "e1", "c1", chess::MoveFlag::CastleQueen));

    chess::Position in_check =
        FromFen("4r1k1/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    const std::vector<chess::Move> checked = in_check.LegalMoves();
    REQUIRE_FALSE(HasMove(checked, "e1", "g1", chess::MoveFlag::CastleKing));
    REQUIRE_FALSE(HasMove(checked, "e1", "c1", chess::MoveFlag::CastleQueen));

    chess::Position destination =
        FromFen("6rk/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    const std::vector<chess::Move> destination_moves =
        destination.LegalMoves();
    REQUIRE_FALSE(
        HasMove(destination_moves, "e1", "g1", chess::MoveFlag::CastleKing));
    REQUIRE(HasMove(destination_moves, "e1", "c1",
                    chess::MoveFlag::CastleQueen));

    chess::Position b_file =
        FromFen("1r4k1/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    const std::vector<chess::Move> b_file_moves = b_file.LegalMoves();
    REQUIRE(HasMove(b_file_moves, "e1", "c1",
                    chess::MoveFlag::CastleQueen));
}

TEST_CASE("en passant discovered check is illegal", "[legal]") {
    chess::Position position =
        FromFen("4r2k/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    const std::vector<chess::Move> moves = position.LegalMoves();

    REQUIRE_FALSE(HasMove(moves, "e5", "d6",
                          chess::MoveFlag::Capture |
                              chess::MoveFlag::EnPassant));
    REQUIRE(HasMove(moves, "e5", "e6"));
}

TEST_CASE("pinned pieces cannot expose the king", "[legal]") {
    chess::Position position =
        FromFen("4r2k/8/8/8/8/8/4N3/4K3 w - - 0 1");
    const std::vector<chess::Move> moves = position.LegalMoves();

    for (const chess::Move& move : moves) {
        REQUIRE(move.from != try4_test::Sq("e2"));
    }
}

TEST_CASE("kings cannot move into check", "[legal]") {
    chess::Position position =
        FromFen("4r2k/8/8/8/8/8/8/4K3 w - - 0 1");
    const std::vector<chess::Move> moves = position.LegalMoves();

    REQUIRE_FALSE(HasMoveFromTo(moves, "e1", "e2"));
    REQUIRE(HasMoveFromTo(moves, "e1", "d1"));
    REQUIRE(HasMoveFromTo(moves, "e1", "f1"));
}

TEST_CASE("Kiwipete has 48 legal moves", "[legal]") {
    chess::Position position = FromFen(
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq "
        "- 0 1");
    const std::vector<chess::Move> moves = position.LegalMoves();
    REQUIRE(moves.size() == 48);
}

TEST_CASE("legal move generation does not leave history behind", "[legal]") {
    chess::Position position;
    position.SetupInitial();
    REQUIRE(position.HistoryDepth() == 0);

    const std::vector<chess::Move> first = position.LegalMoves();
    REQUIRE(first.size() == 20);
    REQUIRE(position.HistoryDepth() == 0);

    REQUIRE(position.MakeMove(try4_test::Move("e2", "e4")));
    REQUIRE(position.HistoryDepth() == 1);

    const std::vector<chess::Move> second = position.LegalMoves();
    REQUIRE(second.size() == 20);
    REQUIRE(position.HistoryDepth() == 1);
}
