#include "chess/time_manager.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace chess {
namespace {

constexpr int kDefaultMovesToGo = 30;
constexpr std::int64_t kMinimumSafetyMs = 5;

std::int64_t Positive(std::optional<std::int64_t> value) {
    return value.has_value() ? std::max<std::int64_t>(0, *value) : 0;
}

// Forensic hook (set TRY4_TIME_DEBUG=1 to enable): logs every clock budget
// decision to stderr as "budget rem= mtg= soft= hard=", for match
// forensics in real managers. Empty stderr without the variable; tests
// assert that, so this must stay env-gated.
void TimeDebug(const SearchLimits& limits, Color side, std::int64_t remaining,
               int moves_to_go, const TimeBudget& budget) {
    if (std::getenv("TRY4_TIME_DEBUG") == nullptr) {
        return;
    }
    std::fprintf(stderr, "budget side=%s rem=%lld mtg=%d soft=%lld hard=%lld\n",
                 side == Color::White ? "w" : "b",
                 static_cast<long long>(remaining), moves_to_go,
                 budget.softMs.has_value()
                     ? static_cast<long long>(*budget.softMs)
                     : -1LL,
                 budget.hardMs.has_value()
                     ? static_cast<long long>(*budget.hardMs)
                     : -1LL);
    (void)limits;
}

}  // namespace

// Task 057 budget contract:
// - soft = min(remaining / 2, remaining / moves_to_go + increment * 3 / 4);
// - hard = min(remaining / 2, soft * 5 / 2);
// - safety = move_overhead_ms + max(5 ms, remaining / 100);
// - hard = max(soft, min(hard, remaining - safety));
// `movetime` keeps precedence: it is an exact budget, so soft == hard is the
// movetime clamped by the remaining-minus-safety ceiling. All quantities stay
// nonnegative and hard >= soft.
TimeBudget ComputeTimeBudget(const SearchLimits& limits, Color side,
                             std::int64_t move_overhead_ms) {
    const std::int64_t overhead = move_overhead_ms > 0 ? move_overhead_ms : 0;

    if (limits.moveTimeMs.has_value()) {
        const std::int64_t remaining = Positive(limits.moveTimeMs);
        const std::int64_t safety =
            overhead + std::max<std::int64_t>(kMinimumSafetyMs, remaining / 100);
        const std::int64_t hard =
            std::max<std::int64_t>(0, remaining - safety);
        const TimeBudget budget{hard, hard};
        TimeDebug(limits, side, remaining, 0, budget);
        return budget;
    }

    const std::optional<std::int64_t> clock =
        side == Color::White ? limits.whiteTimeMs : limits.blackTimeMs;
    if (!clock.has_value()) {
        return {};
    }

    const std::int64_t remaining = Positive(clock);
    if (remaining == 0) {
        const TimeBudget budget{0, 0};
        TimeDebug(limits, side, remaining, 0, budget);
        return budget;
    }
    const std::int64_t increment =
        Positive(side == Color::White ? limits.whiteIncrementMs
                                      : limits.blackIncrementMs);
    const int moves_to_go =
        std::max(1, limits.movesToGo.value_or(kDefaultMovesToGo));

    std::int64_t soft = remaining / 2;
    const std::int64_t per_move =
        remaining / moves_to_go + increment * 3 / 4;
    soft = std::min(soft, per_move);
    if (soft < 1) {
        soft = 1;
    }

    std::int64_t hard = std::min(remaining / 2, soft * 5 / 2);
    const std::int64_t safety =
        overhead + std::max<std::int64_t>(kMinimumSafetyMs, remaining / 100);
    const std::int64_t ceiling = std::max<std::int64_t>(0, remaining - safety);
    hard = std::max(soft, std::min(hard, ceiling));
    const TimeBudget budget{soft, hard};
    TimeDebug(limits, side, remaining, moves_to_go, budget);
    return budget;
}

}  // namespace chess