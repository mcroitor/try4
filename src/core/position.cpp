#include "chess/position.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "chess/movegen.hpp"
#include "chess/zobrist.hpp"

namespace chess {

Position::Position() {
    Clear();
}

void Position::Clear() {
    board_.fill(Piece{});
    side_to_move_ = Color::White;
    castling_rights_ = kNoCastlingRights;
    en_passant_ = std::nullopt;
    halfmove_clock_ = 0;
    fullmove_number_ = 1;
    history_.clear();
    hash_ = ComputeHash();
}

std::optional<Square> Position::From0x88(int index) {
    if (!IsOnBoard(index)) {
        return std::nullopt;
    }
    const auto square_index = static_cast<std::uint8_t>(
        ((index >> 4) * 8) + (index & 7));
    return Square(square_index);
}

void Position::SetupInitial() {
    Clear();

    constexpr std::array<PieceType, 8> kBackRank = {
        PieceType::Rook,
        PieceType::Knight,
        PieceType::Bishop,
        PieceType::Queen,
        PieceType::King,
        PieceType::Bishop,
        PieceType::Knight,
        PieceType::Rook,
    };

    for (int file = 0; file < 8; ++file) {
        const auto white_back = static_cast<std::uint8_t>(file);
        const auto white_pawn = static_cast<std::uint8_t>(8 + file);
        const auto black_pawn = static_cast<std::uint8_t>(48 + file);
        const auto black_back = static_cast<std::uint8_t>(56 + file);

        SetPiece(Square(white_back), Piece(Color::White, kBackRank[file]));
        SetPiece(Square(white_pawn),
                 Piece(Color::White, PieceType::Pawn));
        SetPiece(Square(black_pawn),
                 Piece(Color::Black, PieceType::Pawn));
        SetPiece(Square(black_back), Piece(Color::Black, kBackRank[file]));
    }

    side_to_move_ = Color::White;
    castling_rights_ = kAllCastlingRights;
    en_passant_ = std::nullopt;
    halfmove_clock_ = 0;
    fullmove_number_ = 1;
    hash_ = ComputeHash();
}

Piece Position::PieceAt(Square square) const {
    if (!square.IsValid()) {
        return Piece{};
    }
    return board_[static_cast<std::size_t>(To0x88(square))];
}

bool Position::SetPiece(Square square, Piece piece) {
    if (!square.IsValid()) {
        return false;
    }
    board_[static_cast<std::size_t>(To0x88(square))] = piece;
    hash_ = ComputeHash();
    return true;
}

std::optional<Square> Position::KingSquare(Color color) const {
    for (int index = 0; index < 64; ++index) {
        const Square square(static_cast<std::uint8_t>(index));
        const Piece piece = PieceAt(square);
        if (piece.Type() == PieceType::King && piece.Color() == color) {
            return square;
        }
    }
    return std::nullopt;
}

void Position::SetSideToMove(Color side) {
    side_to_move_ = side;
    hash_ = ComputeHash();
}

void Position::SetCastlingRights(chess::CastlingRights rights) {
    castling_rights_ = static_cast<chess::CastlingRights>(
        rights & kAllCastlingRights);
    hash_ = ComputeHash();
}

void Position::SetEnPassant(std::optional<Square> square) {
    if (square.has_value() && !square->IsValid()) {
        return;
    }
    en_passant_ = square;
    hash_ = ComputeHash();
}

bool Position::CastlingRightsConsistent() const {
    const auto home_piece = [this](CastlingRight right,
                                   std::uint8_t index, Color color,
                                   PieceType type) {
        if (!HasCastlingRight(castling_rights_, right)) {
            return true;
        }
        const Piece piece = PieceAt(Square(index));
        return !piece.IsEmpty() && piece.Color() == color &&
               piece.Type() == type;
    };

    return home_piece(CastlingRight::WhiteKingSide, 4, Color::White,
                      PieceType::King) &&
           home_piece(CastlingRight::WhiteKingSide, 7, Color::White,
                      PieceType::Rook) &&
           home_piece(CastlingRight::WhiteQueenSide, 4, Color::White,
                      PieceType::King) &&
           home_piece(CastlingRight::WhiteQueenSide, 0, Color::White,
                      PieceType::Rook) &&
           home_piece(CastlingRight::BlackKingSide, 60, Color::Black,
                      PieceType::King) &&
           home_piece(CastlingRight::BlackKingSide, 63, Color::Black,
                      PieceType::Rook) &&
           home_piece(CastlingRight::BlackQueenSide, 60, Color::Black,
                      PieceType::King) &&
           home_piece(CastlingRight::BlackQueenSide, 56, Color::Black,
                      PieceType::Rook);
}

std::optional<int> Position::HashEnPassantFile() const {
    if (!en_passant_.has_value() || !en_passant_->IsValid()) {
        return std::nullopt;
    }

    const int target = To0x88(*en_passant_);
    if (!IsOnBoard(target) ||
        !board_[static_cast<std::size_t>(target)].IsEmpty()) {
        return std::nullopt;
    }

    const int direction = side_to_move_ == Color::White ? 16 : -16;
    const int captured = target - direction;
    if (!IsOnBoard(captured)) {
        return std::nullopt;
    }
    const Piece captured_piece = board_[static_cast<std::size_t>(captured)];
    if (captured_piece.IsEmpty() || captured_piece.Color() == side_to_move_ ||
        captured_piece.Type() != PieceType::Pawn) {
        return std::nullopt;
    }

    for (const int file_delta : {-1, 1}) {
        const int source = target - direction + file_delta;
        if (!IsOnBoard(source)) {
            continue;
        }
        const Piece pawn = board_[static_cast<std::size_t>(source)];
        if (!pawn.IsEmpty() && pawn.Color() == side_to_move_ &&
            pawn.Type() == PieceType::Pawn) {
            return en_passant_->File();
        }
    }
    return std::nullopt;
}

std::uint64_t Position::ComputeHash() const {
    std::uint64_t hash = zobrist::CastlingKey(castling_rights_);
    if (side_to_move_ == Color::Black) {
        hash ^= zobrist::SideKey();
    }

    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            const int index = (rank << 4) | file;
            const Piece piece = board_[static_cast<std::size_t>(index)];
            if (piece.IsEmpty()) {
                continue;
            }
            const auto square = static_cast<std::uint8_t>(rank * 8 + file);
            hash ^= zobrist::PieceKey(piece.Color(), piece.Type(),
                                      Square(square));
        }
    }

    if (const std::optional<int> file = HashEnPassantFile();
        file.has_value()) {
        hash ^= zobrist::EnPassantKey(*file);
    }
    return hash;
}

