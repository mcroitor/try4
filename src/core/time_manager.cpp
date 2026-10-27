#include "chess/time_manager.hpp"

#include <algorithm>
#include <cstdint>

namespace chess {
namespace {

constexpr int kDefaultMovesToGo = 30;
constexpr std::int64_t kMinimumReserveMs = 5;

std::int64_t Positive(std::optional<std::int64_t> value) {
    return value.has_value() ? std::max<std::int64_t>(0, *value) : 0;
}

}  // namespace

TimeBudget ComputeTimeBudget(const SearchLimits& limits, Color side) {
    if (limits.moveTimeMs.has_value()) {
        const std::int64_t move_time = Positive(limits.moveTimeMs);
        return {move_time, move_time};
    }

    const std::optional<std::int64_t> clock =
        side == Color::White ? limits.whiteTimeMs : limits.blackTimeMs;
    if (!clock.has_value()) {
        return {};
    }

    const std::int64_t time = Positive(clock);
    const std::int64_t increment =
        Positive(side == Color::White ? limits.whiteIncrementMs
                                      : limits.blackIncrementMs);
    const int moves_to_go =
        std::max(1, limits.movesToGo.value_or(kDefaultMovesToGo));

    std::int64_t soft = time / moves_to_go + increment / 2;
    soft = std::min(soft, time);
    if (time > 0 && soft < 1) {
        soft = 1;
    }

    const std::int64_t reserve =
        std::max(kMinimumReserveMs, time / 100);
    const std::int64_t available = std::max<std::int64_t>(0, time - reserve);
    std::int64_t hard = std::max(soft + soft, soft + 1);
    hard = std::min(hard, available);
    if (hard < 0) {
        hard = 0;
    }
    soft = std::min(soft, hard);
    return {soft, hard};
}

}  // namespace chess
