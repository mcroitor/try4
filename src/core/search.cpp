#include "chess/search.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <utility>
#include <vector>

#include "chess/evaluation.hpp"
#include "chess/time_manager.hpp"
#include "chess/transposition_table.hpp"

namespace chess {
namespace {

constexpr int kInfinity = 1000000;
constexpr int kDefaultDepth = 4;
constexpr std::uint64_t kStopPollInterval = 32;
constexpr int kMaxPly = 128;

constexpr int kCaptureOrderBase = 1000000;
constexpr int kPromotionOrderBase = 900000;
constexpr int kKillerOneBonus = 800000;
constexpr int kKillerTwoBonus = 790000;
constexpr int kHashMoveBonus = 2000000;

// Aspiration windows start at depth kAspirationMinDepth around the previous
// iteration's score, widen the failed side by kAspirationWidenFactor on each
// failed attempt, and fall back to the full window after
// kMaxAspirationFailures failures or when a score reaches the mate band. A
// completed narrow search reports the same exact score as a full-window
// search; failed-window results are never reported.
constexpr int kAspirationDelta = 25;
constexpr int kAspirationMinDepth = 3;
constexpr int kAspirationWidenFactor = 4;
constexpr int kMaxAspirationFailures = 2;

// Null-move probing starts at this depth; shallower nodes search exactly.
constexpr int kNullMoveMinDepth = 3;

bool IsCapture(const Move& move) {
    return move.HasFlag(MoveFlag::Capture) ||
           move.HasFlag(MoveFlag::EnPassant);
}

bool IsTactical(const Move& move) {
    return IsCapture(move) || move.HasFlag(MoveFlag::Promotion);
}

using Clock = std::chrono::steady_clock;

struct StagedMoveOrder {
    int scores[MoveList::kCapacity] = {};
    std::size_t indices[MoveList::kCapacity] = {};
    std::size_t next = 0;
};

struct SearchContext {
    std::uint64_t nodes = 0;
    std::optional<std::uint64_t> node_limit;
    std::atomic_bool& stop;
    bool use_move_ordering = true;
    bool use_quiescence = true;
    bool use_transposition_table = true;
    bool use_principal_variation = true;
    bool use_aspiration_windows = true;
    PruningSettings pruning;
    EvaluationMode evaluation_mode = EvaluationMode::Full;
    EvaluationParameters evaluation;
    ForcingSearchSettings forcing;
    TranspositionTable* table = nullptr;
    bool aborted = false;

    Move killers[kMaxPly][2] = {};
    int history[2][64][64] = {};

    // Lazy move picker state, staged per ply. Scores are computed once and the
    // picker swaps in the best remaining candidate on demand.
    StagedMoveOrder order[kMaxPly];

    // Triangular PV table. Row `ply` is only written by the node at that ply
    // and read by its parent; rows 1..kMaxPly-1 are therefore safe to reuse.
    // The line is converted to a vector only at the root of Search().
    Move pv[kMaxPly][kMaxPly] = {};
    std::size_t pv_length[kMaxPly] = {};

    // One preallocated move list per ply; the search reuses them in place and
    // avoids allocating or zero-initializing a fresh MoveList per node.
    MoveList moves[kMaxPly] = {};

    std::optional<Clock::time_point> soft_deadline;
    std::optional<Clock::time_point> hard_deadline;

    SearchContext(std::atomic_bool& stop_flag,
                  std::optional<std::uint64_t> limit, bool ordering,
                  bool quiescence, bool use_tt, bool principal_variation,
                  bool aspiration_windows,
                  const PruningSettings& pruning_settings,
                  TranspositionTable* tt)
        : node_limit(limit),
          stop(stop_flag),
          use_move_ordering(ordering),
          use_quiescence(quiescence),
          use_transposition_table(use_tt),
          use_principal_variation(principal_variation),
          use_aspiration_windows(aspiration_windows),
          pruning(pruning_settings),
          table(tt) {}