bool Position::IsRepetition() const {
    std::size_t occurrences = 1;
    for (const HistoryEntry& entry : history_) {
        if (entry.hash == hash_ && ++occurrences >= 3) {
            return true;
        }
    }
    return false;
}

bool Position::IsThreefoldRepetition() const {
    return IsRepetition();
}

bool Position::HasInsufficientMaterial() const {
    int bishop_count = 0;
    int knight_count = 0;
    int white_bishops = 0;
    int black_bishops = 0;
    bool all_bishops_same_color = true;
    int bishop_square_color = -1;

    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            const int index = (rank << 4) | file;
            const Piece piece = board_[static_cast<std::size_t>(index)];
            if (piece.IsEmpty() || piece.Type() == PieceType::King) {
                continue;
            }

            switch (piece.Type()) {
                case PieceType::Bishop: {
                    ++bishop_count;
                    if (piece.Color() == Color::White) {
                        ++white_bishops;
                    } else {
                        ++black_bishops;
                    }
                    const int square_color = (rank + file) & 1;
                    if (bishop_square_color == -1) {
                        bishop_square_color = square_color;
                    } else if (bishop_square_color != square_color) {
                        all_bishops_same_color = false;
                    }
                    break;
                }
                case PieceType::Knight:
                    ++knight_count;
                    break;
                case PieceType::Pawn:
                case PieceType::Rook:
                case PieceType::Queen:
                    return false;
                case PieceType::None:
                case PieceType::King:
                    break;
            }
        }
    }

    if (bishop_count + knight_count <= 1) {
        return true;
    }
    return bishop_count == 2 && knight_count == 0 && white_bishops == 1 &&
           black_bishops == 1 && all_bishops_same_color;
}

void Position::SetHalfmoveClock(int value) {
    if (value >= 0) {
        halfmove_clock_ = value;
    }
}

void Position::SetFullmoveNumber(int value) {
    if (value >= 1) {
        fullmove_number_ = value;
    }
}

std::string Position::ToString() const {
    std::string result;
    result.reserve(72);

    for (int rank = 7; rank >= 0; --rank) {
        for (int file = 0; file < 8; ++file) {
            const auto index =
                static_cast<std::uint8_t>(rank * 8 + file);
            result += PieceAt(Square(index)).ToString();
        }
        result += '\n';
    }
    return result;
}

