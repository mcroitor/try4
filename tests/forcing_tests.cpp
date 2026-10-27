#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <limits>

#include "chess/search.hpp"

namespace {

// Small exhaustive minimax oracle, without ordering, alpha-beta or TT. Its
// fixtures finish below the hard bound. Stand-pat is an explicit quiet option.
int Reference(chess::Position& position, int depth, int qply, int ply,
              const chess::SearchLimits& settings) {
    const auto result = position.Result();
    if (result != chess::GameResult::Ongoing) {
        return result == chess::GameResult::Checkmate ? -chess::kMateScore + ply : 0;
    }
    const bool check = position.InCheck(position.SideToMove());
    const int evaluation = chess::Evaluate(position, settings.evaluation);
    if (depth <= 0 && qply >= settings.forcing.max_plies && !check) { return evaluation; }
    REQUIRE(ply < 32);
    int best = depth <= 0 && !check ? evaluation : -1000000;
    for (const auto& move : position.LegalMoves()) {
        const bool capture = move.HasFlag(chess::MoveFlag::Capture) ||
                             move.HasFlag(chess::MoveFlag::EnPassant) ||
                             move.HasFlag(chess::MoveFlag::Promotion);
        REQUIRE(position.MakeMove(move));
        const bool gives_check = position.InCheck(position.SideToMove());
        if (depth > 0 || check || capture || (settings.forcing.checks && gives_check)) {
            best = std::max(best, -Reference(position, depth > 0 ? depth - 1 : 0,
                            depth > 0 ? 0 : qply + 1, ply + 1, settings));
        }
        REQUIRE(position.UndoMove());
    }
    return best;
}

}  // namespace

TEST_CASE("Quiet checking finish of Legal's mate is found beyond depth one", "[forcing]") {
    // After ...Bxd1??, Bxf7+ Ke7 Nd5# is a forced three-ply mate.
    const auto position = chess::Position::FromFen(
        "r2qkbnr/ppp2ppp/2np4/4N3/2B1P3/2N4P/PPPP1PP1/R1BbK2R w KQkq - 0 7");
    REQUIRE(position);
    auto proof = *position;
    for (const auto* uci : {"c4f7", "e8e7", "c3d5"}) {
        const auto legal = proof.LegalMoves();
        const auto move = std::find_if(legal.begin(), legal.end(), [uci](const chess::Move& m) { return m.ToUci() == uci; });
        REQUIRE(move != legal.end());
        REQUIRE(proof.MakeMove(*move));
        if (std::string(uci) == "c4f7") {
            REQUIRE(proof.InCheck(proof.SideToMove()));
            REQUIRE(proof.LegalMoves().size() == 1);
        }
    }
    REQUIRE(proof.Result() == chess::GameResult::Checkmate);
    chess::Searcher searcher;
    chess::SearchLimits limits;
    limits.depth = 1;
    limits.forcing.max_plies = 2;
    std::atomic_bool stop{false};
    const auto legacy = searcher.Search(*position, limits, stop);
    REQUIRE(std::abs(legacy.scoreCp) < chess::kMateThreshold);
    limits.forcing.checks = true;
    const auto forcing = searcher.Search(*position, limits, stop);
    REQUIRE(forcing.depth == 1);
    REQUIRE(forcing.scoreCp == chess::kMateScore - 3);
    REQUIRE(forcing.bestMove->ToUci() == "c4f7");
    REQUIRE(forcing.pv.size() == 3);
    REQUIRE(forcing.pv.back().ToUci() == "c3d5");
}

TEST_CASE("Forcing checks agree with independent bounded exhaustive scores", "[forcing]") {
    // Small king/queen, rook and EP positions permit independent enumeration
    // of checking and capture continuations, including quiet evasions.
    for (const auto* fen : {
        "7k/8/8/4K3/8/8/8/6Q1 w - - 0 1",
        "7k/8/8/8/8/8/6R1/6K1 w - - 0 1",
        "5k2/8/8/8/8/8/8/R3K2R b KQ - 0 1",
        "7k/8/8/3pP3/8/8/8/7K w - d6 0 2"}) {
        auto position = chess::Position::FromFen(fen);
        REQUIRE(position);
        const auto before = *position;
        chess::SearchLimits limits;
        limits.depth = 1;
        limits.forcing.checks = true;
        limits.forcing.max_plies = 2;
        std::atomic_bool stop{false};
        const int reference = Reference(*position, 1, 0, 0, limits);
        REQUIRE(*position == before);
        for (bool optimize : {false, true}) {
            limits.useMoveOrdering = optimize;
            limits.useTranspositionTable = optimize;
            chess::Searcher searcher;
            const auto result = searcher.Search(*position, limits, stop);
            INFO(fen);
            REQUIRE(result.depth == 1);
            REQUIRE(result.scoreCp == reference);
            REQUIRE(result.bestMove);
            REQUIRE(position->IsLegal(*result.bestMove));
            REQUIRE(*position == before);
        }
    }
}

