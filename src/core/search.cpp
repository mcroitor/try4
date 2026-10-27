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

bool IsCapture(const Move& move) {
    return move.HasFlag(MoveFlag::Capture) ||
           move.HasFlag(MoveFlag::EnPassant);
}

bool IsTactical(const Move& move) {
    return IsCapture(move) || move.HasFlag(MoveFlag::Promotion);
}

using Clock = std::chrono::steady_clock;

struct SearchContext {
    std::uint64_t nodes = 0;
    std::optional<std::uint64_t> node_limit;
    std::atomic_bool& stop;
    bool use_move_ordering = true;
    bool use_quiescence = true;
    bool use_transposition_table = true;
    EvaluationMode evaluation_mode = EvaluationMode::Full;
    EvaluationParameters evaluation;
    ForcingSearchSettings forcing;
    TranspositionTable* table = nullptr;
    bool aborted = false;

    Move killers[kMaxPly][2] = {};
    int history[2][64][64] = {};

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
                  bool quiescence, bool use_tt, TranspositionTable* tt)
        : node_limit(limit),
          stop(stop_flag),
          use_move_ordering(ordering),
          use_quiescence(quiescence),
          use_transposition_table(use_tt),
          table(tt) {}

    int StaticEvaluation(const Position& position) const {
        return evaluation_mode == EvaluationMode::Material
                   ? EvaluateMaterial(position, evaluation)
                   : Evaluate(position, evaluation);
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

struct SortedMove {
    Move move;
    int score = 0;
    std::size_t index = 0;
};

// Stable sorting keeps the legal-move generator's order for equal scores, so
// fixed-depth searches remain deterministic. Sorting is done in place over a
// stack buffer: no heap allocation is allowed inside the search hot path.
void OrderMoves(const Position& position, MoveList* moves, int ply,
                const SearchContext& context, const Move& hash_move) {
    if (!context.use_move_ordering || moves->Size() < 2) {
        return;
    }

    const std::size_t count = moves->Size();
    assert(count <= MoveList::kCapacity);
    SortedMove sorted[MoveList::kCapacity];
    for (std::size_t index = 0; index < count; ++index) {
        sorted[index].move = (*moves)[index];
        sorted[index].score =
            MoveOrderScore(position, sorted[index].move, ply, context, hash_move);
        sorted[index].index = index;
    }

    std::sort(
        sorted, sorted + count, [](const SortedMove& lhs, const SortedMove& rhs) {
            if (lhs.score != rhs.score) {
                return lhs.score > rhs.score;
            }
            return lhs.index < rhs.index;
        });

    moves->Clear();
    for (std::size_t index = 0; index < count; ++index) {
        moves->Add(sorted[index].move);
    }
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
                moves[kept++] = move;
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
                moves[kept++] = move;
            }
        }
        moves.Resize(kept);

        if (context.aborted) {
            return 0;
        }

        const int stand_pat = context.StaticEvaluation(position);
        if (stand_pat >= beta) {
            return stand_pat;
        }
        if (stand_pat > alpha) {
            alpha = stand_pat;
        }
        best = stand_pat;
    }

    OrderMoves(position, &moves, ply, context, Move{});

    for (std::size_t index = 0; index < moves.Size(); ++index) {
        const Move move = moves[index];
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

    if (depth <= 0) {
        if (context.use_quiescence) {
            return Quiescence(position, alpha, beta, ply, 0, context);
        }
        return context.StaticEvaluation(position);
    }

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

    OrderMoves(position, &moves, ply, context, hash_move);

    const int alpha_original = alpha;
    int best = -kInfinity;
#ifndef NDEBUG
    const std::size_t history_depth = position.HistoryDepth();
#endif

    for (std::size_t index = 0; index < moves.Size(); ++index) {
        const Move move = moves[index];
        const Color mover = position.SideToMove();
        const bool tactical = IsTactical(move);
        position.MakeMoveAssumeLegal(move);

        context.moves[ply + 1].Clear();
        const int score = -Negamax(position, depth - 1, -beta, -alpha,
                                   ply + 1, context);
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
        previous_policy_->useQuiescence != limits.useQuiescence) {
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

    const TimeBudget budget = ComputeTimeBudget(limits, position.SideToMove());
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

    root.ReserveHistory(root.HistoryDepth() + static_cast<std::size_t>(kMaxPly));

    for (int depth = 1; depth <= requested_depth; ++depth) {
        if (stop.load(std::memory_order_relaxed) || context.HardExpired()) {
            break;
        }
        if (depth > 1 && context.SoftExpired()) {
            break;
        }

        context.moves[0].Clear();
        const int score = Negamax(root, depth, -kInfinity, kInfinity, 0,
                                  context);
        info.nodes = context.nodes;
        if (context.aborted) {
            break;
        }
        const std::size_t pv_length = context.pv_length[0];
        if (pv_length == 0) {
            break;
        }

        info.depth = depth;
        info.scoreCp = score;
        info.pv.assign(context.pv[0], context.pv[0] + pv_length);
        info.bestMove = info.pv.front();

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
