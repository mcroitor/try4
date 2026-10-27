#include "chess/perft.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace chess {

std::uint64_t Perft(Position& position, int depth) {
    assert(depth >= 0);
    if (depth < 0) {
        return 0;
    }
    if (depth == 0) {
        return 1;
    }

    const std::vector<Move> moves = position.LegalMoves();
    if (depth == 1) {
        return static_cast<std::uint64_t>(moves.size());
    }

    std::uint64_t nodes = 0;
    for (const Move& move : moves) {
        position.MakeMoveUnchecked(move);
        nodes += Perft(position, depth - 1);
        const bool undone = position.UndoMove();
        assert(undone);
        (void)undone;
    }
    return nodes;
}

}  // namespace chess
