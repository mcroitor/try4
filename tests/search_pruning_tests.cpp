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

chess::PruningSettings NothingEnabled() {
    chess::PruningSettings settings;
    settings.useNullMove = false;
    settings.useLateMoveReduction = false;
    settings.useCheckExtension = false;
    settings.useDeltaPruning = false;
    return settings;
}

chess::SearchInfo Run(const chess::Position& position, int depth,
                      const chess::PruningSettings& pruning) {
    chess::SearchLimits limits;
    limits.depth = depth;
    limits.pruning = pruning;
    std::atomic_bool stop{false};
    return chess::Searcher().Search(position, limits, stop);
}

std::vector<std::string> PvUci(const std::vector<chess::Move>& pv) {
    std::vector<std::string> result;
    result.reserve(pv.size());
    for (const chess::Move& move : pv) {
        result.push_back(move.ToUci());
    }
    return result;
}

}  // namespace

TEST_CASE("null move make/undo restores the exact position and hash",
          "[search][pruning]") {
    chess::Position en_passant =
        FromFen("rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3");
    const chess::Position before = en_passant;
    const std::uint64_t hash_before = en_passant.Hash();

    REQUIRE(en_passant.MakeNullMove());
    CHECK(en_passant.SideToMove() == chess::Color::Black);
    CHECK_FALSE(en_passant.EnPassant().has_value());
    CHECK(en_passant.CastlingRights() == before.CastlingRights());
    CHECK(en_passant.HalfmoveClock() == before.HalfmoveClock());
    CHECK(en_passant.FullmoveNumber() == before.FullmoveNumber());

    REQUIRE(en_passant.UndoMove());
    CHECK(en_passant == before);
    CHECK(en_passant.Hash() == hash_before);
    CHECK(en_passant.Hash() == en_passant.ComputeHash());
    CHECK(en_passant.UndoMove() == false);

    chess::Position plain;
    plain.SetupInitial();
    const chess::Position plain_before = plain;
    REQUIRE(plain.MakeNullMove());
    REQUIRE(plain.UndoMove());
    CHECK(plain == plain_before);
    CHECK(plain.Hash() == plain.ComputeHash());
}

// Task 056 requirement 3: with every technique disabled the search must
// reproduce the task 055 engine exactly. 2029 nodes is the measured task 055
// node count for this fixture and depth, so this also proves the node
// accounting did not change.
TEST_CASE("pruning disabled reproduces the task 055 search", "[search][pruning]") {
    const chess::Position position = FromFen("7k/8/8/4K3/8/8/8/6Q1 w - - 0 1");
    const chess::SearchInfo oracle = Run(position, 3, NothingEnabled());

    REQUIRE(oracle.depth == 3);
    REQUIRE(oracle.scoreCp == chess::kMateScore - 3);
    REQUIRE(oracle.nodes == 2029);
    REQUIRE(oracle.bestMove->ToUci() == "e5f6");
    REQUIRE(PvUci(oracle.pv) ==
            std::vector<std::string>{"e5f6", "h8h7", "g1g7"});

    // Deterministic across searchers.
    const chess::SearchInfo again = Run(position, 3, NothingEnabled());
    CHECK(again.nodes == oracle.nodes);
    CHECK(again.scoreCp == oracle.scoreCp);
}

// Task 056 requirement 2: null move in king-and-pawn endings. Both sides hold
// only king and pawns, so the mover guard disables the technique at every
// node and the search must be node-for-node identical to the disabled run.
TEST_CASE("null move never fires in king and pawn endings", "[search][pruning]") {
    const chess::Position zugzwang = FromFen("8/3p4/8/8/8/8/1P6/K6k w - - 0 1");
    chess::PruningSettings null_on = NothingEnabled();
    null_on.useNullMove = true;
    const chess::SearchInfo enabled = Run(zugzwang, 5, null_on);
    const chess::SearchInfo disabled = Run(zugzwang, 5, NothingEnabled());

    CHECK(enabled.nodes == disabled.nodes);
    CHECK(enabled.scoreCp == disabled.scoreCp);
    CHECK(enabled.depth == disabled.depth);
    CHECK(enabled.bestMove->ToUci() == disabled.bestMove->ToUci());
}

