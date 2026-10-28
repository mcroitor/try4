#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <random>
#include <string>

#include "chess/evaluation.hpp"
#include "chess/position.hpp"

namespace {

void CheckBreakdownEqual(const chess::EvaluationBreakdown& expected,
                         const chess::EvaluationBreakdown& actual) {
    CHECK(expected.middlegame == actual.middlegame);
    CHECK(expected.endgame == actual.endgame);
    CHECK(expected.phase == actual.phase);
    CHECK(expected.score == actual.score);
    CHECK(expected.pawn_structure_mg == actual.pawn_structure_mg);
    CHECK(expected.pawn_structure_eg == actual.pawn_structure_eg);
    CHECK(expected.mobility_mg == actual.mobility_mg);
    CHECK(expected.mobility_eg == actual.mobility_eg);
    CHECK(expected.bishop_pair_mg == actual.bishop_pair_mg);
    CHECK(expected.bishop_pair_eg == actual.bishop_pair_eg);
    CHECK(expected.rook_files_mg == actual.rook_files_mg);
    CHECK(expected.rook_files_eg == actual.rook_files_eg);
    CHECK(expected.king_safety_mg == actual.king_safety_mg);
    CHECK(expected.king_safety_eg == actual.king_safety_eg);
    CHECK(expected.tempo == actual.tempo);
}

// A second, still valid parameter set: different material and PST percentages
// with all bounds respected, so the equivalence covers parameterized inputs.
chess::EvaluationParameters CustomParameters() {
    chess::EvaluationParameters parameters;
    parameters.material_mg[1] = 90;
    parameters.material_mg[2] = 300;
    parameters.material_mg[3] = 310;
    parameters.material_mg[4] = 500;
    parameters.material_mg[5] = 800;
    parameters.material_eg[1] = 105;
    parameters.material_eg[2] = 290;
    parameters.material_eg[3] = 305;
    parameters.material_eg[4] = 520;
    parameters.material_eg[5] = 850;
    parameters.passed_mg[6] = 60;
    parameters.passed_eg[6] = 90;
    parameters[ chess::EvalWeight::DoubledPawnMg ] = 8;
    parameters[ chess::EvalWeight::RookOpenEg ] = 12;
    parameters[ chess::EvalWeight::ShieldOneEg ] = 4;
    parameters[ chess::EvalWeight::PstMgPercent ] = 80;
    parameters[ chess::EvalWeight::PstEgPercent ] = 110;
    return parameters;
}

}  // namespace

