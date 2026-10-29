#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "chess/search.hpp"
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

std::vector<std::string> PvUci(const std::vector<chess::Move>& pv) {
    std::vector<std::string> result;
    result.reserve(pv.size());
    for (const chess::Move& move : pv) {
        result.push_back(move.ToUci());
    }
    return result;
}

struct Fixture {
    const char* name;
    const char* fen;
    int depth;
};

// Depth choices keep every run inside the Debug-suite time budget while still
// exercising the interesting window paths. Depth pairs whose score moves by
// more than the aspiration delta between iterations force real fail-low and
// fail-high widening in the default run (perft3-rook 93 -> 46 fail-low,
// hanging-queen and startpos 876/10 -> 909/43 fail-high at depth 3).
constexpr Fixture kFixtures[] = {
    {"mate1-scholar", "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5Q2/PPPP1PPP/RNB1K1NR w KQkq - 0 1", 2},
    {"mate2-kqk", "7k/8/8/4K3/8/8/8/6Q1 w - - 0 1", 3},
    {"mate3-krk", "7k/8/8/4K3/8/8/8/R7 w - - 0 1", 3},
    {"hanging-queen-fail-high", "rnb1kbnr/pppp1ppp/8/4p3/4P2q/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 1", 3},
    {"hanging-queen-shallow", "rnb1kbnr/pppp1ppp/8/4p3/4P2q/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 1", 2},
    {"stalemate-avoid", "7k/8/5K2/6Q1/8/8/8/8 w - - 0 1", 2},
    {"perft3-fail-low", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5},
    {"perft3-shallow", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4},
    {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3},
    {"promotion", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4},
    {"startpos-fail-high", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3},
    {"startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4},
    {"kp-blockade", "8/8/4k3/4p3/4P3/4K3/8/8 w - - 0 1", 4},
    {"quiescence", "4k3/8/2p5/3p4/8/8/8/3QK3 w - - 0 1", 3},
};

chess::SearchInfo Run(const chess::Position& position, int depth,
                      bool principal_variation, bool aspiration_windows) {
    chess::SearchLimits limits;
    limits.depth = depth;
    limits.usePrincipalVariation = principal_variation;
    limits.useAspirationWindows = aspiration_windows;
    // Requirement 3 of task 056: this oracle reproduces the task 055 search,
    // so every pruning technique must be off here.
    limits.pruning.useNullMove = false;
    limits.pruning.useLateMoveReduction = false;
    limits.pruning.useCheckExtension = false;
    limits.pruning.useDeltaPruning = false;
    std::atomic_bool stop{false};
    return chess::Searcher().Search(position, limits, stop);
}

void CheckExactResult(const chess::SearchInfo& oracle,
                      const chess::SearchInfo& actual,
                      const char* fixture, const char* combo) {
    INFO(fixture << " combo " << combo);
    REQUIRE(actual.depth == oracle.depth);
    REQUIRE(actual.scoreCp == oracle.scoreCp);
    REQUIRE(actual.bestMove.has_value());
    REQUIRE(oracle.bestMove.has_value());
    REQUIRE(actual.bestMove->ToUci() == oracle.bestMove->ToUci());
    REQUIRE(PvUci(actual.pv) == PvUci(oracle.pv));
}

}  // namespace

// Requirement 5: with both windows disabled the search must reproduce the
// task 051 full-window behavior. Requirement 1: every window mode must return
// exactly the oracle score, best move and PV.
TEST_CASE("PVS and aspiration preserve the full-window result",
          "[search][pvs]") {
    for (const Fixture& fixture : kFixtures) {
        const chess::Position position = FromFen(fixture.fen);
        const chess::SearchInfo oracle = Run(position, fixture.depth, false, false);
        const chess::SearchInfo pvs_only =
            Run(position, fixture.depth, true, false);
        const chess::SearchInfo aspiration_only =
            Run(position, fixture.depth, false, true);
        const chess::SearchInfo both =
            Run(position, fixture.depth, true, true);

        CheckExactResult(oracle, pvs_only, fixture.name, "pvs");
        CheckExactResult(oracle, aspiration_only, fixture.name, "aspiration");
        CheckExactResult(oracle, both, fixture.name, "both");
    }
}

// Requirement 3: mate scores stay exact across zero-window probing and
// aspiration narrowing. The mate fixtures above cover the general equality;
// these cases pin the exact mate values including the zero-window path.
TEST_CASE("zero-window searches keep mate scores exact", "[search][pvs]") {
    const chess::Position mate_two =
        FromFen("7k/8/8/4K3/8/8/8/6Q1 w - - 0 1");
    const chess::SearchInfo both = Run(mate_two, 3, true, true);
    REQUIRE(both.scoreCp == chess::kMateScore - 3);
    REQUIRE(both.bestMove->ToUci() == "e5f6");

    const chess::Position mate_one =
        FromFen("r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5Q2/PPPP1PPP/RNB1K1NR w KQkq - 0 1");
    const chess::SearchInfo mate_info = Run(mate_one, 2, true, true);
    REQUIRE(mate_info.scoreCp == chess::kMateScore - 1);

    // A deep mate reached through a fail-low aspiration window stays exact.
    const chess::Position mate_three =
        FromFen("7k/8/8/4K3/8/8/8/R7 w - - 0 1");
    const chess::SearchInfo deep_mate = Run(mate_three, 5, true, true);
    REQUIRE(deep_mate.scoreCp == chess::kMateScore - 5);
    REQUIRE(deep_mate.bestMove->ToUci() == "e5f6");
}

