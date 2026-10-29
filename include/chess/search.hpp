#ifndef CHESS_SEARCH_HPP_
#define CHESS_SEARCH_HPP_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "chess/position.hpp"
#include "chess/evaluation.hpp"
#include "chess/transposition_table.hpp"

namespace chess {

// Mate scores are centipawn-like integers with a safe distance from ordinary
// evaluation scores. A mate at ply p is encoded as kMateScore - p; a score
// whose magnitude is at least kMateThreshold is a mate score.
constexpr int kMateScore = 100000;
constexpr int kMateThreshold = kMateScore - 1000;

enum class EvaluationMode {
    Full,
    Material,
};

struct ForcingSearchSettings {
    bool checks = false;
    int max_plies = 8;
    int hard_plies = 32;
    bool operator==(const ForcingSearchSettings& other) const {
        return checks == other.checks && max_plies == other.max_plies && hard_plies == other.hard_plies;
    }
    bool operator!=(const ForcingSearchSettings& other) const { return !(*this == other); }
};

// Selective pruning techniques. Each one is individually switchable so a
// regression can be attributed by A/B testing a single technique. Pruning is
// approximate by construction: it may change fixed-depth scores (never the
// reported mate exactness of the tactical corpus), which is why the defaults
// were selected from the measured A/B evidence recorded in task 056.
struct PruningSettings {
    // Null move: the mover passes once and the child proves the mover can
    // still reach beta even with the opponent's extra move. Guarded by the
    // search itself: never at the root, never in check, never below depth 3, never in all-pawn
    // endings (zugzwang) and never on windows that reach the mate band.
    bool useNullMove = true;
    int nullMoveReduction = 2;  // Probe depth = depth - 1 - this.

    // Late-move reductions: quiet, non-first moves past the move-number
    // threshold are searched shallower; a fail-high triggers a full-depth
    // re-search. The hash move, captures, promotions, giving-check moves and
    // moves made while in check are never reduced. Off by default: the A/B
    // evidence in task 056 shows that reduced probes cannot see quiet mating
    // setups that arrive late in the move order (the curated KRK and KQK
    // mate fixtures regress), and the tactical corpus is authoritative.
    bool useLateMoveReduction = false;
    int lateMoveReductionMinDepth = 3;
    int lateMoveReductionMinMoveNumber = 4;

    // Check extensions: search a check evasion one ply deeper, bounded by a
    // per-line extension budget on top of the kMaxPly guard. Off by default:
    // the A/B evidence in task 056 shows check cascades inflate the node
    // count of check-heavy positions (in-check +85% at the same depth) and
    // can even cost a ply at fixed time (queen-endgame 8 -> 7), while the
    // tactical corpus is unaffected by turning it off.
    bool useCheckExtension = false;
    int checkExtensionCap = 4;

    // Delta pruning in quiescence: skip a capture that cannot raise alpha even
    // with this margin above the captured value. Never applied in check or
    // when alpha is near a mate score.
    bool useDeltaPruning = true;
    int deltaPruningMargin = 200;

    // Static-exchange pruning of quiescence captures (task 034): skip a
    // capture the SEE proves loses material. Never applied in check, to
    // promotions, or when alpha is near the mate band. Off by default with
    // recorded evidence: on this 0x88 engine a SEE call costs one board copy
    // plus an attacker scan per quiescence capture, roughly doubling the
    // per-node rate, so the node savings turn into a one-ply loss at fixed
    // time on four of eight bench positions (startpos, perft5-promotion,
    // queen-endgame, in-check). Revisit with bitboards (task 033).
    bool useStaticExchangePruning = false;

    bool operator==(const PruningSettings& other) const {
        return useNullMove == other.useNullMove &&
               nullMoveReduction == other.nullMoveReduction &&
               useLateMoveReduction == other.useLateMoveReduction &&
               lateMoveReductionMinDepth == other.lateMoveReductionMinDepth &&
               lateMoveReductionMinMoveNumber ==
                   other.lateMoveReductionMinMoveNumber &&
               useCheckExtension == other.useCheckExtension &&
               checkExtensionCap == other.checkExtensionCap &&
               useDeltaPruning == other.useDeltaPruning &&
               deltaPruningMargin == other.deltaPruningMargin &&
               useStaticExchangePruning == other.useStaticExchangePruning;
    }
    bool operator!=(const PruningSettings& other) const {
        return !(*this == other);
    }
};

struct SearchLimits {
    // Omitted depth uses a small deterministic default (4) for unbounded
    // searches and a high iteration cap when a clock or movetime is present.
    std::optional<int> depth;
    // UCI analysis mode: no default depth/time limit. The adapter holds a
    // completed/terminal result until the explicit stop transaction.
    bool infinite = false;
    EvaluationParameters evaluation;
    ForcingSearchSettings forcing;

