#include "chess/position.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace chess {
namespace {

void SetError(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
}

std::vector<std::string_view> Split(std::string_view text, char delimiter) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = text.find(delimiter, start);
        if (end == std::string_view::npos) {
            fields.push_back(text.substr(start));
            break;
        }
        fields.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return fields;
}

bool ParseNonNegativeInt(std::string_view text, int* value) {
    if (text.empty()) {
        return false;
    }
    int parsed = 0;
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc() || result.ptr != end || parsed < 0) {
        return false;
    }
    *value = parsed;
    return true;
}

bool ParsePositiveInt(std::string_view text, int* value) {
    if (!ParseNonNegativeInt(text, value)) {
        return false;
    }
    return *value >= 1;
}

}  // namespace

std::optional<Position> Position::FromFen(std::string_view fen,
                                          std::string* error) {
    if (error != nullptr) {
        error->clear();
    }

    const std::vector<std::string_view> fields = Split(fen, ' ');
    if (fields.size() != 6) {
        SetError(error, "FEN must contain six space-separated fields");
        return std::nullopt;
    }
    for (const std::string_view field : fields) {
        if (field.empty()) {
            SetError(error, "FEN fields must not be empty");
            return std::nullopt;
        }
    }

    Position position;
    position.Clear();

    const std::vector<std::string_view> ranks = Split(fields[0], '/');
    if (ranks.size() != 8) {
        SetError(error, "piece placement: expected eight ranks");
        return std::nullopt;
    }

    for (int rank_index = 0; rank_index < 8; ++rank_index) {
        const std::string_view rank_text = ranks[rank_index];
        int file = 0;
        for (const char value : rank_text) {
            if (value >= '1' && value <= '8') {
                file += value - '0';
            } else {
                const std::optional<Piece> piece = PieceFromChar(value);
                if (!piece.has_value()) {
                    SetError(error, std::string("piece placement: invalid piece '") +
                                        value + "'");
                    return std::nullopt;
                }
                if (file >= 8) {
                    SetError(error,
                             "piece placement: rank has more than eight files");
                    return std::nullopt;
                }

                const int board_rank = 7 - rank_index;
                const auto square_index =
                    static_cast<std::uint8_t>(board_rank * 8 + file);
                position.SetPiece(Square(square_index), *piece);
                ++file;
            }

            if (file > 8) {
                SetError(error,
                         "piece placement: rank has more than eight files");
                return std::nullopt;
            }
        }

        if (file != 8) {
            SetError(error, "piece placement: rank " +
                                std::to_string(8 - rank_index) +
                                " must contain eight files");
            return std::nullopt;
        }
    }

    int white_kings = 0;
    int black_kings = 0;
    for (int index = 0; index < 64; ++index) {
        const Piece piece =
            position.PieceAt(Square(static_cast<std::uint8_t>(index)));
        if (piece.Type() != PieceType::King) {
            continue;
        }
        if (piece.Color() == Color::White) {
            ++white_kings;
        } else {
            ++black_kings;
        }
    }
    if (white_kings != 1 || black_kings != 1) {
        SetError(error,
                 "piece placement: exactly one white king and one black king "
                 "are required");
        return std::nullopt;
    }

    if (fields[1] == "w") {
        position.SetSideToMove(Color::White);
    } else if (fields[1] == "b") {
        position.SetSideToMove(Color::Black);
    } else {
        SetError(error, "side to move: expected 'w' or 'b'");
        return std::nullopt;
    }

    chess::CastlingRights rights = kNoCastlingRights;
    if (fields[2] != "-") {
        for (const char value : fields[2]) {
            CastlingRight right;
            switch (value) {
                case 'K':
                    right = CastlingRight::WhiteKingSide;
                    break;
                case 'Q':
                    right = CastlingRight::WhiteQueenSide;
                    break;
                case 'k':
                    right = CastlingRight::BlackKingSide;
                    break;
                case 'q':
                    right = CastlingRight::BlackQueenSide;
                    break;
                default:
                    SetError(error,
                             "castling: expected '-' or a subset of KQkq");
                    return std::nullopt;
            }

            if (HasCastlingRight(rights, right)) {
                SetError(error, "castling: duplicate castling right");
                return std::nullopt;
            }
            rights = WithCastlingRight(rights, right);
        }
    }
    position.SetCastlingRights(rights);

    if (fields[3] != "-") {
        const std::optional<Square> square = Square::Parse(fields[3]);
        if (!square.has_value()) {
            SetError(error, "en passant: invalid target square");
            return std::nullopt;
        }
        if (square->Rank() != 2 && square->Rank() != 5) {
            SetError(error, "en passant: target must be on rank 3 or 6");
            return std::nullopt;
        }
        position.SetEnPassant(square);
    }

    int halfmove_clock = 0;
    if (!ParseNonNegativeInt(fields[4], &halfmove_clock)) {
        SetError(error, "halfmove clock: expected a non-negative integer");
        return std::nullopt;
    }
    position.SetHalfmoveClock(halfmove_clock);

    int fullmove_number = 0;
    if (!ParsePositiveInt(fields[5], &fullmove_number)) {
        SetError(error, "fullmove number: expected an integer of at least 1");
        return std::nullopt;
    }
    position.SetFullmoveNumber(fullmove_number);

    return position;
}

std::string Position::ToFen() const {
    std::string fen;
    fen.reserve(90);

    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            const auto index = static_cast<std::uint8_t>(rank * 8 + file);
            const Piece piece = PieceAt(Square(index));
            if (piece.IsEmpty()) {
                ++empty;
                continue;
            }
            if (empty > 0) {
                fen += static_cast<char>('0' + empty);
                empty = 0;
            }
            fen += piece.ToString();
        }
        if (empty > 0) {
            fen += static_cast<char>('0' + empty);
        }
        if (rank > 0) {
            fen += '/';
        }
    }

    fen += ' ';
    fen += side_to_move_ == Color::White ? 'w' : 'b';
    fen += ' ';

    std::string castling;
    if (HasCastlingRight(castling_rights_,
                         CastlingRight::WhiteKingSide)) {
        castling += 'K';
    }
    if (HasCastlingRight(castling_rights_,
                         CastlingRight::WhiteQueenSide)) {
        castling += 'Q';
    }
    if (HasCastlingRight(castling_rights_,
                         CastlingRight::BlackKingSide)) {
        castling += 'k';
    }
    if (HasCastlingRight(castling_rights_,
                         CastlingRight::BlackQueenSide)) {
        castling += 'q';
    }
    fen += castling.empty() ? "-" : castling;

    fen += ' ';
    fen += en_passant_.has_value() ? en_passant_->ToString() : "-";
    fen += ' ';
    fen += std::to_string(halfmove_clock_);
    fen += ' ';
    fen += std::to_string(fullmove_number_);
    return fen;
}

}  // namespace chess