std::size_t Position::GenerateLegalMoves(MoveList* out) const {
    if (out == nullptr) {
        assert(false);
        return 0;
    }

    out->Clear();
    MoveList pseudo_legal;
    MoveGenerator::GeneratePseudoLegal(*this, &pseudo_legal);

    const Color moving_color = side_to_move_;
    const Color opponent = Opposite(moving_color);
    // King-square scanning is O(64) and it used to be called once per
    // candidate move. The king only moves for king moves, so it is enough to
    // compute it once per generation.
    const std::optional<Square> king_before = KingSquare(moving_color);
    if (!king_before.has_value()) {
        return out->Size();
    }
    Position* const mutable_this = const_cast<Position*>(this);

    for (const Move& move : pseudo_legal) {
        if (move.HasFlag(MoveFlag::CastleKing) ||
            move.HasFlag(MoveFlag::CastleQueen)) {
            if (king_before.has_value() && IsSquareAttacked(*king_before, opponent)) {
                continue;
            }

            const int intermediate_index =
                moving_color == Color::White
                    ? (move.HasFlag(MoveFlag::CastleKing) ? 5 : 3)
                    : (move.HasFlag(MoveFlag::CastleKing) ? 117 : 115);
            const std::optional<Square> intermediate =
                From0x88(intermediate_index);
            if (!intermediate.has_value() ||
                IsSquareAttacked(*intermediate, opponent)) {
                continue;
            }
        }

        const Piece moving_piece = board_[static_cast<std::size_t>(To0x88(move.from))];
        const Square king_after =
            moving_piece.Type() == PieceType::King ? move.to : *king_before;

        mutable_this->MakeMoveUnchecked(move);
        const bool safe = !IsSquareAttacked(king_after, opponent);
        const bool undone = mutable_this->UndoMove();
        assert(undone);
        (void)undone;
        if (safe) {
            out->Add(move);
        }
    }

    return out->Size();
}

std::vector<Move> Position::LegalMoves() {
    MoveList legal;
    GenerateLegalMoves(&legal);
    return std::vector<Move>(legal.begin(), legal.end());
}

std::vector<Move> Position::LegalMoves() const {
    MoveList legal;
    GenerateLegalMoves(&legal);
    return std::vector<Move>(legal.begin(), legal.end());
}

GameResult Position::Result() const {
    MoveList legal;
    GenerateLegalMoves(&legal);
    if (!legal.Empty()) {
        if (halfmove_clock_ >= 100) {
            return GameResult::DrawFiftyMove;
        }
        if (IsThreefoldRepetition()) {
            return GameResult::DrawRepetition;
        }
        if (HasInsufficientMaterial()) {
            return GameResult::DrawInsufficientMaterial;
        }
        return GameResult::Ongoing;
    }
    return InCheck(side_to_move_) ? GameResult::Checkmate
                                  : GameResult::Stalemate;
}

std::optional<Move> Position::FindLegalMove(const Move& move) const {
    MoveList legal;
    GenerateLegalMoves(&legal);
    for (const Move& candidate : legal) {
        if (candidate.from == move.from && candidate.to == move.to &&
            candidate.promotion == move.promotion) {
            return candidate;
        }
    }
    return std::nullopt;
}

bool Position::IsLegal(const Move& move) const {
    return FindLegalMove(move).has_value();
}

bool Position::MakeMove(const Move& move) {
    const std::optional<Move> legal = FindLegalMove(move);
    if (!legal.has_value()) {
        return false;
    }
    MakeMoveUnchecked(*legal);
    return true;
}

bool Position::MakeMoveAssumeLegal(const Move& move) {
    // The legality check is debug-only. Release builds rely on the documented
    // precondition to avoid repeating move generation on every search node.
#ifndef NDEBUG
    const std::optional<Move> legal = FindLegalMove(move);
    if (!legal.has_value() || *legal != move) {
        throw std::logic_error(
            "MakeMoveAssumeLegal requires a legal move from this position");
    }
#endif
    MakeMoveUnchecked(move);
    return true;
}