TEST_CASE("Forcing candidates cover castling, promotion, en-passant and discovered checks", "[forcing]") {
    // Each fixture contains a legal checking move of the named kind. Comparing
    // the optimized search with the exhaustive reference proves that the check
    // was a candidate, not merely that it exists in the position.
    struct Fixture {
        const char* fen;
        const char* move;
        const char* kind;
    };
    for (const Fixture fixture : {
             Fixture{"5k2/8/8/8/8/8/8/4K2R w K - 0 1", "e1g1", "checking castle"},
             Fixture{"3k4/1P6/8/8/8/8/8/K7 w - - 0 1", "b7b8q", "promotion check"},
             Fixture{"8/2k5/8/3pP3/8/8/8/7K w - d6 0 2", "e5d6", "en-passant check"},
             Fixture{"k7/8/8/8/N7/8/8/R6K w - - 0 1", "a4b6", "discovered check"}}) {
        auto position = chess::Position::FromFen(fixture.fen);
        REQUIRE(position);
        const auto before = *position;
        const auto legal = position->LegalMoves();
        const auto move = std::find_if(legal.begin(), legal.end(), [&fixture](const chess::Move& candidate) {
            return candidate.ToUci() == fixture.move;
        });
        INFO(fixture.kind);
        REQUIRE(move != legal.end());
        REQUIRE(position->MakeMove(*move));
        REQUIRE(position->InCheck(position->SideToMove()));
        REQUIRE(position->UndoMove());

        chess::SearchLimits limits;
        limits.depth = 1;
        limits.forcing.checks = true;
        limits.forcing.max_plies = 2;
        std::atomic_bool stop{false};
        const int reference = Reference(*position, 1, 0, 0, limits);
        chess::Searcher searcher;
        const auto result = searcher.Search(*position, limits, stop);
        INFO(fixture.fen);
        REQUIRE(result.depth == 1);
        REQUIRE(result.scoreCp == reference);
        REQUIRE(result.bestMove);
        REQUIRE(position->IsLegal(*result.bestMove));
        REQUIRE(*position == before);
    }
}

TEST_CASE("Checked horizon searches the only quiet block instead of a static leaf", "[forcing]") {
    // After Re1+ the black king has no flight square; the only legal evasion is
    // the quiet ...Be5 block. With a zero nominal tactical budget and a checked
    // node, the search must continue to the hard bound or evaluate the block.
    auto position = chess::Position::FromFen(
        "3rkr2/2pp1p1p/8/8/8/2b5/3P3K/4R3 b - - 0 1");
    REQUIRE(position);
    REQUIRE(position->LegalMoves().size() == 1);
    REQUIRE(position->LegalMoves().front().ToUci() == "c3e5");
    chess::SearchLimits limits;
    limits.depth = 1;
    limits.forcing.checks = true;
    limits.forcing.max_plies = 0;
    limits.forcing.hard_plies = 3;
    std::atomic_bool stop{false};
    const int reference = Reference(*position, 1, 0, 0, limits);
    chess::Searcher searcher;
    const auto result = searcher.Search(*position, limits, stop);
    REQUIRE(result.bestMove);
    REQUIRE(result.bestMove->ToUci() == "c3e5");
    REQUIRE(result.scoreCp == reference);
    // The quiet block is not a static leaf: White refutes it with Rxe5#.
    REQUIRE(result.scoreCp < -chess::kMateThreshold);
    REQUIRE(result.pv.size() == 2);
    REQUIRE(result.pv.back().ToUci() == "e1e5");
}

TEST_CASE("Checked hard boundary aborts rather than reporting static exact result", "[forcing]") {
    // After the first quiet rook move, Black has non-capturing checks such as
    // Qh2+. They are not mates (Kf2 is available), so a checked qply=1 node
    // must hit the hard budget rather than receiving a static score.
    const auto position = chess::Position::FromFen(
        "6k1/8/3b4/8/2b4q/8/6P1/R5K1 w - - 0 1");
    REQUIRE(position);
    chess::SearchLimits limits;
    limits.depth = 1;
    limits.forcing.checks = true;
    limits.forcing.max_plies = 1;
    limits.forcing.hard_plies = 1;
    limits.nodes = 10000;
    limits.useMoveOrdering = false;
    chess::Searcher searcher;
    std::atomic_bool stop{false};
    const auto before = *position;
    const auto result = searcher.Search(*position, limits, stop);
    REQUIRE(result.bestMove);
    REQUIRE(position->IsLegal(*result.bestMove));
    REQUIRE(result.depth == 0);
    REQUIRE(result.nodes < 10000);
    REQUIRE(*position == before);
}

TEST_CASE("Forcing checking trees respect cancellation and global node limits", "[forcing]") {
    const auto position = chess::Position::FromFen("7k/8/8/4K3/8/8/8/6Q1 w - - 0 1");
    REQUIRE(position);
    chess::SearchLimits limits;
    limits.depth = 6;
    limits.forcing.checks = true;
    limits.nodeLimit = 100;
    chess::Searcher searcher;
    std::atomic_bool stop{false};
    const auto result = searcher.Search(*position, limits, stop);
    REQUIRE(result.nodes <= 100);
    REQUIRE(result.bestMove);
    REQUIRE(position->IsLegal(*result.bestMove));
    stop = true;
    const auto stopped = searcher.Search(*position, limits, stop);
    REQUIRE(stopped.depth == 0);
    REQUIRE(stopped.bestMove);
}
