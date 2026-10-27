#ifndef CHESS_PERFT_HPP_
#define CHESS_PERFT_HPP_

#include <cstdint>

#include "chess/position.hpp"

namespace chess {

// Counts leaf nodes at the requested depth using legal moves.
//
// The position is restored to its original state before returning, including
// any history it had on entry. Negative depths are rejected.
std::uint64_t Perft(Position& position, int depth);

}  // namespace chess

#endif  // CHESS_PERFT_HPP_