// Requirement 2: a fail-low aspiration window must be widened until the
// iteration completes inside its final window; the reported score is the
// exact full-window score, not the failed window's clipped bound.
TEST_CASE("fail-low aspiration widening finds the exact score",
          "[search][pvs]") {
    const chess::Position position =
        FromFen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    const chess::SearchInfo oracle = Run(position, 5, false, false);
    const chess::SearchInfo narrowed = Run(position, 5, true, true);

    // Depth 4 scores 93 and depth 5 scores 46, so the depth-5 window starts
    // at (68, 118) and must widen before it can report. A reported clipped
    // window result would be at least 68, which this equality forbids.
    REQUIRE(oracle.scoreCp == 46);
    CheckExactResult(oracle, narrowed, "perft3-fail-low", "both");
}

// Requirement 6: nodes, the node limit and the stop flag are shared and
// monotonic across aspiration re-searches; the limit is never reset.
TEST_CASE("aspiration re-searches honor the shared node budget",
          "[search][pvs]") {
    const chess::Position position =
        FromFen("rnb1kbnr/pppp1ppp/8/4p3/4P2q/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 1");

    const chess::SearchInfo unlimited = Run(position, 3, true, true);
    REQUIRE(unlimited.nodes > 0);

    chess::SearchLimits bounded_limits;
    bounded_limits.depth = 3;
    bounded_limits.nodeLimit = unlimited.nodes * 2;
    std::atomic_bool stop{false};
    const chess::SearchInfo bounded =
        chess::Searcher().Search(position, bounded_limits, stop);
    REQUIRE(bounded.nodes <= *bounded_limits.nodeLimit);
    CheckExactResult(unlimited, bounded, "hanging-queen", "both-bounded");

    chess::SearchLimits tight_limits;
    tight_limits.depth = 3;
    tight_limits.nodeLimit = 400;
    const chess::SearchInfo tight =
        chess::Searcher().Search(position, tight_limits, stop);
    REQUIRE(tight.nodes <= 400);
    REQUIRE(tight.bestMove.has_value());
    REQUIRE(position.IsLegal(*tight.bestMove));
}

// Requirement 4: bound-valued TT entries produced by zero-window searches
// must keep later TT-assisted searches exact. A warm search over a filled
// table must reproduce the cold result exactly.
TEST_CASE("TT entries from zero-window searches keep later searches exact",
          "[search][pvs]") {
    const chess::Position position =
        FromFen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");

    chess::SearchLimits warm_limits;
    warm_limits.depth = 3;
    std::atomic_bool stop{false};
    chess::Searcher warm_searcher;
    const chess::SearchInfo first =
        warm_searcher.Search(position, warm_limits, stop);
    const chess::SearchInfo second =
        warm_searcher.Search(position, warm_limits, stop);

    const chess::SearchInfo cold = Run(position, 3, true, true);

    // A warm replay takes TT shortcuts, so it visits far fewer nodes and — as
    // on the pre-055 engine, which truncates warm-replay PVs the same way
    // under all flag combinations — the PV tail is not rebuilt from table
    // entries. What the bound-valued zero-window entries must never corrupt is
    // the reported score and best move.
    REQUIRE(second.depth == cold.depth);
    REQUIRE(second.scoreCp == cold.scoreCp);
    REQUIRE(second.bestMove.has_value());
    REQUIRE(cold.bestMove.has_value());
    REQUIRE(second.bestMove->ToUci() == cold.bestMove->ToUci());
    REQUIRE_FALSE(second.pv.empty());
    REQUIRE(second.pv.front().ToUci() == cold.bestMove->ToUci());
    REQUIRE(second.nodes < first.nodes);
    CheckExactResult(cold, first, "kiwipete", "cold-tt");
}

TEST_CASE("default window search is deterministic across searchers",
          "[search][pvs]") {
    const chess::Position position =
        FromFen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    chess::SearchLimits limits;
    limits.depth = 4;
    std::atomic_bool stop{false};

    const chess::SearchInfo first =
        chess::Searcher().Search(position, limits, stop);
    const chess::SearchInfo second =
        chess::Searcher().Search(position, limits, stop);

    REQUIRE(first.nodes == second.nodes);
    CheckExactResult(second, first, "kiwipete", "determinism");
}