#ifndef CHESS_EVALUATION_HPP_
#define CHESS_EVALUATION_HPP_

#include <array>
#include <string>

#include "chess/position.hpp"

namespace chess {

enum class EvalWeight : std::size_t {
    MobilityMg, MobilityEg, BishopPairMg, BishopPairEg,
    RookOpenMg, RookOpenEg, RookSemiOpenMg, RookSemiOpenEg,
    DoubledPawnMg, DoubledPawnEg, IsolatedPawnMg, IsolatedPawnEg,
    ShieldOneMg, ShieldTwoMg, ShieldMissingMg,
    ShieldOneEg, ShieldTwoEg, ShieldMissingEg,
    KingAttackerMg, KingAttackerEg, Tempo, PstMgPercent, PstEgPercent, Count
};

// Values are centipawns; penalty weights are nonnegative magnitudes. PST
// percentages scale the built-in tables, not their individual cells.
struct EvaluationParameters {
    std::array<int, 7> material_mg{{0, 100, 320, 330, 500, 900, 0}};
    std::array<int, 7> material_eg{{0, 120, 300, 320, 540, 900, 0}};
    std::array<int, 8> passed_mg{{0, 5, 10, 20, 35, 55, 80, 0}};
    std::array<int, 8> passed_eg{{0, 10, 20, 35, 55, 80, 120, 0}};
    std::array<int, static_cast<std::size_t>(EvalWeight::Count)> weights{{
        3, 2, 30, 50, 20, 10, 10, 5, 15, 20, 12, 15,
        12, 6, 10, 3, 1, 2, 12, 4, 10, 100, 100}};
    int operator[](EvalWeight weight) const { return weights[static_cast<std::size_t>(weight)]; }
    int& operator[](EvalWeight weight) { return weights[static_cast<std::size_t>(weight)]; }
    bool operator==(const EvaluationParameters& other) const {
        return material_mg == other.material_mg && material_eg == other.material_eg &&
               passed_mg == other.passed_mg && passed_eg == other.passed_eg && weights == other.weights;
    }
    bool operator!=(const EvaluationParameters& other) const { return !(*this == other); }
};

bool ValidateEvaluation(const EvaluationParameters& parameters, std::string* error = nullptr);
inline constexpr int kMaxStaticScore = 90000;

// Material-only baseline retained for A/B diagnostics and tests. The search
// uses the tapered evaluator below.
int EvaluateMaterial(const Position& position, const EvaluationParameters& parameters = {});

// Standard material values used by move ordering and the material baseline.
// Kings are zero because they are never captured in legal play.
int PieceValue(PieceType type, const EvaluationParameters& parameters = {});

struct EvaluationBreakdown {
    // Totals from the side-to-move perspective. They include material, PSTs,
    // and all positional terms; tempo is added separately to `score`.
    int middlegame = 0;
    int endgame = 0;
    int phase = 0;  // 0..24; 24 is the opening/middlegame phase.
    int score = 0;  // Tapered centipawn score plus tempo.

    // Positional contributions, also from the side-to-move perspective.
    int pawn_structure_mg = 0;
    int pawn_structure_eg = 0;
    int mobility_mg = 0;
    int mobility_eg = 0;
    int bishop_pair_mg = 0;
    int bishop_pair_eg = 0;
    int rook_files_mg = 0;
    int rook_files_eg = 0;
    int king_safety_mg = 0;
    int king_safety_eg = 0;

    // Constant side-to-move bonus included in `score`.
    int tempo = 0;
};

constexpr int kTempoBonus = 10;

// Material plus separate middlegame/endgame piece-square tables, tapered by the
// remaining non-pawn material. Material is P100/N320/B330/R500/Q900 in the
// middlegame and P120/N300/B320/R540/Q900 in the endgame; kings count zero.
// Positional terms cover pawn structure, mobility, bishop pair, rook files,
// king safety, and tempo. Scores are centipawns relative to the side to move.
EvaluationBreakdown EvaluateDetailed(const Position& position, const EvaluationParameters& parameters = {});
int Evaluate(const Position& position, const EvaluationParameters& parameters = {});

// Fast paths for the search hot loop. They assume the parameters have already
// been validated by ValidateEvaluation once per search (Searcher::Search does
// this before the first node); calling them with invalid parameters is a
// precondition violation and does not use the zeroed public fallback, so the
// per-call validation stay entirely out of the inner path. The computation
// itself is otherwise identical to EvaluateDetailed/Evaluate.
int EvaluateMaterialFast(const Position& position, const EvaluationParameters& parameters);
EvaluationBreakdown EvaluateDetailedFast(const Position& position, const EvaluationParameters& parameters);
int EvaluateFast(const Position& position, const EvaluationParameters& parameters);

}  // namespace chess

#endif  // CHESS_EVALUATION_HPP_
