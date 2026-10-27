#include "chess/search.hpp"

#include <algorithm>
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

int TerminalScore(GameResult result, int ply) {
    if (result == GameResult::Checkmate) {
        return -kMateScore + ply;
    }
    return 0;
}

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

// Stable sorting keeps the legal-move generator's order for equal scores, so
// fixed-depth searches remain deterministic.
void OrderMoves(const Position& position, std::vector<Move>* moves, int ply,
                const SearchContext& context, const Move& hash_move) {
    if (!context.use_move_ordering || moves->size() < 2) {
        return;
    }
    std::stable_sort(
        moves->begin(), moves->end(),
        [&position, ply, &context, &hash_move](const Move& lhs,
                                               const Move& rhs) {
            return MoveOrderScore(position, lhs, ply, context, hash_move) >
                   MoveOrderScore(position, rhs, ply, context, hash_move);
        });
}

int Quiescence(Position& position, int alpha, int beta, int ply, int qply,
               SearchContext& context, std::vector<Move>* pv) {
    pv->clear();
    ++context.nodes;
    if (context.ShouldStop()) {
        return 0;
    }

    const GameResult result = position.Result();
    if (result != GameResult::Ongoing) {
        return TerminalScore(result, ply);
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

    std::vector<Move> moves = position.LegalMoves();
    if (moves.empty()) {
        return in_check ? -kMateScore + ply : 0;
    }

    int best = -kInfinity;
    if (!in_check) {
        moves.erase(std::remove_if(moves.begin(), moves.end(),
                                   [&position, &context](const Move& move) {
                                       if (context.stop.load() || context.HardExpired()) {
                                           context.aborted = true;
                                           return true;
                                       }
                                       if (IsTactical(move)) { return false; }
                                       if (!context.forcing.checks) { return true; }
                                       if (!position.MakeMove(move)) { return true; }
                                       const bool gives_check = position.InCheck(position.SideToMove());
                                       position.UndoMove();
                                       return !gives_check;
                                   }),
                     moves.end());
        if (context.aborted) { return 0; }
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

    std::vector<Move> best_line;
    for (const Move& move : moves) {
        if (!position.MakeMove(move)) {
            continue;
        }
        std::vector<Move> child_line;
        const int score =
            -Quiescence(position, -beta, -alpha, ply + 1, qply + 1, context,
                        &child_line);
        position.UndoMove();

        if (context.aborted) {
            return 0;
        }
        if (score > best) {
            best = score;
            best_line.clear();
            best_line.push_back(move);
            best_line.insert(best_line.end(), child_line.begin(),
                             child_line.end());
        }
        if (best > alpha) {
            alpha = best;
        }
        if (alpha >= beta) {
            break;
        }
    }

    *pv = std::move(best_line);
    return best;
}

// Fail-soft negamax. `pv` belongs to the caller and is cleared when the node has
// no continuation. A stopped search returns zero and is discarded by the root.
int Negamax(Position& position, int depth, int alpha, int beta, int ply,
            SearchContext& context, std::vector<Move>* pv) {
    pv->clear();
    ++context.nodes;
    if (context.ShouldStop()) {
        return 0;
    }

    const GameResult result = position.Result();
    if (result != GameResult::Ongoing) {
        return TerminalScore(result, ply);
    }

    if (depth <= 0) {
        if (context.use_quiescence) {
            return Quiescence(position, alpha, beta, ply, 0, context, pv);
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

    std::vector<Move> moves = position.LegalMoves();
    if (moves.empty()) {
        // Result() should have classified this position, but keep the search
        // safe if a future position invariant is temporarily violated.
        return position.InCheck(position.SideToMove()) ? -kMateScore + ply : 0;
    }
    OrderMoves(position, &moves, ply, context, hash_move);

    const int alpha_original = alpha;
    int best = -kInfinity;
    std::vector<Move> best_line;

    for (const Move& move : moves) {
        const Color mover = position.SideToMove();
        const bool tactical = IsTactical(move);
        if (!position.MakeMove(move)) {
            continue;
        }

        std::vector<Move> child_line;
        const int score = -Negamax(position, depth - 1, -beta, -alpha,
                                   ply + 1, context, &child_line);
        position.UndoMove();

        if (context.aborted) {
            return 0;
        }
        if (score > best) {
            best = score;
            best_line.clear();
            best_line.push_back(move);
            best_line.insert(best_line.end(), child_line.begin(),
                             child_line.end());
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

    if (context.use_transposition_table && context.table != nullptr &&
        !best_line.empty() && !context.aborted) {
        TTBound bound = TTBound::Exact;
        if (best <= alpha_original) {
            bound = TTBound::Upper;
        } else if (best >= beta) {
            bound = TTBound::Lower;
        }
        context.table->Store(SearchTTKey(position), depth, ScoreToTT(best, ply),
                             bound, best_line.front());
    }

    *pv = std::move(best_line);
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
    const std::vector<Move> root_moves = root.LegalMoves();
    if (root_result == GameResult::Checkmate || root_moves.empty()) {
        info.scoreCp = root_result == GameResult::Checkmate ||
                               root.InCheck(root.SideToMove())
                           ? -kMateScore
                           : 0;
        return info;
    }

    // A legal fallback is available even if the caller supplied a zero node
    // budget or the stop flag was already set.
    info.bestMove = root_moves.front();
    info.pv.push_back(root_moves.front());
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
                          limits.useQuiescence,
                          limits.useTranspositionTable, &table_);
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

    for (int depth = 1; depth <= requested_depth; ++depth) {
        if (stop.load(std::memory_order_relaxed) || context.HardExpired()) {
            break;
        }
        if (depth > 1 && context.SoftExpired()) {
            break;
        }

        std::vector<Move> pv;
        const int score =
            Negamax(root, depth, -kInfinity, kInfinity, 0, context, &pv);
        info.nodes = context.nodes;
        if (context.aborted) {
            break;
        }
        if (pv.empty()) {
            break;
        }

        info.depth = depth;
        info.scoreCp = score;
        info.pv = pv;
        info.bestMove = pv.front();

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
