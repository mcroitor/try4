#ifndef CHESS_TIME_MANAGER_HPP_
#define CHESS_TIME_MANAGER_HPP_

#include <cstdint>
#include <optional>

#include "chess/search.hpp"

namespace chess {

struct TimeBudget {
    // Soft target: do not begin a new iterative-deepening iteration after this.
    std::optional<std::int64_t> softMs;

    // Hard ceiling: the current iteration is aborted at this point.
    std::optional<std::int64_t> hardMs;

    bool HasLimit() const { return hardMs.has_value(); }
};

// Computes a soft target and hard ceiling for the side to move. `movetime`
// takes precedence over the clock. A missing clock produces no limit.
TimeBudget ComputeTimeBudget(const SearchLimits& limits, Color side);

}  // namespace chess

#endif  // CHESS_TIME_MANAGER_HPP_
