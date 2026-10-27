#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "chess/movegen.hpp"
#include "chess/position.hpp"
#include "test_support.hpp"

namespace {

struct Snapshot {
    std::string fen;
    std::string board;
    std::uint64_t hash = 0;
    std::size_t history_depth = 0;
};

Snapshot TakeSnapshot(const chess::Position& position) {
    return {position.ToFen(), position.ToString(), position.Hash(),
            position.HistoryDepth()};
}

bool SameSnapshot(const Snapshot& lhs, const Snapshot& rhs) {
    return lhs.fen == rhs.fen && lhs.board == rhs.board &&
           lhs.hash == rhs.hash && lhs.history_depth == rhs.history_depth;
}

bool HasMove(const chess::MoveList& moves, const char* from, const char* to,
             chess::MoveFlag flags = chess::MoveFlag::None,
             chess::PieceType promotion = chess::PieceType::None) {
    const chess::Move expected = try4_test::Move(from, to, flags, promotion);
    for (const chess::Move& move : moves) {
        if (move == expected) {
            return true;
        }
    }
    return false;
}

void RequireSequenceEqual(const chess::Position& position) {
    const std::vector<chess::Move> expected = position.LegalMoves();
    chess::MoveList actual;
    const std::size_t count = position.GenerateLegalMoves(&actual);

    REQUIRE(count == actual.Size());
    REQUIRE(count == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        INFO("move index: " << i);
        REQUIRE(actual[i] == expected[i]);
    }
}

}  // namespace

TEST_CASE("GenerateLegalMoves preserves the LegalMoves sequence",
          "[position][property]") {
    constexpr int kSamples = 10000;
    constexpr std::uint64_t kSeed = 0x050A11B0C0FFEEULL;

    std::mt19937_64 rng(kSeed);
    chess::Position position;
    position.SetupInitial();

    for (int sample = 0; sample < kSamples; ++sample) {
        INFO("sample: " << sample);
        const Snapshot before = TakeSnapshot(position);
        RequireSequenceEqual(position);
        REQUIRE(SameSnapshot(before, TakeSnapshot(position)));

        const std::vector<chess::Move> legal = position.LegalMoves();
        if (legal.empty()) {
            position.SetupInitial();
            continue;
        }

        std::uniform_int_distribution<std::size_t> choose(0, legal.size() - 1);
        REQUIRE(position.MakeMove(legal[choose(rng)]));
    }
}

TEST_CASE("MakeMoveAssumeLegal matches MakeMove state transitions",
          "[position][property]") {
    constexpr int kSamples = 1000;
    constexpr std::uint64_t kSeed = 0xA55E7E6A1ULL;

    std::mt19937_64 rng(kSeed);
    chess::Position position;
    position.SetupInitial();

    for (int sample = 0; sample < kSamples; ++sample) {
        INFO("sample: " << sample);
        const std::vector<chess::Move> legal = position.LegalMoves();
        if (legal.empty()) {
            position.SetupInitial();
            continue;
        }

        std::uniform_int_distribution<std::size_t> choose(0, legal.size() - 1);
        const chess::Move move = legal[choose(rng)];
        const Snapshot before = TakeSnapshot(position);

        chess::Position regular = position;
        chess::Position unchecked = position;
        REQUIRE(regular.MakeMove(move));
        REQUIRE(unchecked.MakeMoveAssumeLegal(move));
        REQUIRE(SameSnapshot(TakeSnapshot(regular), TakeSnapshot(unchecked)));

        REQUIRE(unchecked.UndoMove());
        REQUIRE(SameSnapshot(before, TakeSnapshot(unchecked)));
        REQUIRE(position.MakeMove(move));
    }
}

TEST_CASE("GenerateLegalMoves handles castling, en passant and promotion",
          "[position]") {
    const char* const fixtures[] = {
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
        "4k3/P7/8/8/8/8/8/4K3 w - - 0 1",
    };

    for (const char* fen : fixtures) {
        INFO("FEN: " << fen);
        std::string error;
        const auto parsed = chess::Position::FromFen(fen, &error);
        REQUIRE(parsed.has_value());
        const chess::Position& position = *parsed;
        const Snapshot before = TakeSnapshot(position);
        RequireSequenceEqual(position);
        REQUIRE(SameSnapshot(before, TakeSnapshot(position)));

        chess::MoveList moves;
        position.GenerateLegalMoves(&moves);
        if (std::string(fen).find("R3K2R") != std::string::npos) {
            REQUIRE(HasMove(moves, "e1", "g1",
                            chess::MoveFlag::CastleKing));
            REQUIRE(HasMove(moves, "e1", "c1",
                            chess::MoveFlag::CastleQueen));
        } else if (std::string(fen).find("d6") != std::string::npos) {
            REQUIRE(HasMove(moves, "e5", "d6",
                            chess::MoveFlag::Capture |
                                chess::MoveFlag::EnPassant));
        } else {
            REQUIRE(HasMove(moves, "a7", "a8",
                            chess::MoveFlag::Promotion,
                            chess::PieceType::Queen));
        }
    }
}

TEST_CASE("IsRepetition matches threefold repetition semantics",
          "[position][draw]") {
    chess::Position position;
    position.SetupInitial();

    const char* const cycle[][2] = {
        {"g1", "f3"}, {"g8", "f6"}, {"f3", "g1"}, {"f6", "g8"},
    };

    for (int repetition = 0; repetition < 2; ++repetition) {
        for (const auto& move : cycle) {
            REQUIRE(position.MakeMove(try4_test::Move(move[0], move[1])));
        }
        if (repetition == 0) {
            REQUIRE_FALSE(position.IsRepetition());
            REQUIRE(position.Result() !=
                    chess::GameResult::DrawRepetition);
        } else {
            REQUIRE(position.IsRepetition());
            REQUIRE(position.Result() ==
                    chess::GameResult::DrawRepetition);
        }
    }

    position.SetHalfmoveClock(100);
    REQUIRE(position.IsRepetition());
    REQUIRE(position.Result() == chess::GameResult::DrawFiftyMove);
}

#ifndef NDEBUG
TEST_CASE("MakeMoveAssumeLegal rejects illegal moves in debug builds",
          "[position][debug]") {
    chess::Position position;
    position.SetupInitial();
    const Snapshot before = TakeSnapshot(position);

    REQUIRE_THROWS_AS(position.MakeMoveAssumeLegal(
                          try4_test::Move("e2", "e5")),
                      std::logic_error);
    REQUIRE(SameSnapshot(before, TakeSnapshot(position)));
    REQUIRE(position.HistoryDepth() == 0);
}
#endif

TEST_CASE("ReserveHistory preserves position and history semantics",
          "[position]") {
    chess::Position position;
    position.SetupInitial();
    REQUIRE(position.MakeMove(try4_test::Move("e2", "e4")));

    const Snapshot before = TakeSnapshot(position);
    position.ReserveHistory(256);
    REQUIRE(SameSnapshot(before, TakeSnapshot(position)));
    REQUIRE(position.HistoryDepth() == 1);

    position.ReserveHistory(0);
    REQUIRE(SameSnapshot(before, TakeSnapshot(position)));

    REQUIRE(position.MakeMove(try4_test::Move("e7", "e5")));
    REQUIRE(position.HistoryDepth() == 2);
    REQUIRE(position.UndoMove());
    REQUIRE(position.HistoryDepth() == 1);
    REQUIRE(SameSnapshot(before, TakeSnapshot(position)));
}
