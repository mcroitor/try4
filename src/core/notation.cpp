#include "chess/notation.hpp"

#include <cctype>
#include <cstddef>
#include <string>
#include <vector>

namespace chess {
namespace {

std::string Lowercase(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char value : text) {
        result += static_cast<char>(
            std::tolower(static_cast<unsigned char>(value)));
    }
    return result;
}

std::optional<PieceType> PromotionPiece(char value) {
    switch (static_cast<char>(
        std::tolower(static_cast<unsigned char>(value)))) {
        case 'q':
            return PieceType::Queen;
        case 'r':
            return PieceType::Rook;
        case 'b':
            return PieceType::Bishop;
        case 'n':
            return PieceType::Knight;
        default:
            return std::nullopt;
    }
}

std::optional<Move> FindLegal(const Position& position, const Move& move) {
    for (const Move& candidate : position.LegalMoves()) {
        if (candidate.from == move.from && candidate.to == move.to &&
            candidate.promotion == move.promotion) {
            return candidate;
        }
    }
    return std::nullopt;
}

char FileChar(Square square) {
    return static_cast<char>('a' + square.File());
}

char RankChar(Square square) {
    return static_cast<char>('1' + square.Rank());
}

std::string WithoutCheckSuffix(std::string text) {
    while (!text.empty() && (text.back() == '+' || text.back() == '#')) {
        text.pop_back();
    }
    return text;
}

}  // namespace

std::optional<Move> ParseUci(const Position& position, std::string_view text) {
    const std::string normalized = Lowercase(text);
    if (normalized.size() != 4 && normalized.size() != 5) {
        return std::nullopt;
    }

    const std::optional<Square> from =
        Square::Parse(std::string_view(normalized).substr(0, 2));
    const std::optional<Square> to =
        Square::Parse(std::string_view(normalized).substr(2, 2));
    if (!from.has_value() || !to.has_value()) {
        return std::nullopt;
    }

    PieceType promotion = PieceType::None;
    if (normalized.size() == 5) {
        const std::optional<PieceType> parsed = PromotionPiece(normalized[4]);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        promotion = *parsed;
    }

    const MoveFlag flags =
        promotion == PieceType::None ? MoveFlag::None : MoveFlag::Promotion;
    return FindLegal(position, Move(*from, *to, flags, promotion));
}

std::string ToSan(const Position& position, const Move& requested) {
    const std::optional<Move> found = FindLegal(position, requested);
    if (!found.has_value()) {
        return "";
    }
    const Move move = *found;
    const Piece moving_piece = position.PieceAt(move.from);
    if (moving_piece.IsEmpty()) {
        return "";
    }

    std::string san;
    if (move.HasFlag(MoveFlag::CastleKing)) {
        san = "O-O";
    } else if (move.HasFlag(MoveFlag::CastleQueen)) {
        san = "O-O-O";
    } else {
        const bool capture = move.HasFlag(MoveFlag::Capture);
        const PieceType type = moving_piece.Type();

        if (type == PieceType::Pawn) {
            if (capture) {
                san += FileChar(move.from);
                san += 'x';
            }
            san += move.to.ToString();
            if (move.HasFlag(MoveFlag::Promotion)) {
                san += '=';
                san += PieceTypeToChar(move.promotion);
            }
        } else {
            san += PieceTypeToChar(type);

            bool another_piece = false;
            bool same_file = false;
            bool same_rank = false;
            for (const Move& other : position.LegalMoves()) {
                if (other.to != move.to || other.from == move.from) {
                    continue;
                }
                const Piece other_piece = position.PieceAt(other.from);
                if (other_piece.Color() != moving_piece.Color() ||
                    other_piece.Type() != type) {
                    continue;
                }
                another_piece = true;
                if (other.from.File() == move.from.File()) {
                    same_file = true;
                }
                if (other.from.Rank() == move.from.Rank()) {
                    same_rank = true;
                }
            }

            if (another_piece) {
                if (!same_file) {
                    san += FileChar(move.from);
                } else if (!same_rank) {
                    san += RankChar(move.from);
                } else {
                    san += FileChar(move.from);
                    san += RankChar(move.from);
                }
            }

            if (capture) {
                san += 'x';
            }
            san += move.to.ToString();
        }
    }

    Position after = position;
    if (!after.MakeMove(move)) {
        return "";
    }
    const Color opponent = Opposite(position.SideToMove());
    if (after.InCheck(opponent)) {
        san += after.LegalMoves().empty() ? '#' : '+';
    }
    return san;
}

std::optional<Move> ParseSan(const Position& position, std::string_view text) {
    std::string normalized;
    normalized.reserve(text.size());
    for (const char value : text) {
        if (value == '0') {
            normalized += 'O';
        } else {
            normalized += value;
        }
    }
    normalized = WithoutCheckSuffix(normalized);
    if (normalized.empty()) {
        return std::nullopt;
    }

    for (const Move& move : position.LegalMoves()) {
        const std::string san = WithoutCheckSuffix(ToSan(position, move));
        if (san == normalized) {
            return move;
        }
    }
    return std::nullopt;
}

}  // namespace chess