// Task 056 requirements 2 and 5: mate verdicts stay exact under every
// technique. The mate band, in-check and near-mate delta guards must
// guarantee the curated corpus mates even when every technique is enabled.
TEST_CASE("mate verdicts stay exact with every pruning technique enabled",
          "[search][pruning]") {
    const chess::PruningSettings all_on;  // production defaults
    const struct {
        const char* fen;
        int depth;
        int mate_plies;
        const char* best;
    } mates[] = {
        {"r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5Q2/PPPP1PPP/RNB1K1NR w KQkq - 0 1", 1,
         1, "f3f7"},
        {"7k/8/8/4K3/8/8/8/6Q1 w - - 0 1", 3, 3, "e5f6"},
        {"7k/8/8/4K3/8/8/8/R7 w - - 0 1", 5, 5, "e5f6"},
        {"7k/8/5K2/6Q1/8/8/8/8 w - - 0 1", 1, 1, "g5g7"},
    };
    for (const auto& mate : mates) {
        const chess::Position position = FromFen(mate.fen);
        const chess::SearchInfo enabled = Run(position, mate.depth, all_on);
        const chess::SearchInfo disabled =
            Run(position, mate.depth, NothingEnabled());

        CHECK(enabled.scoreCp == chess::kMateScore - mate.mate_plies);
        CHECK(disabled.scoreCp == chess::kMateScore - mate.mate_plies);
        CHECK(enabled.bestMove->ToUci() == mate.best);
        CHECK(disabled.bestMove->ToUci() == mate.best);
        CHECK(PvUci(enabled.pv) == PvUci(disabled.pv));
    }
}

// Task 056 requirement 2: the null-move window guards. The stalemate trap
// must keep its verdict with null-move pruning explicitly enabled.
TEST_CASE("null move does not change stalemate traps", "[search][pruning]") {
    // Qg6 would stalemate; the winning move is the immediate queen mate.
    const chess::Position position = FromFen("7k/8/5K2/6Q1/8/8/8/8 w - - 0 1");
    chess::PruningSettings null_on = NothingEnabled();
    null_on.useNullMove = true;
    const chess::SearchInfo enabled = Run(position, 2, null_on);
    const chess::SearchInfo disabled = Run(position, 2, NothingEnabled());

    CHECK(enabled.scoreCp == chess::kMateScore - 1);
    CHECK(disabled.scoreCp == chess::kMateScore - 1);
    CHECK(enabled.bestMove->ToUci() == "g5g7");
}

// Task 056 requirement 2: the LMR guards never reduce the mating capture that
// gives check, so the scholar's mate is found even when reduction is
// explicitly enabled with an aggressive threshold.
TEST_CASE("LMR never reduces the mating capture", "[search][pruning]") {
    const chess::Position position = FromFen(
        "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5Q2/PPPP1PPP/RNB1K1NR w KQkq - 0 1");
    chess::PruningSettings lmr_on = NothingEnabled();
    lmr_on.useLateMoveReduction = true;
    lmr_on.lateMoveReductionMinDepth = 2;
    lmr_on.lateMoveReductionMinMoveNumber = 0;
    const chess::SearchInfo info = Run(position, 1, lmr_on);

    CHECK(info.scoreCp == chess::kMateScore - 1);
    CHECK(info.bestMove->ToUci() == "f3f7");
}

// Task 056 requirement 2: the check extension budget is bounded. A search
// with the extension enabled must terminate at the requested depth on a
// forcing checking position, with the ply guard intact.
TEST_CASE("check extension stays bounded and terminates", "[search][pruning]") {
    // Winning queen-and-king position with forced checking lines available.
    const chess::Position queen_end = FromFen(
        "8/8/4k3/8/8/4K3/8/6Q1 w - - 0 1");
    chess::PruningSettings ext_on = NothingEnabled();
    ext_on.useCheckExtension = true;
    ext_on.checkExtensionCap = 4;
    const chess::SearchInfo enabled = Run(queen_end, 6, ext_on);
    const chess::SearchInfo disabled = Run(queen_end, 6, NothingEnabled());

    CHECK(enabled.depth <= 6);
    CHECK(disabled.depth <= 6);
    CHECK(enabled.bestMove.has_value());
}

// Task 056 requirement 6: in queen endings the opponent holds a lone king, so
// the opponent guard blocks every null probe and the node counts must match
// the disabled run exactly.
TEST_CASE("null move probes stay affordable in queen endings", "[search][pruning]") {
    const chess::Position queens = FromFen("8/8/4k3/8/8/4K3/8/6Q1 w - - 0 1");
    chess::PruningSettings null_on = NothingEnabled();
    null_on.useNullMove = true;
    const chess::SearchInfo enabled = Run(queens, 6, null_on);
    const chess::SearchInfo disabled = Run(queens, 6, NothingEnabled());

    CHECK(enabled.nodes == disabled.nodes);
    CHECK(enabled.scoreCp == disabled.scoreCp);
    CHECK(enabled.bestMove->ToUci() == disabled.bestMove->ToUci());
    CHECK(PvUci(enabled.pv) == PvUci(disabled.pv));
}