#include "chess/types.hpp"

#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace chess {

char PieceTypeToChar(PieceType type) {
    switch (type) {
        case PieceType::None:
            return ' ';
        case PieceType::Pawn:
            return 'P';
        case PieceType::Knight:
            return 'N';
        case PieceType::Bishop:
            return 'B';
        case PieceType::Rook:
            return 'R';
        case PieceType::Queen:
            return 'Q';
        case PieceType::King:
            return 'K';
    }
    return ' ';
}

std::string Piece::ToString() const {
    if (IsEmpty()) {
        return " ";
    }

    char value = PieceTypeToChar(type_);
    if (color_ == Color::Black) {
        value = static_cast<char>(
            std::tolower(static_cast<unsigned char>(value)));
    }
    return std::string(1, value);
}

std::optional<Piece> PieceFromChar(char value) {
    if (value >= 'A' && value <= 'Z') {
        const char lower = static_cast<char>(value - 'A' + 'a');
        switch (lower) {
            case 'p':
                return Piece(Color::White, PieceType::Pawn);
            case 'n':
                return Piece(Color::White, PieceType::Knight);
            case 'b':
                return Piece(Color::White, PieceType::Bishop);
            case 'r':
                return Piece(Color::White, PieceType::Rook);
            case 'q':
                return Piece(Color::White, PieceType::Queen);
            case 'k':
                return Piece(Color::White, PieceType::King);
            default:
                return std::nullopt;
        }
    }

    if (value >= 'a' && value <= 'z') {
        switch (value) {
            case 'p':
                return Piece(Color::Black, PieceType::Pawn);
            case 'n':
                return Piece(Color::Black, PieceType::Knight);
            case 'b':
                return Piece(Color::Black, PieceType::Bishop);
            case 'r':
                return Piece(Color::Black, PieceType::Rook);
            case 'q':
                return Piece(Color::Black, PieceType::Queen);
            case 'k':
                return Piece(Color::Black, PieceType::King);
            default:
                return std::nullopt;
        }
    }

    return std::nullopt;
}

std::string Square::ToString() const {
    if (!IsValid()) {
        return "";
    }

    const char file = static_cast<char>('a' + File());
    const char rank = static_cast<char>('1' + Rank());
    return std::string{file, rank};
}

std::optional<Square> Square::Parse(std::string_view text) {
    if (text.size() != 2) {
        return std::nullopt;
    }

    char file = text[0];
    if (file >= 'A' && file <= 'H') {
        file = static_cast<char>(file - 'A' + 'a');
    }
    const char rank = text[1];

    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') {
        return std::nullopt;
    }

    const auto index = static_cast<std::uint8_t>(
        (rank - '1') * 8 + (file - 'a'));
    return Square(index);
}

std::string Move::ToUci() const {
    if (IsNull()) {
        return "0000";
    }
    if (!IsValid()) {
        return "";
    }

    std::string uci = from.ToString() + to.ToString();
    if (HasFlag(MoveFlag::Promotion)) {
        char piece = PieceTypeToChar(promotion);
        piece = static_cast<char>(
            std::tolower(static_cast<unsigned char>(piece)));
        uci += piece;
    }
    return uci;
}

}  // namespace chess