// Requirement 1 in task 054: Evaluate/EvaluateDetailed and their pre-validated
// fast paths must agree on every field across a frozen corpus. The corpus is a
// seeded random legal playout (>= 10,000 positions) plus tactical fixtures, so
// reruns are byte-identical by construction.
TEST_CASE("fast evaluation matches the validated public evaluation",
          "[evaluation]") {
    constexpr std::uint64_t kSeed = 0x05400010ULL;
    constexpr int kCorpusPositions = 10000;
    constexpr int kMaxPliesPerGame = 300;

    std::mt19937_64 rng(kSeed);
    const chess::EvaluationParameters default_parameters;
    const chess::EvaluationParameters custom_parameters = CustomParameters();

    chess::Position position;
    position.SetupInitial();
    chess::MoveList moves;
    const char* fixtures[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3P4/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        // Promotion race with kings in the corners (a7 promotes to a8).
        "8/P6k/8/8/8/8/6p1/K7 w - - 0 1",
        "8/2p5/3p4/KP5r/8/4p1k1/8/8 w - - 0 1",
        // Open rook files against a king zone.
        "8/8/8/R6k/8/8/5r1K/8 w - - 0 1",
        "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQ1RK1 b - - 5 4",
        "8/8/8/bqk5/8/3K4/8/4R3 w - - 0 1",
    };

    int built = 0;
    int plies = 0;
    while (built < kCorpusPositions) {
        const chess::EvaluationBreakdown expected =
            chess::EvaluateDetailed(position, default_parameters);
        const chess::EvaluationBreakdown actual =
            chess::EvaluateDetailedFast(position, default_parameters);
        CheckBreakdownEqual(expected, actual);
        CHECK(chess::Evaluate(position, default_parameters) ==
              chess::EvaluateFast(position, default_parameters));
        CHECK(chess::EvaluateMaterial(position, default_parameters) ==
              chess::EvaluateMaterialFast(position, default_parameters));

        const chess::EvaluationBreakdown custom_expected =
            chess::EvaluateDetailed(position, custom_parameters);
        const chess::EvaluationBreakdown custom_actual =
            chess::EvaluateDetailedFast(position, custom_parameters);
        CheckBreakdownEqual(custom_expected, custom_actual);
        CHECK(chess::EvaluateMaterial(position, custom_parameters) ==
              chess::EvaluateMaterialFast(position, custom_parameters));
        ++built;

        position.GenerateLegalMoves(&moves);
        if (moves.Empty() || plies >= kMaxPliesPerGame) {
            position.SetupInitial();
            plies = 0;
            continue;
        }
        const chess::Move move = moves[rng() % moves.Size()];
        position.MakeMoveAssumeLegal(move);
        ++plies;
    }
    CHECK(built == kCorpusPositions);

    for (const char* fen : fixtures) {
        const auto position_option =
            chess::Position::FromFen(fen);
        REQUIRE(position_option.has_value());
        const chess::Position fixture_position = *position_option;
        const chess::EvaluationBreakdown expected =
            chess::EvaluateDetailed(fixture_position, default_parameters);
        const chess::EvaluationBreakdown actual =
            chess::EvaluateDetailedFast(fixture_position, default_parameters);
        CheckBreakdownEqual(expected, actual);
        CHECK(chess::Evaluate(fixture_position, default_parameters) ==
              chess::EvaluateFast(fixture_position, default_parameters));
        CHECK(chess::EvaluateMaterial(fixture_position, default_parameters) ==
              chess::EvaluateMaterialFast(fixture_position, default_parameters));
    }

    // King-less boards are rejected by FromFen (exactly one king per side is
    // required), so build one directly to keep the missing-king guard in both
    // paths covered by the equivalence test.
    chess::Position kingless;
    kingless.SetupInitial();
    REQUIRE(kingless.KingSquare(chess::Color::White).has_value());
    REQUIRE(kingless.KingSquare(chess::Color::Black).has_value());
    kingless.SetPiece(*kingless.KingSquare(chess::Color::White), chess::Piece{});
    kingless.SetPiece(*kingless.KingSquare(chess::Color::Black), chess::Piece{});
    const chess::EvaluationBreakdown kingless_expected =
        chess::EvaluateDetailed(kingless, default_parameters);
    const chess::EvaluationBreakdown kingless_actual =
        chess::EvaluateDetailedFast(kingless, default_parameters);
    CheckBreakdownEqual(kingless_expected, kingless_actual);
    CHECK(chess::Evaluate(kingless, default_parameters) ==
          chess::EvaluateFast(kingless, default_parameters));
    CHECK(chess::EvaluateMaterial(kingless, default_parameters) ==
          chess::EvaluateMaterialFast(kingless, default_parameters));
}

TEST_CASE("public evaluation keeps zeroed behavior for invalid parameters",
          "[evaluation]") {
    chess::EvaluationParameters invalid;
    invalid.weights[static_cast<std::size_t>(chess::EvalWeight::Tempo)] = -1;
    REQUIRE_FALSE(chess::ValidateEvaluation(invalid));

    chess::Position position;
    position.SetupInitial();

    const chess::EvaluationBreakdown breakdown =
        chess::EvaluateDetailed(position, invalid);
    CHECK(breakdown.middlegame == 0);
    CHECK(breakdown.endgame == 0);
    CHECK(breakdown.phase == 0);
    CHECK(breakdown.score == 0);
    CHECK(breakdown.tempo == 0);
    CHECK(breakdown.pawn_structure_mg == 0);
    CHECK(breakdown.mobility_mg == 0);
    CHECK(breakdown.king_safety_eg == 0);
    CHECK(chess::Evaluate(position, invalid) == 0);
    CHECK(chess::EvaluateMaterial(position, invalid) == 0);
}