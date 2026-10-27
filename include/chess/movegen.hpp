#ifndef CHESS_MOVEGEN_HPP_
#define CHESS_MOVEGEN_HPP_

#include <array>
#include <cassert>
#include <cstddef>
#include <optional>

#include "chess/types.hpp"

namespace chess {

class Position;

// Fixed-capacity move container used by generation and search. The maximum
// number of legal moves in a chess position is 218, so 256 is a safe bound.
class MoveList {
public:
    static constexpr std::size_t kCapacity = 256;

    void Add(Move move) {
        assert(size_ < kCapacity);
        moves_[size_] = move;
        ++size_;
    }

    void Clear() { size_ = 0; }
    bool Empty() const { return size_ == 0; }
    std::size_t Size() const { return size_; }

    // Low-level in-place compaction helper for allocation-free search. The
    // caller is responsible for providing a valid index range.
    void Resize(std::size_t size) {
        assert(size <= kCapacity);
        size_ = size;
    }

    const Move& operator[](std::size_t index) const {
        assert(index < size_);
        return moves_[index];
    }

    Move& operator[](std::size_t index) {
        assert(index < size_);
        return moves_[index];
    }

    const Move* begin() const { return moves_.data(); }
    const Move* end() const { return moves_.data() + size_; }

private:
    std::array<Move, kCapacity> moves_;
    std::size_t size_ = 0;
};

class MoveGenerator {
public:
    // True when square is attacked by any piece of by_color.
    static bool IsSquareAttacked(const Position& position, Square square,
                                 Color by_color);

    // Generates pseudo-legal moves for the side to move. Castling and en
    // passant are added by task 014; legality is filtered by task 015.
    static void GeneratePseudoLegal(const Position& position,
                                    MoveList* moves);

private:
    static void GeneratePawnMoves(const Position& position, int from,
                                  Color color, MoveList* moves);
    static void GenerateKnightMoves(const Position& position, int from,
                                    Color color, MoveList* moves);
    static void GenerateKingMoves(const Position& position, int from,
                                  Color color, MoveList* moves);
    static void GenerateSlidingMoves(const Position& position, int from,
                                     Color color, MoveList* moves,
                                     const int* offsets,
                                     std::size_t offset_count);
    static void AddPromotions(MoveList* moves, int from, int to,
                              MoveFlag flags);
    static void AddMove(MoveList* moves, int from, int to, MoveFlag flags,
                        PieceType promotion = PieceType::None);
};

}  // namespace chess

#endif  // CHESS_MOVEGEN_HPP_
