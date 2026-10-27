#include "chess/movegen.hpp"

#include <cassert>
#include <optional>

#include "chess/position.hpp"

namespace chess {

bool MoveGenerator::IsSquareAttacked(const Position& position, Square square,
                                     Color by_color) {
    if (!square.IsValid()) {
        return false;
    }

    const int target = Position::To0x88(square);

    const auto piece_at = [&position](int index) -> Piece {
        if (!Position::IsOnBoard(index)) {
            return Piece{};
        }
        return position.board_[static_cast<std::size_t>(index)];
    };

    const auto attacks = [&piece_at, by_color](int index, PieceType type) {
        const Piece piece = piece_at(index);
        return !piece.IsEmpty() && piece.Color() == by_color &&
               piece.Type() == type;
    };

    if (by_color == Color::White) {
        if (attacks(target - 17, PieceType::Pawn) ||
            attacks(target - 15, PieceType::Pawn)) {
            return true;
        }
    } else {
        if (attacks(target + 17, PieceType::Pawn) ||
            attacks(target + 15, PieceType::Pawn)) {
            return true;
        }
    }

    constexpr int kKnightOffsets[] = {33, 31, 18, 14, -14, -18, -31, -33};
    for (const int offset : kKnightOffsets) {
        if (attacks(target + offset, PieceType::Knight)) {
            return true;
        }
    }

    constexpr int kKingOffsets[] = {1, -1, 16, -16, 17, 15, -17, -15};
    for (const int offset : kKingOffsets) {
        if (attacks(target + offset, PieceType::King)) {
            return true;
        }
    }

    constexpr int kDiagonalOffsets[] = {17, 15, -17, -15};
    for (const int offset : kDiagonalOffsets) {
        int index = target + offset;
        while (Position::IsOnBoard(index)) {
            const Piece piece = piece_at(index);
            if (!piece.IsEmpty()) {
                if (piece.Color() == by_color &&
                    (piece.Type() == PieceType::Bishop ||
                     piece.Type() == PieceType::Queen)) {
                    return true;
                }
                break;
            }
            index += offset;
        }
    }

    constexpr int kOrthogonalOffsets[] = {16, -16, 1, -1};
    for (const int offset : kOrthogonalOffsets) {
        int index = target + offset;
        while (Position::IsOnBoard(index)) {
            const Piece piece = piece_at(index);
            if (!piece.IsEmpty()) {
                if (piece.Color() == by_color &&
                    (piece.Type() == PieceType::Rook ||
                     piece.Type() == PieceType::Queen)) {
                    return true;
                }
                break;
            }
            index += offset;
        }
    }

    return false;
}

void MoveGenerator::GeneratePseudoLegal(const Position& position,
                                        MoveList* moves) {
    assert(moves != nullptr);
    moves->Clear();

    const Color side = position.SideToMove();

    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            const int from = (rank << 4) | file;
            const Piece piece =
                position.board_[static_cast<std::size_t>(from)];
            if (piece.IsEmpty() || piece.Color() != side) {
                continue;
            }

            switch (piece.Type()) {
                case PieceType::Pawn:
                    GeneratePawnMoves(position, from, side, moves);
                    break;
                case PieceType::Knight:
                    GenerateKnightMoves(position, from, side, moves);
                    break;
                case PieceType::Bishop: {
                    constexpr int kDiagonals[] = {17, 15, -17, -15};
                    GenerateSlidingMoves(position, from, side, moves,
                                         kDiagonals, 4);
                    break;
                }
                case PieceType::Rook: {
                    constexpr int kOrthogonals[] = {16, -16, 1, -1};
                    GenerateSlidingMoves(position, from, side, moves,
                                         kOrthogonals, 4);
                    break;
                }
                case PieceType::Queen: {
                    constexpr int kDiagonals[] = {17, 15, -17, -15};
                    constexpr int kOrthogonals[] = {16, -16, 1, -1};
                    GenerateSlidingMoves(position, from, side, moves,
                                         kDiagonals, 4);
                    GenerateSlidingMoves(position, from, side, moves,
                                         kOrthogonals, 4);
                    break;
                }
                case PieceType::King:
                    GenerateKingMoves(position, from, side, moves);
                    break;
                case PieceType::None:
                    break;
            }
        }
    }
}

void MoveGenerator::GeneratePawnMoves(const Position& position, int from,
                                      Color color, MoveList* moves) {
    const int direction = color == Color::White ? 16 : -16;
    const int start_rank = color == Color::White ? 1 : 6;
    const int promotion_rank = color == Color::White ? 7 : 0;
    const int from_rank = from >> 4;

    const int one_step = from + direction;
    if (Position::IsOnBoard(one_step) &&
        position.board_[static_cast<std::size_t>(one_step)].IsEmpty()) {
        if ((one_step >> 4) == promotion_rank) {
            AddPromotions(moves, from, one_step, MoveFlag::None);
        } else {
            AddMove(moves, from, one_step, MoveFlag::None);
            if (from_rank == start_rank) {
                const int two_step = one_step + direction;
                if (Position::IsOnBoard(two_step) &&
                    position.board_[static_cast<std::size_t>(two_step)]
                        .IsEmpty()) {
                    AddMove(moves, from, two_step, MoveFlag::DoublePush);
                }
            }
        }
    }

    constexpr int kCaptureAdjustments[] = {-1, 1};
    for (const int adjustment : kCaptureAdjustments) {
        const int to = from + direction + adjustment;
        if (!Position::IsOnBoard(to)) {
            continue;
        }

        const Piece target =
            position.board_[static_cast<std::size_t>(to)];
        if (target.IsEmpty()) {
            if (position.en_passant_.has_value() &&
                Position::To0x88(*position.en_passant_) == to) {
                const int captured = to - direction;
                if (Position::IsOnBoard(captured)) {
                    const Piece captured_piece =
                        position.board_[static_cast<std::size_t>(captured)];
                    if (!captured_piece.IsEmpty() &&
                        captured_piece.Color() != color &&
                        captured_piece.Type() == PieceType::Pawn) {
                        AddMove(moves, from, to,
                                MoveFlag::Capture | MoveFlag::EnPassant);
                    }
                }
            }
            continue;
        }
        if (target.Color() != color) {
            if ((to >> 4) == promotion_rank) {
                AddPromotions(moves, from, to, MoveFlag::Capture);
            } else {
                AddMove(moves, from, to, MoveFlag::Capture);
            }
        }
    }
}

