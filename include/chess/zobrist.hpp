#ifndef CHESS_ZOBRIST_HPP_
#define CHESS_ZOBRIST_HPP_

#include <cstdint>

#include "chess/types.hpp"

namespace chess::zobrist {

// Deterministic keys generated from a fixed seed at first use.
std::uint64_t PieceKey(Color color, PieceType type, Square square);
std::uint64_t SideKey();
std::uint64_t CastlingKey(CastlingRights rights);
std::uint64_t EnPassantKey(int file);

}  // namespace chess::zobrist

#endif  // CHESS_ZOBRIST_HPP_
