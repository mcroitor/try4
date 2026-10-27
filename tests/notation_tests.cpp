#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "chess/notation.hpp"
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

std::string San(const char* fen, const char* from, const char* to,
                chess::MoveFlag flags = chess::MoveFlag::None,
                chess::PieceType promotion = chess::PieceType::None) {
    chess::Position position = FromFen(fen);
    return chess::ToSan(position, try4_test::Move(from, to, flags, promotion));
}

}  // namespace

TEST_CASE("UCI parsing returns canonical legal moves", "[notation]") {
    chess::Position start;
    start.SetupInitial();
    const auto e4 = chess::ParseUci(start, "e2e4");
    REQUIRE(e4.has_value());
    REQUIRE(e4->HasFlag(chess::MoveFlag::DoublePush));
    REQUIRE(e4->ToUci() == "e2e4");
    REQUIRE_FALSE(chess::ParseUci(start, "e2e5").has_value());
    REQUIRE_FALSE(chess::ParseUci(start, "0000").has_value());
    REQUIRE_FALSE(chess::ParseUci(start, "e7e8x").has_value());
    REQUIRE_FALSE(chess::ParseUci(start, "z9z9").has_value());

    chess::Position castle =
        FromFen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    const auto king_side = chess::ParseUci(castle, "e1g1");
    REQUIRE(king_side.has_value());
    REQUIRE(king_side->HasFlag(chess::MoveFlag::CastleKing));

    chess::Position ep = FromFen("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    const auto en_passant = chess::ParseUci(ep, "e5d6");
    REQUIRE(en_passant.has_value());
    REQUIRE(en_passant->HasFlag(chess::MoveFlag::EnPassant));

    chess::Position promotion = FromFen("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    const auto queen = chess::ParseUci(promotion, "a7a8q");
    REQUIRE(queen.has_value());
    REQUIRE(queen->HasFlag(chess::MoveFlag::Promotion));
    REQUIRE(queen->promotion == chess::PieceType::Queen);
    REQUIRE_FALSE(chess::ParseUci(promotion, "a7a8").has_value());
}

TEST_CASE("SAN formats quiet moves, captures, and castle", "[notation]") {
    chess::Position start;
    start.SetupInitial();
    REQUIRE(chess::ToSan(start, try4_test::Move("e2", "e4")) == "e4");
    REQUIRE(chess::ToSan(start, try4_test::Move("g1", "f3")) == "Nf3");
    REQUIRE(chess::ToSan(start, try4_test::Move("b1", "c3")) == "Nc3");
    REQUIRE(chess::ToSan(start, try4_test::Move("e2", "e5")).empty());

    REQUIRE(San("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "e1", "g1") ==
            "O-O");
    REQUIRE(San("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "e1", "c1") ==
            "O-O-O");

    REQUIRE(San("4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1", "e4", "d5") ==
            "exd5");
    REQUIRE(San("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", "e5", "d6") ==
            "exd6");
}

TEST_CASE("SAN formats promotion, check, and mate", "[notation]") {
    REQUIRE(San("4k3/P7/8/8/8/8/8/4K3 w - - 0 1", "a7", "a8",
                chess::MoveFlag::Promotion, chess::PieceType::Queen) ==
            "a8=Q+");
    REQUIRE(San("6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1", "a1", "a8") ==
            "Ra8#");
    REQUIRE(San("4k3/8/8/8/8/8/8/4K2R w - - 0 1", "h1", "h8") ==
            "Rh8+");
    REQUIRE(San("6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1", "a1", "a2") ==
            "Ra2");
}

TEST_CASE("SAN disambiguates by file, rank, or both", "[notation]") {
    REQUIRE(San("4k3/8/8/8/8/8/8/1N2KN2 w - - 0 1", "b1", "d2") ==
            "Nbd2");
    REQUIRE(San("4k3/8/8/8/8/8/8/1N2KN2 w - - 0 1", "f1", "d2") ==
            "Nfd2");
    REQUIRE(San("4k3/8/8/8/8/R7/8/R3K3 w - - 0 1", "a1", "a2") ==
            "R1a2");
    REQUIRE(San("4k3/8/8/8/8/R7/8/R3K3 w - - 0 1", "a3", "a2") ==
            "R3a2");
    REQUIRE(San("4k3/8/8/8/8/Q7/8/Q1Q1K3 w - - 0 1", "a1", "b2") ==
            "Qa1b2");
}

TEST_CASE("SAN round-trips every legal move in representative positions",
          "[notation]") {
    const char* const fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
        "4k3/P7/8/8/8/8/8/4K3 w - - 0 1",
        "4k3/8/8/8/8/8/8/1N2KN2 w - - 0 1",
    };

    for (const char* fen : fens) {
        INFO("FEN: " << fen);
        const chess::Position position = FromFen(fen);
        for (const chess::Move& move : position.LegalMoves()) {
            INFO("move: " << move.ToUci());
            const std::string san = chess::ToSan(position, move);
            REQUIRE_FALSE(san.empty());
            const auto parsed = chess::ParseSan(position, san);
            REQUIRE(parsed.has_value());
            REQUIRE(parsed->from == move.from);
            REQUIRE(parsed->to == move.to);
            REQUIRE(parsed->promotion == move.promotion);
        }
    }
}

TEST_CASE("SAN parsing accepts optional suffixes and zero castling",
          "[notation]") {
    const chess::Position start =
        FromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    REQUIRE(chess::ParseSan(start, "e4").has_value());
    REQUIRE(chess::ParseSan(start, "e4+").has_value());
    REQUIRE(chess::ParseSan(start, "Nf3").has_value());
    REQUIRE_FALSE(chess::ParseSan(start, "Nd2").has_value());

    const chess::Position castle =
        FromFen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    REQUIRE(chess::ParseSan(castle, "O-O").has_value());
    REQUIRE(chess::ParseSan(castle, "0-0").has_value());
    REQUIRE(chess::ParseSan(castle, "O-O-O").has_value());
    REQUIRE(chess::ParseSan(castle, "0-0-0").has_value());
}
