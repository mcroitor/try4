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
