#include <catch2/catch_test_macros.hpp>

#include <cstdint>
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

TEST_CASE("zobrist hash is deterministic and matches recomputation", "[draw]") {
    chess::Position first;
    first.SetupInitial();
    chess::Position second;
    second.SetupInitial();

    REQUIRE(first.Hash() != 0);
    REQUIRE(first.Hash() == first.ComputeHash());
    REQUIRE(second.Hash() == second.ComputeHash());
    REQUIRE(first.Hash() == second.Hash());

    REQUIRE(first.MakeMove(try4_test::Move("e2", "e4")));
    REQUIRE(first.Hash() == first.ComputeHash());
    const std::uint64_t after_e4 = first.Hash();
    REQUIRE(after_e4 != second.Hash());

    REQUIRE(first.UndoMove());
    REQUIRE(first.Hash() == second.Hash());
    REQUIRE(first.Hash() == first.ComputeHash());
}

TEST_CASE("hash covers side, castling, and effective en passant", "[draw]") {
    const chess::Position white =
        FromFen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    const chess::Position black =
        FromFen("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
    REQUIRE(white.Hash() != black.Hash());

    const chess::Position no_rights =
        FromFen("r3k2r/8/8/8/8/8/8/R3K2R w - - 0 1");
    REQUIRE(white.Hash() != no_rights.Hash());

    const chess::Position with_ep =
        FromFen("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    const chess::Position without_ep =
        FromFen("4k3/8/8/3pP3/8/8/8/4K3 w - - 0 1");
    REQUIRE(with_ep.Hash() != without_ep.Hash());

    // An en passant target with no adjacent pawn does not change the
    // position for repetition purposes.
    const chess::Position ineffective_ep =
        FromFen("4k3/8/8/4P3/8/8/8/4K3 w - d6 0 1");
    const chess::Position no_ep_target =
        FromFen("4k3/8/8/4P3/8/8/8/4K3 w - - 0 1");
    REQUIRE(ineffective_ep.Hash() == no_ep_target.Hash());
}

TEST_CASE("hash remains correct through special moves", "[draw]") {
    struct Case {
        const char* fen;
        const char* from;
        const char* to;
        chess::MoveFlag flags;
        chess::PieceType promotion;
    };

    const Case cases[] = {
        {"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "e1", "g1",
         chess::MoveFlag::CastleKing, chess::PieceType::None},
        {"4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", "e5", "d6",
         chess::MoveFlag::Capture | chess::MoveFlag::EnPassant,
         chess::PieceType::None},
        {"4k3/P7/8/8/8/8/8/4K3 w - - 0 1", "a7", "a8",
         chess::MoveFlag::Promotion, chess::PieceType::Queen},
    };

    for (const Case& test : cases) {
        INFO("FEN: " << test.fen);
        chess::Position position = FromFen(test.fen);
        const std::uint64_t original = position.Hash();
        REQUIRE(original == position.ComputeHash());

        REQUIRE(position.MakeMove(try4_test::Move(test.from, test.to,
                                                  test.flags,
                                                  test.promotion)));
        REQUIRE(position.Hash() == position.ComputeHash());
        REQUIRE(position.UndoMove());
        REQUIRE(position.Hash() == original);
        REQUIRE(position.Hash() == position.ComputeHash());
    }
}

TEST_CASE("fifty-move draw triggers at 100 halfmoves", "[draw]") {
    chess::Position position =
        FromFen("k7/8/8/8/8/8/8/K6R w - - 99 50");
    REQUIRE(position.Result() == chess::GameResult::Ongoing);

    REQUIRE(position.MakeMove(try4_test::Move("h1", "g1")));
    REQUIRE(position.HalfmoveClock() == 100);
    REQUIRE(position.Result() == chess::GameResult::FiftyMove);

    chess::Position already =
        FromFen("k7/8/8/8/8/8/8/K6R w - - 100 50");
    REQUIRE(already.Result() == chess::GameResult::FiftyMove);
}

TEST_CASE("checkmate takes precedence over the fifty-move rule", "[draw]") {
    chess::Position position =
        FromFen("R5k1/5ppp/8/8/8/8/8/6K1 b - - 100 1");
    REQUIRE(position.InCheck(chess::Color::Black));
    REQUIRE(position.Result() == chess::GameResult::Checkmate);
}

TEST_CASE("threefold repetition is detected across reversible moves",
          "[draw]") {
    chess::Position position;
    position.SetupInitial();
    const std::uint64_t start_hash = position.Hash();

    const char* const cycle[][2] = {
        {"g1", "f3"}, {"g8", "f6"}, {"f3", "g1"}, {"f6", "g8"},
    };

    for (int repetition = 0; repetition < 2; ++repetition) {
        for (const auto& move : cycle) {
            Play(&position, move[0], move[1]);
        }
        REQUIRE(position.Hash() == start_hash);
        if (repetition == 0) {
            REQUIRE(position.Result() == chess::GameResult::Ongoing);
        }
    }

    REQUIRE(position.HistoryDepth() == 8);
    REQUIRE(position.Result() == chess::GameResult::ThreefoldRepetition);
}

TEST_CASE("insufficient material cases are detected", "[draw]") {
    const char* const draws[] = {
        "7k/8/8/8/8/8/8/K7 w - - 0 1",
        "7k/8/8/8/8/8/8/K1B5 w - - 0 1",
        "7k/8/8/8/8/8/8/K1N5 w - - 0 1",
        "5b1k/8/8/8/8/8/8/K1B5 w - - 0 1",
    };

    for (const char* fen : draws) {
        INFO("FEN: " << fen);
        const chess::Position position = FromFen(fen);
        REQUIRE(position.Result() == chess::GameResult::InsufficientMaterial);
    }
}

TEST_CASE("sufficient material cases are not drawn", "[draw]") {
    const char* const ongoing[] = {
        // Opposite-colored bishops can still mate with help.
        "6bk/8/8/8/8/8/8/K1B5 w - - 0 1",
        // Two knights on one side are not covered by the draw rule.
        "7k/8/8/8/8/8/8/K1N1N3 w - - 0 1",
        // A pawn makes the position sufficient even with same-color bishops.
        "5b1k/8/8/8/8/8/P7/K1B5 w - - 0 1",
    };

    for (const char* fen : ongoing) {
        INFO("FEN: " << fen);
        const chess::Position position = FromFen(fen);
        REQUIRE(position.Result() == chess::GameResult::Ongoing);
    }
}