void MoveGenerator::GenerateKnightMoves(const Position& position, int from,
                                        Color color, MoveList* moves) {
    constexpr int kOffsets[] = {33, 31, 18, 14, -14, -18, -31, -33};

    for (const int offset : kOffsets) {
        const int to = from + offset;
        if (!Position::IsOnBoard(to)) {
            continue;
        }

        const Piece target =
            position.board_[static_cast<std::size_t>(to)];
        if (target.IsEmpty()) {
            AddMove(moves, from, to, MoveFlag::None);
        } else if (target.Color() != color) {
            AddMove(moves, from, to, MoveFlag::Capture);
        }
    }
}

void MoveGenerator::GenerateKingMoves(const Position& position, int from,
                                      Color color, MoveList* moves) {
    constexpr int kOffsets[] = {1, -1, 16, -16, 17, 15, -17, -15};

    for (const int offset : kOffsets) {
        const int to = from + offset;
        if (!Position::IsOnBoard(to)) {
            continue;
        }

        const Piece target =
            position.board_[static_cast<std::size_t>(to)];
        if (target.IsEmpty()) {
            AddMove(moves, from, to, MoveFlag::None);
        } else if (target.Color() != color) {
            AddMove(moves, from, to, MoveFlag::Capture);
        }
    }

    const auto home_rook = [&position, color](int index) {
        const Piece piece =
            position.board_[static_cast<std::size_t>(index)];
        return !piece.IsEmpty() && piece.Color() == color &&
               piece.Type() == PieceType::Rook;
    };
    const auto empty_square = [&position](int index) {
        return position.board_[static_cast<std::size_t>(index)].IsEmpty();
    };

    // Check-related castling restrictions belong to legal-move filtering.
    // Missing kings or rooks are reported separately by
    // Position::CastlingRightsConsistent(); generation only requires the
    // home pieces and an empty path.
    if (color == Color::White && from == 4) {
        if (HasCastlingRight(position.castling_rights_,
                             CastlingRight::WhiteKingSide) &&
            home_rook(7) && empty_square(5) && empty_square(6)) {
            AddMove(moves, 4, 6, MoveFlag::CastleKing);
        }
        if (HasCastlingRight(position.castling_rights_,
                             CastlingRight::WhiteQueenSide) &&
            home_rook(0) && empty_square(1) && empty_square(2) &&
            empty_square(3)) {
            AddMove(moves, 4, 2, MoveFlag::CastleQueen);
        }
    } else if (color == Color::Black && from == 116) {
        if (HasCastlingRight(position.castling_rights_,
                             CastlingRight::BlackKingSide) &&
            home_rook(119) && empty_square(117) && empty_square(118)) {
            AddMove(moves, 116, 118, MoveFlag::CastleKing);
        }
        if (HasCastlingRight(position.castling_rights_,
                             CastlingRight::BlackQueenSide) &&
            home_rook(112) && empty_square(113) && empty_square(114) &&
            empty_square(115)) {
            AddMove(moves, 116, 114, MoveFlag::CastleQueen);
        }
    }
}

void MoveGenerator::GenerateSlidingMoves(const Position& position, int from,
                                         Color color, MoveList* moves,
                                         const int* offsets,
                                         std::size_t offset_count) {
    for (std::size_t i = 0; i < offset_count; ++i) {
        const int offset = offsets[i];
        int to = from + offset;
        while (Position::IsOnBoard(to)) {
            const Piece target =
                position.board_[static_cast<std::size_t>(to)];
            if (target.IsEmpty()) {
                AddMove(moves, from, to, MoveFlag::None);
            } else {
                if (target.Color() != color) {
                    AddMove(moves, from, to, MoveFlag::Capture);
                }
                break;
            }
            to += offset;
        }
    }
}

void MoveGenerator::AddPromotions(MoveList* moves, int from, int to,
                                  MoveFlag flags) {
    constexpr PieceType kPromotions[] = {
        PieceType::Knight,
        PieceType::Bishop,
        PieceType::Rook,
        PieceType::Queen,
    };

    for (const PieceType promotion : kPromotions) {
        AddMove(moves, from, to, flags | MoveFlag::Promotion, promotion);
    }
}

void MoveGenerator::AddMove(MoveList* moves, int from, int to, MoveFlag flags,
                            PieceType promotion) {
    const std::optional<Square> from_square = Position::From0x88(from);
    const std::optional<Square> to_square = Position::From0x88(to);
    if (!from_square.has_value() || !to_square.has_value()) {
        return;
    }
    moves->Add(Move(*from_square, *to_square, flags, promotion));
}

bool Position::IsSquareAttacked(Square square, Color by_color) const {
    return MoveGenerator::IsSquareAttacked(*this, square, by_color);
}

bool Position::InCheck(Color color) const {
    const std::optional<Square> king = KingSquare(color);
    if (!king.has_value()) {
        assert(false && "Position has no king");
        return false;
    }
    return IsSquareAttacked(*king, Opposite(color));
}

}  // namespace chess