    // Node limits are optional. `nodes` is a compatibility alias for callers
    // that use the shorter name; `nodeLimit` takes precedence when both are set.
    std::optional<std::uint64_t> nodeLimit;
    std::optional<std::uint64_t> nodes;

    // Clock fields are applied by task 024. `movetime` takes precedence over
    // the side clock; omit all time fields for a depth/node-only search.
    std::optional<std::int64_t> moveTimeMs;
    std::optional<std::int64_t> whiteTimeMs;
    std::optional<std::int64_t> blackTimeMs;
    std::optional<std::int64_t> whiteIncrementMs;
    std::optional<std::int64_t> blackIncrementMs;
    std::optional<int> movesToGo;

    // Ordering is deterministic: captures use MVV-LVA, then promotions,
    // killers, and history. Disabling it is useful for A/B tests that verify
    // the minimax score is unchanged.
    bool useMoveOrdering = true;

    // Quiescence is enabled for normal search. Tests can disable it to measure
    // the horizon-effect improvement at the same nominal depth. Quiescence
    // extends capture/promotion lines to ForcingSearchSettings::max_plies and
    // keeps checked lines alive to hard_plies before aborting the iteration.
    bool useQuiescence = true;

    // The table belongs to the Searcher; disabling it is useful for A/B tests.
    bool useTranspositionTable = true;

    // Selective pruning guards and thresholds. Disabling everything returns
    // the task 055 search exactly, which stays the A/B oracle for this task.
    PruningSettings pruning;

    // Principal-variation search windows every interior move after the first
    // with a zero window (alpha, alpha + 1) and re-searches it with the full
    // window when the zero-window probe improves alpha. The re-search makes
    // scores, best moves and PVs identical to the full-window search while
    // cutting nodes where the zero window proves a move is not better.
    bool usePrincipalVariation = true;

    // Narrows the root window of each iterative-deepening iteration around the
    // previous iteration's score from depth 3 on, widening on fail-low and
    // fail-high and falling back to the full window after two failures or when
    // the score enters the mate band. Only a search that finishes inside its
    // window is reported, and its score is the exact full-window score.
    bool useAspirationWindows = true;

    // Full evaluation is the production default. Material selects the
    // material-only baseline used by the self-play harness. A table entry's
    // score depends on this mode; use a separate Searcher or clear the table
    // when switching modes.
    EvaluationMode evaluationMode = EvaluationMode::Full;

    std::optional<std::uint64_t> EffectiveNodeLimit() const {
        return nodeLimit.has_value() ? nodeLimit : nodes;
    }
};

struct SearchInfo {
    // Last fully completed iterative-deepening depth, or 0 when no depth
    // completed (for example when the initial stop flag was already set).
    int depth = 0;

    // Score in centipawns from the root side-to-move perspective. Mate scores
    // use the convention described above.
    int scoreCp = 0;

    std::uint64_t nodes = 0;

    // Legal principal variation from the root. It is empty for terminal
    // positions; when no depth completed, it contains the legal fallback move.
    std::vector<Move> pv;

    // The first PV move, or nullopt when the root has no legal move or the
    // position is already terminal.
    std::optional<Move> bestMove;
};

// Optional progress observer. It is called on the searching thread after each
// completed iterative-deepening iteration, with the same information that
// Search() will return for that iteration. The callback must not re-enter the
// Searcher; UCI uses it to stream `info` lines.
using SearchProgressCallback = std::function<void(const SearchInfo&)>;

class Searcher {
public:
    explicit Searcher(std::size_t transpositionTableMegabytes = 16);

    void ClearTranspositionTable();
    void ResizeTranspositionTable(std::size_t megabytes);
    std::size_t TranspositionTableMegabytes() const;

    // Searches a copy of `position`; the caller's position is never modified.
    // The search polls `stop` and any hard deadline every 32 nodes and returns
    // the best information from the last completed depth. `stop` may be set
    // before or during the search. When `on_progress` is set, it is invoked
    // after every completed iteration, before the search advances or stops.
    SearchInfo Search(const Position& position, const SearchLimits& limits,
                      std::atomic_bool& stop,
                      const SearchProgressCallback& on_progress = {});

private:
    TranspositionTable table_;
    std::optional<SearchLimits> previous_policy_;
};

}  // namespace chess

#endif  // CHESS_SEARCH_HPP_