    // Pre-validated fast paths. Search() validates limits.evaluation once
    // before the context is built, so the hot loop must not revalidate.
    int StaticEvaluation(const Position& position) const {
        return evaluation_mode == EvaluationMode::Material
                   ? EvaluateMaterialFast(position, evaluation)
                   : EvaluateFast(position, evaluation);
    }

    bool ShouldStop() {
        if (node_limit.has_value() && nodes >= *node_limit) {
            aborted = true;
            return true;
        }
        if (nodes == 1 || nodes % kStopPollInterval == 0) {
            if (stop.load(std::memory_order_relaxed)) {
                aborted = true;
                return true;
            }
            if (hard_deadline.has_value() &&
                Clock::now() >= *hard_deadline) {
                aborted = true;
                return true;
            }
        }
        return false;
    }

    bool SoftExpired() const {
        return soft_deadline.has_value() && Clock::now() >= *soft_deadline;
    }

    bool HardExpired() const {
        return hard_deadline.has_value() && Clock::now() >= *hard_deadline;
    }

    void RecordCutoff(Color mover, const Move& move, int ply, int depth) {
        if (IsTactical(move) || ply < 0 || ply >= kMaxPly) {
            return;
        }
        if (!(killers[ply][0] == move)) {
            killers[ply][1] = killers[ply][0];
            killers[ply][0] = move;
        }

        const int color = static_cast<int>(mover);
        const int from = static_cast<int>(move.from.Index());
        const int to = static_cast<int>(move.to.Index());
        const int bonus = std::min(40000, depth * depth);
        history[color][from][to] =
            std::min(50000, history[color][from][to] + bonus);
    }
};

// The Zobrist hash deliberately excludes the halfmove clock, which is correct
// for repetition but not for fifty-move semantics. Mix it into the TT key so a
// score cached near the fifty-move boundary cannot leak into a fresh position.
std::uint64_t SearchTTKey(const Position& position) {
    return position.Hash() ^
           (static_cast<std::uint64_t>(position.HalfmoveClock()) *
            0x9E3779B97F4A7C15ULL);
}

PieceType CaptureVictim(const Position& position, const Move& move) {
    if (move.HasFlag(MoveFlag::EnPassant)) {
        return PieceType::Pawn;
    }
    return position.PieceAt(move.to).Type();
}

int MoveOrderScore(const Position& position, const Move& move, int ply,
                   const SearchContext& context, const Move& hash_move) {
    int score = 0;
    const bool capture = IsCapture(move);
    const bool promotion = move.HasFlag(MoveFlag::Promotion);

    if (hash_move.IsValid() && move == hash_move) {
        score += kHashMoveBonus;
    }
    if (capture) {
        const PieceType victim = CaptureVictim(position, move);
        const PieceType attacker = position.PieceAt(move.from).Type();
        score += kCaptureOrderBase + 100 * PieceValue(victim, context.evaluation) -
                 PieceValue(attacker, context.evaluation);
    }
    if (promotion) {
        score += kPromotionOrderBase + PieceValue(move.promotion, context.evaluation);
    }
    if (!capture && !promotion) {
        if (ply >= 0 && ply < kMaxPly) {
            if (move == context.killers[ply][0]) {
                score += kKillerOneBonus;
            } else if (move == context.killers[ply][1]) {
                score += kKillerTwoBonus;
            }
        }
        const int color = static_cast<int>(position.SideToMove());
        const int from = static_cast<int>(move.from.Index());
        const int to = static_cast<int>(move.to.Index());
        score += std::min(context.history[color][from][to], 700000);
    }
    return score;
}

// True when `color` has no knight, bishop, rook or queen on the board. A pass
// can never hurt a side that has no non-pawn moves to spare, so the search
// disables null-move pruning in such endings to keep zugzwang sound.
bool SideHasOnlyKingAndPawns(const Position& position, Color color) {
    for (std::uint8_t index = 0; index < 64; ++index) {
        const Piece piece = position.PieceAt(Square(index));
        if (piece.IsEmpty() || piece.Color() != color) {
            continue;
        }
        const PieceType type = piece.Type();
        if (type != PieceType::King && type != PieceType::Pawn) {
            return false;
        }
    }
    return true;
}

// The picker is score-once: the ordering key is evaluated one time per move.
// The remaining candidates are then selected lazily with a stable maximum
// selection, which preserves the current ordering policy and avoids paying for
// candidates that are never searched because of an early cutoff.
//
// Stages encoded by `MoveOrderScore` are:
// hash move, MVV-LVA captures, promotions, killers, and history-ordered quiet
// moves. Equal scores stay deterministic by the generator's original index.
void StageMoveOrder(const Position& position, MoveList* moves, int ply,
                    SearchContext& context, const Move& hash_move) {
    const std::size_t count = moves->Size();
    assert(count <= MoveList::kCapacity);
    assert(ply >= 0 && ply < kMaxPly);
    StagedMoveOrder& order = context.order[ply];
    order.next = 0;
    if (!context.use_move_ordering) {
        return;
    }
    for (std::size_t index = 0; index < count; ++index) {
        const Move move = (*moves)[index];
        order.scores[index] = MoveOrderScore(position, move, ply, context, hash_move);
        order.indices[index] = index;
    }
}

Move SelectNextMove(MoveList* moves, int ply, SearchContext& context) {
    if (!context.use_move_ordering) {
        return (*moves)[context.order[ply].next++];
    }

    StagedMoveOrder& order = context.order[ply];
    const std::size_t count = moves->Size();
    assert(order.next < count);
    std::size_t best = order.next;
    for (std::size_t index = best + 1; index < count; ++index) {
        const int score = order.scores[index];
        const std::size_t original = order.indices[index];
        if (score > order.scores[best] ||
            (score == order.scores[best] && original < order.indices[best])) {
            best = index;
        }
    }

    const Move move = (*moves)[best];
    if (best != order.next) {
        std::swap((*moves)[order.next], (*moves)[best]);
        std::swap(order.scores[order.next], order.scores[best]);
        std::swap(order.indices[order.next], order.indices[best]);
    }
    ++order.next;
    return move;
}

int Quiescence(Position& position, int alpha, int beta, int ply, int qply,
               SearchContext& context) {
    context.pv_length[ply] = 0;
    ++context.nodes;
    if (context.ShouldStop()) {
        return 0;
    }

    MoveList& moves = context.moves[ply];
    if (moves.Empty()) {
        position.GenerateLegalMoves(&moves);
    }

    // Cheap terminal/draw checks replace the expensive per-node Result() call.
    // The original `Result()` precedence is preserved here as far as the
    // interior search is concerned; insufficient material remains a root-only
    // draw contract (see Search()).
    if (moves.Empty()) {
        return position.InCheck(position.SideToMove())
                   ? -kMateScore + ply
                   : 0;
    }
    if (position.HalfmoveClock() >= 100) {
        return 0;
    }
    if (position.IsRepetition()) {
        return 0;
    }

    if (ply >= kMaxPly - 1) {
        context.aborted = true;
        return 0;
    }

    const bool in_check = position.InCheck(position.SideToMove());
    if (qply >= context.forcing.max_plies && !in_check) {
        return context.StaticEvaluation(position);
    }
    if (qply >= context.forcing.hard_plies) {
        context.aborted = true;
        return 0;
    }

    int best = -kInfinity;
    int stand_pat = 0;
    bool delta_active = false;
    if (!in_check) {
        std::size_t kept = 0;
        const std::size_t count = moves.Size();
        for (std::size_t index = 0; index < count; ++index) {
            const Move move = moves[index];
            if (context.stop.load() || context.HardExpired()) {
                context.aborted = true;
                return 0;
            }

            if (IsTactical(move)) {
                if (kept != index) {
                    moves[kept] = move;
                }
                ++kept;
                continue;
            }
            if (!context.forcing.checks) {
                continue;
            }

            position.MakeMoveAssumeLegal(move);
            const bool gives_check =
                position.InCheck(position.SideToMove());
            position.UndoMove();
            if (gives_check) {
                if (kept != index) {
                    moves[kept] = move;
                }
                ++kept;
            }
        }
        moves.Resize(kept);

        if (context.aborted) {
            return 0;
        }

        stand_pat = context.StaticEvaluation(position);
        if (stand_pat >= beta) {
            return stand_pat;
        }
        if (stand_pat > alpha) {
            alpha = stand_pat;
        }
        best = stand_pat;

        // Delta pruning: a capture whose best case cannot even reach alpha,
        // with a safety margin, is never searched. Never applied in check
        // (no stand pat) and never when alpha is near a mate score, where
        // pruning could hide a mate; promotions are never pruned because
        // their gain is not bounded by the victim's value.
        delta_active = context.pruning.useDeltaPruning &&
                       std::abs(alpha) < kMateThreshold;
    }

    StageMoveOrder(position, &moves, ply, context, Move{});

    for (std::size_t index = 0; index < moves.Size(); ++index) {
        const Move move = SelectNextMove(&moves, ply, context);
        // Delta pruning applies to captures only: a quiet check kept by the
        // checks filter can mate, and its gain is never bounded by a victim's
        // value. Promotions are exempt for the same reason.
        if (delta_active && !move.HasFlag(MoveFlag::Promotion) &&
            IsCapture(move)) {
            const PieceType victim = CaptureVictim(position, move);
            const int best_case =
                stand_pat + PieceValue(victim, context.evaluation) +
                context.pruning.deltaPruningMargin;
            if (best_case <= alpha) {
                continue;
            }
        }
        position.MakeMoveAssumeLegal(move);
        context.moves[ply + 1].Clear();
        const int score = -Quiescence(position, -beta, -alpha, ply + 1,
                                      qply + 1, context);
        position.UndoMove();

        if (context.aborted) {
            return 0;
        }
        if (score > best) {
            best = score;
            context.pv[ply][0] = move;
            const std::size_t child_length = context.pv_length[ply + 1];
            for (std::size_t offset = 0; offset < child_length; ++offset) {
                context.pv[ply][offset + 1] = context.pv[ply + 1][offset];
            }
            context.pv_length[ply] = child_length + 1;
        }
        if (best > alpha) {
            alpha = best;
        }
        if (alpha >= beta) {
            break;
        }
    }

    return best;
}

int Negamax(Position& position, int depth, int alpha, int beta, int ply,
            int extensions, SearchContext& context) {
    context.pv_length[ply] = 0;
    ++context.nodes;
    if (context.ShouldStop()) {
        return 0;
    }

    MoveList& moves = context.moves[ply];
    if (moves.Empty()) {
        position.GenerateLegalMoves(&moves);
    }

    // Cheap terminal/draw checks replace the expensive per-node Result() call.
    // The original `Result()` precedence is preserved here as far as the
    // interior search is concerned; insufficient material remains a root-only
    // draw contract (see Search()).
    if (moves.Empty()) {
        return position.InCheck(position.SideToMove())
                   ? -kMateScore + ply
                   : 0;
    }
    if (position.HalfmoveClock() >= 100) {
        return 0;
    }
    if (position.IsRepetition()) {
        return 0;
    }

    if (depth <= 0) {
        if (context.use_quiescence) {
            return Quiescence(position, alpha, beta, ply, 0, context);
        }
        return context.StaticEvaluation(position);
    }

    const bool in_check = position.InCheck(position.SideToMove());

    Move hash_move;
    if (context.use_transposition_table && context.table != nullptr) {
        TTEntry entry;
        if (context.table->Probe(SearchTTKey(position), &entry)) {
            hash_move = entry.bestMove;
            if (entry.depth >= depth && ply > 0) {
                const int tt_score = ScoreFromTT(entry.score, ply);
                const bool exact = entry.bound == TTBound::Exact;
                const bool lower_ok =
                    entry.bound == TTBound::Lower && tt_score >= beta;
                const bool upper_ok =
                    entry.bound == TTBound::Upper && tt_score <= alpha;
                if (exact || lower_ok || upper_ok) {
                    return tt_score;
                }
            }
        }
    }

    // Guarded null-move probe: the mover passes and a zero-window search of
    // the position with the opponent moving twice decides whether beta is
    // already reachable. The guards keep the technique safe and affordable by
    // construction: never at the root (ply > 0), never in check, never below
    // depth kNullMoveMinDepth, never in endings where the mover has only
    // king and pawns (zugzwang), never against an opponent with no non-pawn
    // material (a lone king survives a passed tempo, so the probe can only
    // waste nodes), never on windows that reach the mate band, and never
    // when beta is so large that a cutoff is unprovable and the probe would
    // only spend nodes.
    if (context.pruning.useNullMove && !in_check && ply > 0 &&
        depth >= kNullMoveMinDepth &&
        !SideHasOnlyKingAndPawns(position, position.SideToMove()) &&
        !SideHasOnlyKingAndPawns(position, Opposite(position.SideToMove())) &&
        alpha < kMateThreshold && beta > -kMateThreshold && beta < kInfinity) {
        // The probe's child runs at ply + 1 like any real child, so its stale
        // move list must be cleared exactly like the move loop does; reusing
        // the previous sibling's list would search moves of another position.
        position.MakeNullMove();
        context.moves[ply + 1].Clear();
        const int null_depth =
            std::max(0, depth - 1 - context.pruning.nullMoveReduction);
        const int null_verdict =
            -Negamax(position, null_depth, -beta, -beta + 1, ply + 1,
                     extensions, context);
        position.UndoMove();
        if (context.aborted) {
            return 0;
        }
        if (null_verdict >= beta) {
            // A mate score carried by a null verdict would claim an unproven
            // mate line; return the still-valid beta bound instead.
            return null_verdict >= kMateThreshold ? beta : null_verdict;
        }
    }

    // Check extension: a check evasion is searched one ply deeper. The
    // extension is bounded by the per-line budget passed down through the
    // recursion and by the existing kMaxPly guard.
    if (context.pruning.useCheckExtension && in_check && extensions > 0) {
        --extensions;
        ++depth;
    }
    const int search_depth = depth;

    StageMoveOrder(position, &moves, ply, context, hash_move);

    const int alpha_original = alpha;
    int best = -kInfinity;
#ifndef NDEBUG
    const std::size_t history_depth = position.HistoryDepth();
#endif

    for (std::size_t index = 0; index < moves.Size(); ++index) {
        const Move move = SelectNextMove(&moves, ply, context);
        const Color mover = position.SideToMove();
        const bool tactical = IsTactical(move);
        position.MakeMoveAssumeLegal(move);

        // Late-move reduction for quiet, non-first moves past the depth and
        // move-number thresholds. The root is never reduced: its key moves
        // are often quiet, late-ordered mates, and a reduced probe cannot
        // see them. The hash move, captures, promotions, moves while in
        // check and moves that give check are never reduced; the gives-check
        // probe runs on the position after the move. The window guard keeps
        // reductions out of mate-band windows.
        int reduction = 0;
        if (context.pruning.useLateMoveReduction && ply > 0 &&
            static_cast<int>(index) >=
                context.pruning.lateMoveReductionMinMoveNumber &&
            search_depth >= context.pruning.lateMoveReductionMinDepth &&
            search_depth - 1 >= 1 && !tactical && !in_check &&
            std::abs(alpha) < kMateThreshold &&
            std::abs(beta) < kMateThreshold &&
            !(move == hash_move) &&
            !position.InCheck(position.SideToMove())) {
            reduction = 1;
        }

        context.moves[ply + 1].Clear();
        int score = 0;
        bool counted = false;  // Reduced-probe verdicts never feed bookkeeping.
        if (context.use_principal_variation && index > 0) {
            if (reduction > 0) {
                score = -Negamax(position, search_depth - 1 - reduction,
                                 -alpha - 1, -alpha, ply + 1, extensions,
                                 context);
                if (!context.aborted && score > alpha) {
                    // Fail high under the reduced probe: re-search at full
                    // depth, still with the zero window.
                    score = -Negamax(position, search_depth - 1, -alpha - 1,
                                     -alpha, ply + 1, extensions, context);
                    if (context.aborted) {
                        return 0;
                    }
                    // A full-depth zero-window verdict is a sound bound, so
                    // it participates exactly like the un-reduced probe.
                    counted = true;
                    if (score > alpha) {
                        // Still a fail high: the exact full-window re-search
                        // that may raise alpha or write the PV.
                        score = -Negamax(position, search_depth - 1, -beta,
                                         -alpha, ply + 1, extensions, context);
                    }
                }
                // Reduced probe verdict at most alpha: probe-only.
            } else {
                score = -Negamax(position, search_depth - 1, -alpha - 1,
                                 -alpha, ply + 1, extensions, context);
                if (context.aborted) {
                    return 0;
                }
                counted = true;
                if (score > alpha) {
                    // Zero-window fail high: an exact full-window re-search is
                    // required before the move may raise alpha or write
                    // the PV.
                    score = -Negamax(position, search_depth - 1, -beta, -alpha,
                                     ply + 1, extensions, context);
                }
            }
        } else {
            if (reduction > 0) {
                score = -Negamax(position, search_depth - 1 - reduction, -beta,
                                 -alpha, ply + 1, extensions, context);
                if (context.aborted) {
                    return 0;
                }
                if (score >= beta) {
                    // A fail high proves the refutation exists; the lower
                    // bound is sound regardless of the reduction.
                    counted = true;
                } else if (score > alpha) {
                    // Improved alpha without a cutoff: re-search at full
                    // depth for an exact verdict.
                    score = -Negamax(position, search_depth - 1, -beta, -alpha,
                                     ply + 1, extensions, context);
                    counted = true;
                }
            } else {
                score = -Negamax(position, search_depth - 1, -beta, -alpha,
                                 ply + 1, extensions, context);
                counted = true;
            }
        }
        position.UndoMove();

        if (context.aborted) {
            return 0;
        }
        if (!counted) {
            // The reduced probe proved this move does not improve alpha. The
            // verdict is a bound of the reduced tree, so it never feeds the
            // fail-soft bookkeeping, the PV or the table store.
            continue;
        }
        if (score > best) {
            best = score;
            context.pv[ply][0] = move;
            const std::size_t child_length = context.pv_length[ply + 1];
            for (std::size_t offset = 0; offset < child_length; ++offset) {
                context.pv[ply][offset + 1] = context.pv[ply + 1][offset];
            }
            context.pv_length[ply] = child_length + 1;
        }
        if (best > alpha) {
            alpha = best;
        }
        if (alpha >= beta) {
            if (!tactical) {
                context.RecordCutoff(mover, move, ply, depth);
            }
            break;
        }
    }

#ifndef NDEBUG
    assert(position.HistoryDepth() == history_depth);
#endif

    if (context.use_transposition_table && context.table != nullptr &&
        context.pv_length[ply] > 0 && !context.aborted) {
        TTBound bound = TTBound::Exact;
        if (best <= alpha_original) {
            bound = TTBound::Upper;
        } else if (best >= beta) {
            bound = TTBound::Lower;
        }
        context.table->Store(SearchTTKey(position), depth,
                             ScoreToTT(best, ply), bound,
                             context.pv[ply][0]);
    }

    return best;
}

}  // namespace

Searcher::Searcher(std::size_t transpositionTableMegabytes)
    : table_(transpositionTableMegabytes) {}

void Searcher::ClearTranspositionTable() {
    table_.Clear();
}

void Searcher::ResizeTranspositionTable(std::size_t megabytes) {
    table_.Resize(megabytes);
}

std::size_t Searcher::TranspositionTableMegabytes() const {
    return table_.Megabytes();
}

SearchInfo Searcher::Search(const Position& position, const SearchLimits& requested_limits,
                            std::atomic_bool& stop,
                            const SearchProgressCallback& on_progress) {
    const SearchLimits limits = requested_limits;
    if (!previous_policy_ || previous_policy_->evaluation != limits.evaluation ||
        previous_policy_->forcing != limits.forcing ||
        previous_policy_->evaluationMode != limits.evaluationMode ||
        previous_policy_->useQuiescence != limits.useQuiescence ||
        previous_policy_->usePrincipalVariation !=
            limits.usePrincipalVariation ||
        previous_policy_->useAspirationWindows !=
            limits.useAspirationWindows ||
        previous_policy_->pruning != limits.pruning) {
        table_.Clear();
        previous_policy_ = limits;
    }
    const Clock::time_point start = Clock::now();
    SearchInfo info;
    Position root = position;

    const GameResult root_result = root.Result();
    MoveList root_moves;
    root.GenerateLegalMoves(&root_moves);
    if (root_result == GameResult::Checkmate || root_moves.Empty()) {
        info.scoreCp = root_result == GameResult::Checkmate ||
                               root.InCheck(root.SideToMove())
                           ? -kMateScore
                           : 0;
        return info;
    }

    // A legal fallback is available even if the caller supplied a zero node
    // budget or the stop flag was already set.
    info.bestMove = root_moves[0];
    info.pv.push_back(root_moves[0]);
    info.scoreCp = root_result == GameResult::Ongoing
                       ? (limits.evaluationMode == EvaluationMode::Material
                              ? EvaluateMaterial(root, limits.evaluation)
                              : Evaluate(root, limits.evaluation))
                       : 0;

    // Drawn roots with legal moves still expose a legal move for callers that
    // need to complete a protocol transaction; their score is exactly zero.
    if (root_result != GameResult::Ongoing) {
        return info;
    }
    if (!ValidateEvaluation(limits.evaluation) || limits.forcing.max_plies < 0 ||
        limits.forcing.hard_plies < 1 || limits.forcing.hard_plies > 120 ||
        limits.forcing.max_plies > limits.forcing.hard_plies) {
        return info;
    }

    const TimeBudget budget =
        ComputeTimeBudget(limits, position.SideToMove());
    // Task 057: the per-iteration predictor. `predicted_ms` is the previous
    // completed iteration's wall time scaled by a measured effective
    // branching factor (clamped 1.5..4.0, fallback 2.0 when no node history
    // exists); fixed-node/depth searches have no budget and never predict.
    std::int64_t previous_iteration_ms = 0;
    std::uint64_t previous_iteration_nodes = 0;
    std::uint64_t completed_iteration_nodes = 0;
    bool previous_iteration_was_stable = true;
    Move previous_best;
    bool have_previous_best = false;

    const auto predict_iteration_ms = [&]() -> std::int64_t {
        if (!budget.HasLimit() || previous_iteration_ms <= 0 ||
            previous_iteration_nodes == 0) {
            return 0;
        }
        double branching = 2.0;
        if (completed_iteration_nodes > previous_iteration_nodes) {
            branching = static_cast<double>(completed_iteration_nodes) /
                        static_cast<double>(previous_iteration_nodes);
        }
        branching = std::clamp(branching, 1.5, 4.0);
        return std::min<std::int64_t>(
            static_cast<std::int64_t>(
                static_cast<double>(previous_iteration_ms) * branching),
            std::max<std::int64_t>(0, *budget.hardMs * 4));
    };
    const int requested_depth =
        std::min(kMaxPly - 1, limits.depth.value_or(
            limits.infinite ? kMaxPly - 1 : (budget.HasLimit() ? 64 : kDefaultDepth)));
    if (requested_depth <= 0) {
        return info;
    }
    const std::optional<std::uint64_t> node_limit =
        limits.EffectiveNodeLimit();
    if ((node_limit.has_value() && *node_limit == 0) ||
        stop.load(std::memory_order_relaxed)) {
        return info;
    }

    SearchContext context(stop, node_limit, limits.useMoveOrdering,
                          limits.useQuiescence, limits.useTranspositionTable,
                          limits.usePrincipalVariation,
                          limits.useAspirationWindows, limits.pruning,
                          &table_);
    context.evaluation_mode = limits.evaluationMode;
    context.evaluation = limits.evaluation;
    context.forcing = limits.forcing;
    if (budget.softMs.has_value()) {
        context.soft_deadline =
            start + std::chrono::milliseconds(*budget.softMs);
    }
    if (budget.hardMs.has_value()) {
        context.hard_deadline =
            start + std::chrono::milliseconds(*budget.hardMs);
    }

    // The aspiration anchor is the last completed iteration's score; before
    // depth kAspirationMinDepth every iteration uses the full window.
    int previous_score = info.scoreCp;
    Clock::time_point iteration_start = Clock::now();
    for (int depth = 1; depth <= requested_depth; ++depth) {
        if (stop.load(std::memory_order_relaxed) || context.HardExpired()) {
            break;
        }

        const std::int64_t elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                Clock::now() - start)
                .count();
        if (depth > 1) {
            const std::int64_t predicted = predict_iteration_ms();
            const bool soft_expired = context.SoftExpired();
            // Task 057: a soft-limit stop is kept except when the last
            // completed iteration changed the best move and the prediction
            // still lands inside the hard ceiling; in that case the search
            // may extend to hard. The predictor itself never skips an
            // iteration without prior timing (predicted == 0 there).
            const bool extend_to_hard_on_instability =
                !previous_iteration_was_stable && budget.hardMs.has_value() &&
                elapsed_ms + predicted <= *budget.hardMs;
            if ((predicted > 0 && elapsed_ms + predicted > *budget.hardMs) ||
                (soft_expired && !extend_to_hard_on_instability)) {
                break;
            }
        }
        iteration_start = Clock::now();

        const bool narrow =
            context.use_aspiration_windows && depth >= kAspirationMinDepth &&
            std::abs(previous_score) < kMateThreshold;
        int delta = kAspirationDelta;
        int alpha = narrow ? previous_score - delta : -kInfinity;
        int beta = narrow ? previous_score + delta : kInfinity;
        int failures = 0;
        bool complete = false;
        int score = 0;
        while (true) {
            // Reusing the same root move list across iterations and window
            // attempts is safe because each child node uses its own ply and
            // never reorders the root list. Nodes, the node limit and the
            // stop flag are shared and monotonic across attempts; a re-search
            // visits nodes anew but never resets the budget.
            context.moves[0].Clear();
            score = Negamax(root, depth, alpha, beta, 0,
                            context.pruning.checkExtensionCap, context);
            info.nodes = context.nodes;
            if (context.aborted) {
                break;
            }
            if (score > alpha && score < beta) {
                complete = true;
                break;
            }
            // A failed window is never reported. Widen the failed side around
            // the returned bound value, fall back to the full window after the
            // failure budget, and never narrow around mate scores.
            ++failures;
            if (failures >= kMaxAspirationFailures ||
                std::abs(score) >= kMateThreshold) {
                alpha = -kInfinity;
                beta = kInfinity;
            } else if (score <= alpha) {
                alpha = score - delta;
                delta *= kAspirationWidenFactor;
            } else {
                beta = score + delta;
                delta *= kAspirationWidenFactor;
            }
        }
        if (!complete) {
            break;
        }
        previous_score = score;

        const std::size_t pv_length = context.pv_length[0];
        if (pv_length == 0) {
            break;
        }

        // Task 057 predictor bookkeeping: the completed iteration's duration
        // and node share feed the branching-factor prediction; the best-move
        // stability of this iteration decides whether a later soft-expired
        // iteration may extend to hard.
        const Clock::time_point iteration_end = Clock::now();
        const Move best_before =
            info.bestMove.has_value() ? *info.bestMove : Move{};
        previous_iteration_nodes = completed_iteration_nodes;
        completed_iteration_nodes = context.nodes;
        previous_iteration_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                iteration_end - iteration_start)
                .count();

        info.depth = depth;
        info.scoreCp = score;
        info.pv.assign(context.pv[0], context.pv[0] + pv_length);
        info.bestMove = info.pv.front();
        previous_iteration_was_stable =
            have_previous_best && *info.bestMove == best_before;
        have_previous_best = true;

        if (on_progress) {
            on_progress(info);
        }

        // A mate score is exact and cannot be improved by a deeper search.
        if (std::abs(score) >= kMateThreshold) {
            break;
        }
    }

    return info;
}

}  // namespace chess