void Position::MakeMoveUnchecked(const Move& move) {
    assert(move.IsValid());
    const int from = To0x88(move.from);
    const int to = To0x88(move.to);
    const Piece moving_piece = board_[static_cast<std::size_t>(from)];
    assert(!moving_piece.IsEmpty());
    assert(moving_piece.Color() == side_to_move_);

    const int direction = moving_piece.Color() == Color::White ? 16 : -16;
    HistoryEntry entry;
    entry.move = move;
    entry.castling_rights = castling_rights_;
    entry.en_passant = en_passant_;
    entry.halfmove_clock = halfmove_clock_;
    entry.fullmove_number = fullmove_number_;
    entry.side_to_move = side_to_move_;
    entry.hash = hash_;

    if (move.HasFlag(MoveFlag::EnPassant)) {
        entry.captured =
            board_[static_cast<std::size_t>(to - direction)];
    } else if (move.HasFlag(MoveFlag::Capture)) {
        entry.captured = board_[static_cast<std::size_t>(to)];
    } else {
        entry.captured = Piece{};
    }
    history_.push_back(entry);

    const std::optional<int> old_en_passant_file = HashEnPassantFile();
    hash_ ^= zobrist::CastlingKey(castling_rights_);
    if (old_en_passant_file.has_value()) {
        hash_ ^= zobrist::EnPassantKey(*old_en_passant_file);
    }
    hash_ ^= zobrist::PieceKey(moving_piece.Color(), moving_piece.Type(),
                               move.from);
    if (!entry.captured.IsEmpty()) {
        const int captured_index =
            move.HasFlag(MoveFlag::EnPassant) ? to - direction : to;
        const std::optional<Square> captured_square =
            From0x88(captured_index);
        assert(captured_square.has_value());
        if (captured_square.has_value()) {
            hash_ ^= zobrist::PieceKey(entry.captured.Color(),
                                       entry.captured.Type(),
                                       *captured_square);
        }
    }

    en_passant_ = std::nullopt;

    if (move.HasFlag(MoveFlag::CastleKing) ||
        move.HasFlag(MoveFlag::CastleQueen)) {
        board_[static_cast<std::size_t>(from)] = Piece{};
        board_[static_cast<std::size_t>(to)] = moving_piece;

        if (moving_piece.Color() == Color::White) {
            if (move.HasFlag(MoveFlag::CastleKing)) {
                board_[7] = Piece{};
                board_[5] = Piece(Color::White, PieceType::Rook);
            } else {
                board_[0] = Piece{};
                board_[3] = Piece(Color::White, PieceType::Rook);
            }
        } else {
            if (move.HasFlag(MoveFlag::CastleKing)) {
                board_[119] = Piece{};
                board_[117] = Piece(Color::Black, PieceType::Rook);
            } else {
                board_[112] = Piece{};
                board_[115] = Piece(Color::Black, PieceType::Rook);
            }
        }
    } else {
        board_[static_cast<std::size_t>(from)] = Piece{};
        if (move.HasFlag(MoveFlag::EnPassant)) {
            board_[static_cast<std::size_t>(to - direction)] = Piece{};
        }
        board_[static_cast<std::size_t>(to)] =
            move.HasFlag(MoveFlag::Promotion)
                ? Piece(moving_piece.Color(), move.promotion)
                : moving_piece;
    }

    if (move.HasFlag(MoveFlag::CastleKing) ||
        move.HasFlag(MoveFlag::CastleQueen)) {
        hash_ ^= zobrist::PieceKey(moving_piece.Color(), PieceType::King,
                                   move.to);
        const bool king_side = move.HasFlag(MoveFlag::CastleKing);
        const auto rook_from = static_cast<std::uint8_t>(
            moving_piece.Color() == Color::White
                ? (king_side ? 7 : 0)
                : (king_side ? 63 : 56));
        const auto rook_to = static_cast<std::uint8_t>(
            moving_piece.Color() == Color::White
                ? (king_side ? 5 : 3)
                : (king_side ? 61 : 59));
        hash_ ^= zobrist::PieceKey(moving_piece.Color(), PieceType::Rook,
                                   Square(rook_from));
        hash_ ^= zobrist::PieceKey(moving_piece.Color(), PieceType::Rook,
                                   Square(rook_to));
    } else {
        const Piece placed =
            move.HasFlag(MoveFlag::Promotion)
                ? Piece(moving_piece.Color(), move.promotion)
                : moving_piece;
        hash_ ^= zobrist::PieceKey(placed.Color(), placed.Type(), move.to);
    }

    const auto remove_right = [this](CastlingRight right) {
        castling_rights_ =
            WithoutCastlingRight(castling_rights_, right);
    };

    if (moving_piece.Type() == PieceType::King) {
        if (moving_piece.Color() == Color::White) {
            remove_right(CastlingRight::WhiteKingSide);
            remove_right(CastlingRight::WhiteQueenSide);
        } else {
            remove_right(CastlingRight::BlackKingSide);
            remove_right(CastlingRight::BlackQueenSide);
        }
    }

    if (moving_piece.Type() == PieceType::Rook) {
        if (moving_piece.Color() == Color::White) {
            if (from == 0) {
                remove_right(CastlingRight::WhiteQueenSide);
            } else if (from == 7) {
                remove_right(CastlingRight::WhiteKingSide);
            }
        } else {
            if (from == 112) {
                remove_right(CastlingRight::BlackQueenSide);
            } else if (from == 119) {
                remove_right(CastlingRight::BlackKingSide);
            }
        }
    }

    if (entry.captured.Type() == PieceType::Rook) {
        if (entry.captured.Color() == Color::White) {
            if (to == 0) {
                remove_right(CastlingRight::WhiteQueenSide);
            } else if (to == 7) {
                remove_right(CastlingRight::WhiteKingSide);
            }
        } else {
            if (to == 112) {
                remove_right(CastlingRight::BlackQueenSide);
            } else if (to == 119) {
                remove_right(CastlingRight::BlackKingSide);
            }
        }
    }

    if (move.HasFlag(MoveFlag::DoublePush)) {
        const std::optional<Square> target = From0x88((from + to) / 2);
        if (target.has_value()) {
            en_passant_ = target;
        }
    }

    hash_ ^= zobrist::CastlingKey(castling_rights_);

    if (moving_piece.Type() == PieceType::Pawn ||
        move.HasFlag(MoveFlag::Capture) || !entry.captured.IsEmpty()) {
        halfmove_clock_ = 0;
    } else {
        ++halfmove_clock_;
    }

    if (side_to_move_ == Color::Black) {
        ++fullmove_number_;
    }
    side_to_move_ = Opposite(side_to_move_);
    hash_ ^= zobrist::SideKey();
    if (const std::optional<int> new_en_passant_file =
            HashEnPassantFile();
        new_en_passant_file.has_value()) {
        hash_ ^= zobrist::EnPassantKey(*new_en_passant_file);
    }
}

