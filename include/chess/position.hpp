#ifndef CHESS_POSITION_HPP_
#define CHESS_POSITION_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "chess/movegen.hpp"
#include "chess/types.hpp"

namespace chess {

class Position {
public:
    Position();

    // Resets to an empty board, white to move, no castling rights, no en
    // passant target, halfmove clock 0, and fullmove number 1.
    void Clear();

    // Resets to the standard initial position.
    void SetupInitial();

    // Returns an empty piece for an invalid square.
    Piece PieceAt(Square square) const;

    // Returns false and leaves the position unchanged for an invalid square.
    bool SetPiece(Square square, Piece piece);

    Color SideToMove() const { return side_to_move_; }
    void SetSideToMove(Color side);

    // Returns the king square for color, or std::nullopt when no king exists.
    std::optional<Square> KingSquare(Color color) const;

    // True when square is attacked by any piece of by_color.
    bool IsSquareAttacked(Square square, Color by_color) const;

    // True when the king of color is attacked by the opponent.
    bool InCheck(Color color) const;

    chess::CastlingRights CastlingRights() const { return castling_rights_; }
    void SetCastlingRights(chess::CastlingRights rights);

    std::optional<Square> EnPassant() const { return en_passant_; }
    void SetEnPassant(std::optional<Square> square);

    // True when every set castling right has the corresponding king and rook
    // on their home squares. Rights with missing pieces are reported here so
    // callers can reject malformed positions without aborting generation.
    bool CastlingRightsConsistent() const;

    int HalfmoveClock() const { return halfmove_clock_; }
    void SetHalfmoveClock(int value);

    int FullmoveNumber() const { return fullmove_number_; }
    void SetFullmoveNumber(int value);

    // Eight ranks with rank 8 at the top, using piece characters and spaces
    // for empty squares. Each rank is terminated by a newline.
    std::string ToString() const;

    // Legal moves for the side to move. Castling through check, en passant
    // discovered check, and all ordinary king-safety restrictions are applied.
    std::vector<Move> LegalMoves();
    std::vector<Move> LegalMoves() const;

    // Fills out with the legal moves for the side to move, in exactly the same
    // order as LegalMoves(), and returns the number of moves. No vector or
    // Position copy is created. The method is logically const: it temporarily
    // makes and unmakes each candidate move using this position's history, then
    // restores the position. It is not thread-safe or reentrant. Call
    // ReserveHistory() when the additional history capacity must be avoided.
    std::size_t GenerateLegalMoves(MoveList* out) const;

    // True when move is in LegalMoves().
    bool IsLegal(const Move& move) const;

    // Checkmate/stalemate for the side to move. Draw categories are added in
    // the later draw-rules task.
    GameResult Result() const;

    // Applies a legal move and returns false without changing state otherwise.
    bool MakeMove(const Move& move);

    // Applies move without repeating legality generation. Precondition: move
    // came from GenerateLegalMoves() or LegalMoves() for this exact position.
    // Debug builds validate the precondition and throw std::logic_error for an
    // illegal or inexact move; release builds do not check, and an illegal move
    // can corrupt the position. The state transition and history push are
    // identical to MakeMove(). Always returns true when the precondition holds.
    bool MakeMoveAssumeLegal(const Move& move);

    // True when the current position has appeared at least three times in the
    // game history. This scan uses the incrementally maintained 64-bit Zobrist
    // hash and does not copy positions. The hash covers piece placement, side
    // to move, castling rights and the en-passant file; a 64-bit hash collision
    // is therefore accepted as the same position. Result() applies the
    // fifty-move rule, repetition and insufficient-material precedence.
    bool IsRepetition() const;

    // Reserves history capacity without changing HistoryDepth() or position
    // state. Lower values do not shrink the existing capacity.
    void ReserveHistory(std::size_t capacity);

    // Restores the position before the most recent make.
    // Returns false when there is no history.
    bool UndoMove();

    std::size_t HistoryDepth() const { return history_.size(); }

    // Incrementally maintained Zobrist hash. ComputeHash() is the independent
    // full-recomputation oracle used by tests.
    std::uint64_t Hash() const { return hash_; }
    std::uint64_t ComputeHash() const;

    // Strict FEN parsing. Returns std::nullopt and sets *error when the input
    // is invalid. On failure the caller's position is never modified.
    static std::optional<Position> FromFen(std::string_view fen,
                                           std::string* error = nullptr);

    std::string ToFen() const;

    bool operator==(const Position& other) const;
    bool operator!=(const Position& other) const { return !(*this == other); }

private:
    friend class MoveGenerator;
    // Built by the evaluation translation unit to take a single-pass snapshot
    // of the board. This keeps the storage private while letting the
    // evaluation hot path read squares without repeated PieceAt bounds checks.
    friend struct BoardSummary;

    static constexpr bool IsOnBoard(int index) {
        return index >= 0 && index < 128 && (index & 0x88) == 0;
    }

    static constexpr int To0x88(Square square) {
        return (square.Rank() << 4) | square.File();
    }

    static std::optional<Square> From0x88(int index);

    struct HistoryEntry {
        Move move;
        Piece captured;
        chess::CastlingRights castling_rights = kNoCastlingRights;
        std::optional<Square> en_passant;
        int halfmove_clock = 0;
        int fullmove_number = 1;
        Color side_to_move = Color::White;
        std::uint64_t hash = 0;
    };

    void MakeMoveUnchecked(const Move& move);
    std::optional<Move> FindLegalMove(const Move& move) const;
    std::optional<int> HashEnPassantFile() const;
    bool IsThreefoldRepetition() const;
    bool HasInsufficientMaterial() const;
    friend std::uint64_t Perft(Position& position, int depth);

    std::array<Piece, 128> board_{};
    Color side_to_move_ = Color::White;
    chess::CastlingRights castling_rights_ = kNoCastlingRights;
    std::optional<Square> en_passant_;
    int halfmove_clock_ = 0;
    int fullmove_number_ = 1;
    std::uint64_t hash_ = 0;
    std::vector<HistoryEntry> history_;
};

}  // namespace chess

#endif  // CHESS_POSITION_HPP_
