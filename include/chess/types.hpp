#ifndef CHESS_TYPES_HPP_
#define CHESS_TYPES_HPP_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace chess {

enum class Color : std::uint8_t {
    White = 0,
    Black = 1,
};

constexpr Color Opposite(Color color) {
    return color == Color::White ? Color::Black : Color::White;
}

enum class GameResult : std::uint8_t {
    Ongoing = 0,
    Checkmate,
    Stalemate,
    DrawFiftyMove,
    DrawRepetition,
    DrawInsufficientMaterial,

    // Short aliases used by some callers.
    FiftyMove = DrawFiftyMove,
    ThreefoldRepetition = DrawRepetition,
    InsufficientMaterial = DrawInsufficientMaterial,
};

enum class PieceType : std::uint8_t {
    None = 0,
    Pawn,
    Knight,
    Bishop,
    Rook,
    Queen,
    King,
};

// Returns 'P', 'N', 'B', 'R', 'Q', 'K', or a space for PieceType::None.
char PieceTypeToChar(PieceType type);

class Piece {
public:
    constexpr Piece() = default;
    constexpr Piece(chess::Color color, chess::PieceType type)
        : color_(color), type_(type) {}

    constexpr chess::Color Color() const { return color_; }
    constexpr PieceType Type() const { return type_; }
    constexpr bool IsEmpty() const { return type_ == PieceType::None; }

    // Uppercase for white pieces, lowercase for black pieces, and a single
    // space for an empty square.
    std::string ToString() const;

    friend constexpr bool operator==(Piece lhs, Piece rhs) {
        return lhs.color_ == rhs.color_ && lhs.type_ == rhs.type_;
    }

    friend constexpr bool operator!=(Piece lhs, Piece rhs) {
        return !(lhs == rhs);
    }

private:
    chess::Color color_ = chess::Color::White;
    PieceType type_ = PieceType::None;
};

// Parses a FEN piece letter. Uppercase letters are white, lowercase black.
std::optional<Piece> PieceFromChar(char value);

class Square {
public:
    static constexpr std::uint8_t kInvalidIndex = 64;

    constexpr Square() = default;
    explicit constexpr Square(std::uint8_t index) : index_(index) {}

    constexpr bool IsValid() const { return index_ < kInvalidIndex; }
    constexpr std::uint8_t Index() const { return index_; }

    // File is 0..7 for a..h; rank is 0..7 for ranks 1..8.
    // Both functions require IsValid().
    constexpr int File() const { return static_cast<int>(index_ % 8); }
    constexpr int Rank() const { return static_cast<int>(index_ / 8); }

    // Returns an empty string for an invalid square.
    std::string ToString() const;

    // Accepts exactly two characters in the range a1..h8, case-insensitively.
    static std::optional<Square> Parse(std::string_view text);
    static std::optional<Square> FromAlgebraic(std::string_view text) {
        return Parse(text);
    }

    friend constexpr bool operator==(Square lhs, Square rhs) {
        return lhs.index_ == rhs.index_;
    }

    friend constexpr bool operator!=(Square lhs, Square rhs) {
        return !(lhs == rhs);
    }

    friend constexpr bool operator<(Square lhs, Square rhs) {
        return lhs.index_ < rhs.index_;
    }

private:
    std::uint8_t index_ = kInvalidIndex;
};

enum class MoveFlag : std::uint8_t {
    None = 0,
    Capture = 1U << 0,
    DoublePush = 1U << 1,
    EnPassant = 1U << 2,
    CastleKing = 1U << 3,
    CastleQueen = 1U << 4,
    Promotion = 1U << 5,
};

constexpr MoveFlag operator|(MoveFlag lhs, MoveFlag rhs) {
    return static_cast<MoveFlag>(
        static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

constexpr MoveFlag& operator|=(MoveFlag& lhs, MoveFlag rhs) {
    lhs = lhs | rhs;
    return lhs;
}

constexpr MoveFlag operator&(MoveFlag lhs, MoveFlag rhs) {
    return static_cast<MoveFlag>(
        static_cast<std::uint8_t>(lhs) & static_cast<std::uint8_t>(rhs));
}

constexpr bool HasFlag(MoveFlag value, MoveFlag flag) {
    return (static_cast<std::uint8_t>(value) &
            static_cast<std::uint8_t>(flag)) != 0;
}

constexpr bool IsPromotionPiece(PieceType type) {
    return type == PieceType::Knight || type == PieceType::Bishop ||
           type == PieceType::Rook || type == PieceType::Queen;
}

class Move {
public:
    // A default-constructed move is the null move: both squares are invalid.
    constexpr Move() = default;

    // Promotion is normalized to None unless MoveFlag::Promotion is present.
    constexpr Move(Square from, Square to, MoveFlag flags = MoveFlag::None,
                   PieceType promotion = PieceType::None)
        : from(from),
          to(to),
          flags(flags),
          promotion(chess::HasFlag(flags, MoveFlag::Promotion)
                        ? promotion
                        : PieceType::None) {}

    constexpr bool IsNull() const { return !from.IsValid() || !to.IsValid(); }

    constexpr bool HasFlag(MoveFlag flag) const { return chess::HasFlag(flags, flag); }

    // A null move is not a board move. Ordinary moves require two valid,
    // distinct squares, and promotion must name a legal promotion piece.
    constexpr bool IsValid() const {
        if (IsNull() || from == to) {
            return false;
        }
        if (HasFlag(MoveFlag::Promotion)) {
            return IsPromotionPiece(promotion);
        }
        return promotion == PieceType::None;
    }

    // UCI coordinate notation, for example "e2e4" or "e7e8q".
    // Returns "0000" for the null move and an empty string for invalid moves.
    std::string ToUci() const;

    friend constexpr bool operator==(const Move& lhs, const Move& rhs) {
        return lhs.from == rhs.from && lhs.to == rhs.to &&
               lhs.flags == rhs.flags && lhs.promotion == rhs.promotion;
    }

    friend constexpr bool operator!=(const Move& lhs, const Move& rhs) {
        return !(lhs == rhs);
    }

    Square from;
    Square to;
    MoveFlag flags = MoveFlag::None;
    PieceType promotion = PieceType::None;
};

enum class CastlingRight : std::uint8_t {
    WhiteKingSide = 1U << 0,
    WhiteQueenSide = 1U << 1,
    BlackKingSide = 1U << 2,
    BlackQueenSide = 1U << 3,
};

using CastlingRights = std::uint8_t;

constexpr CastlingRights kNoCastlingRights = 0;
constexpr CastlingRights kAllCastlingRights =
    static_cast<CastlingRights>(CastlingRight::WhiteKingSide) |
    static_cast<CastlingRights>(CastlingRight::WhiteQueenSide) |
    static_cast<CastlingRights>(CastlingRight::BlackKingSide) |
    static_cast<CastlingRights>(CastlingRight::BlackQueenSide);

constexpr CastlingRights ToMask(CastlingRight right) {
    return static_cast<CastlingRights>(right);
}

constexpr bool HasCastlingRight(CastlingRights rights, CastlingRight right) {
    return (rights & ToMask(right)) != 0;
}

constexpr CastlingRights WithCastlingRight(CastlingRights rights,
                                           CastlingRight right) {
    return static_cast<CastlingRights>(rights | ToMask(right));
}

constexpr CastlingRights WithoutCastlingRight(CastlingRights rights,
                                              CastlingRight right) {
    return static_cast<CastlingRights>(rights & ~ToMask(right));
}

}  // namespace chess

#endif  // CHESS_TYPES_HPP_