void Position::ReserveHistory(std::size_t capacity) {
    history_.reserve(capacity);
}

bool Position::UndoMove() {
    if (history_.empty()) {
        return false;
    }

    const HistoryEntry entry = history_.back();
    history_.pop_back();

    const int from = To0x88(entry.move.from);
    const int to = To0x88(entry.move.to);
    const Color moving_color = entry.side_to_move;
    const int direction = moving_color == Color::White ? 16 : -16;

    if (entry.move.HasFlag(MoveFlag::CastleKing) ||
        entry.move.HasFlag(MoveFlag::CastleQueen)) {
        board_[static_cast<std::size_t>(from)] =
            board_[static_cast<std::size_t>(to)];
        board_[static_cast<std::size_t>(to)] = Piece{};

        if (moving_color == Color::White) {
            if (entry.move.HasFlag(MoveFlag::CastleKing)) {
                board_[7] = board_[5];
                board_[5] = Piece{};
            } else {
                board_[0] = board_[3];
                board_[3] = Piece{};
            }
        } else {
            if (entry.move.HasFlag(MoveFlag::CastleKing)) {
                board_[119] = board_[117];
                board_[117] = Piece{};
            } else {
                board_[112] = board_[115];
                board_[115] = Piece{};
            }
        }
    } else {
        if (entry.move.HasFlag(MoveFlag::Promotion)) {
            board_[static_cast<std::size_t>(from)] =
                Piece(moving_color, PieceType::Pawn);
        } else {
            board_[static_cast<std::size_t>(from)] =
                board_[static_cast<std::size_t>(to)];
        }

        if (entry.move.HasFlag(MoveFlag::EnPassant)) {
            board_[static_cast<std::size_t>(to)] = Piece{};
            board_[static_cast<std::size_t>(to - direction)] =
                entry.captured;
        } else {
            board_[static_cast<std::size_t>(to)] = entry.captured;
        }
    }

    castling_rights_ = entry.castling_rights;
    en_passant_ = entry.en_passant;
    halfmove_clock_ = entry.halfmove_clock;
    fullmove_number_ = entry.fullmove_number;
    side_to_move_ = entry.side_to_move;
    hash_ = entry.hash;
    return true;
}

bool Position::operator==(const Position& other) const {
    return board_ == other.board_ && side_to_move_ == other.side_to_move_ &&
           castling_rights_ == other.castling_rights_ &&
           en_passant_ == other.en_passant_ &&
           halfmove_clock_ == other.halfmove_clock_ &&
           fullmove_number_ == other.fullmove_number_;
}

}  // namespace chess
