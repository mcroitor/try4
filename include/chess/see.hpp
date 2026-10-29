#ifndef CHESS_SEE_HPP_
#define CHESS_SEE_HPP_

#include "chess/evaluation.hpp"
#include "chess/position.hpp"
#include "chess/types.hpp"

namespace chess {

// Static exchange evaluation (task 034): the material balance, in
// centipawns, of the forced capture sequence that starts with `move` and
// alternates recaptures on the target square until one side runs out of
// attackers. Negative values mean the capture loses material even though it
// wins the victim. Only ordinary captures (including en passant) make sense;
// the caller must not prune promotions or non-captures with this value.
// Sliders behind a captured blocker are picked up on re-scan (x-rays).
int See(const Position& position, const Move& move,
        const EvaluationParameters& parameters);

}  // namespace chess

#endif  // CHESS_SEE_HPP_