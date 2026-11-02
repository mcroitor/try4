#ifndef CHESS_BITBOARD_HPP_
#define CHESS_BITBOARD_HPP_

#include <cstdint>
#include <optional>

#include "chess/types.hpp"

namespace chess {
namespace bitboard {

// 64-bit occupancy masks. Bit i corresponds to Square(i), consistent with the
// existing a1 = 0, h8 = 63 convention.
using Board = std::uint64_t;

constexpr Board kEmptyBoard = 0;
constexpr Board kAllSquares = 0xffffffffffffffffULL;

constexpr Board SquareMask(Square square) {
    return square.IsValid() ? (static_cast<Board>(1) << square.Index())
                            : kEmptyBoard;
}

constexpr bool IsSet(Board board, Square square) {
    return square.IsValid() && (board & SquareMask(square)) != kEmptyBoard;
}

constexpr Board With(Board board, Square square) {
    return square.IsValid() ? (board | SquareMask(square)) : board;
}

constexpr Board Without(Board board, Square square) {
    return square.IsValid() ? (board & ~SquareMask(square)) : board;
}

constexpr Board Move(Board board, Square from, Square to) {
    return With(Without(board, from), to);
}

constexpr bool Any(Board board) {
    return board != kEmptyBoard;
}

constexpr std::uint8_t Count(Board board) {
    std::uint8_t count = 0;
    while (board != kEmptyBoard) {
        board &= board - 1;
        ++count;
    }
    return count;
}

constexpr std::optional<Square> LowestSquare(Board board) {
    if (board == kEmptyBoard) {
        return std::nullopt;
    }
    for (int index = 0; index < 64; ++index) {
        if ((board >> index) & 1) {
            return Square(static_cast<std::uint8_t>(index));
        }
    }
    return std::nullopt;
}

// Removes the lowest set square from `board` and returns it, or nullopt when
// `board` is empty.
constexpr Square HighestSquare(Board board) {
    if (board == kEmptyBoard) {
        return Square();
    }
    for (int index = 63; index >= 0; --index) {
        if ((board >> index) & 1) {
            return Square(static_cast<std::uint8_t>(index));
        }
    }
    return Square();
}

}  // namespace bitboard
}  // namespace chess

#endif  // CHESS_BITBOARD_HPP_