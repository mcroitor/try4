#include "chess/see.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

#include "chess/types.hpp"

namespace chess {
namespace {

constexpr int kMaxExchangeDepth = 32;

// 0x88 helpers local to the SEE; identical arithmetic to Position's private
// To0x88 (rank << 4 | file), which is intentionally not reused because the
// SEE translation unit is not a Position friend.
constexpr std::size_t Index0x88(std::size_t index) {
    return (index / 8) * 16 + index % 8;
}
constexpr bool OnBoard(int index) {
    return index >= 0 && index < 128 && (index & 0x88) == 0;
}

// Returns the 0x88 square index of `side`'s cheapest piece that currently
// attacks `target`, or -1. The scan order implements "cheapest first":
// pawns, knights, then diagonal and orthogonal rays (bishop/rook and queen as
// the ray's first blocker; rays stop at the first piece, so x-rays are picked
// up by the next call after a removal), and finally the king, which may step
// onto the target only while nothing of the opposing side still attacks it.
int CheapestAttacker(const std::array<Piece, 128>& board, int target,
                     Color side) {
    const Piece* at = board.data();

    // Pawns: a white pawn on target-15/-17 attacks the target; a black pawn
    // on target+15/+17.
    for (const int step : {-15, -17}) {
        const int from = side == Color::White ? target + step : target - step;
        if (OnBoard(from) && at[from].Type() == PieceType::Pawn &&
            at[from].Color() == side) {
            return from;
        }
    }

    for (const int offset : {-33, -31, -18, -14, 14, 18, 31, 33}) {
        const int from = target + offset;
        if (OnBoard(from) && at[from].Type() == PieceType::Knight &&
            at[from].Color() == side) {
            return from;
        }
    }

    for (const int step : {-17, -15, 15, 17}) {
        int from = target + step;
        while (OnBoard(from)) {
            const Piece piece = at[from];
            if (!piece.IsEmpty()) {
                if (piece.Color() == side &&
                    (piece.Type() == PieceType::Bishop ||
                     piece.Type() == PieceType::Queen)) {
                    return from;
                }
                break;
            }
            from += step;
        }
    }

    for (const int step : {-16, -1, 1, 16}) {
        int from = target + step;
        while (OnBoard(from)) {
            const Piece piece = at[from];
            if (!piece.IsEmpty()) {
                if (piece.Color() == side &&
                    (piece.Type() == PieceType::Rook ||
                     piece.Type() == PieceType::Queen)) {
                    return from;
                }
                break;
            }
            from += step;
        }
    }

    // The king attacks the target square only when nothing of the opponent
    // still attacks it; otherwise the king could never legally capture.
    for (const int step : {-17, -16, -15, -1, 1, 15, 16, 17}) {
        const int from = target + step;
        if (!OnBoard(from) || at[from].Type() != PieceType::King ||
            at[from].Color() != side) {
            continue;
        }
        const Color enemy = Opposite(side);
        bool enemy_attacks = false;

        // Opponent pawns: a white pawn attacks from target-15/-17; a black
        // pawn from target+15/+17.
        for (const int pawn_step : {-15, -17, 15, 17}) {
            const int pawn_from =
                enemy == Color::White ? target + pawn_step : target - pawn_step;
            if (OnBoard(pawn_from) &&
                at[pawn_from].Type() == PieceType::Pawn &&
                at[pawn_from].Color() == enemy) {
                enemy_attacks = true;
                break;
            }
        }
        // Opponent knights.
        for (const int offset : {-33, -31, -18, -14, 14, 18, 31, 33}) {
            const int knight_from = target + offset;
            if (OnBoard(knight_from) &&
                at[knight_from].Type() == PieceType::Knight &&
                at[knight_from].Color() == enemy) {
                enemy_attacks = true;
                break;
            }
        }
        // Opponent sliders: the first blocker of each ray decides which kind
        // of slider can attack this target.
        for (const int scan_step : {-17, -15, 15, 17, -16, -1, 1, 16}) {
            const bool diagonal = scan_step == -17 || scan_step == -15 ||
                                  scan_step == 15 || scan_step == 17;
            int scan = target + scan_step;
            while (OnBoard(scan)) {
                const Piece piece = at[scan];
                if (!piece.IsEmpty()) {
                    const PieceType type = piece.Type();
                    if (piece.Color() == enemy &&
                        (type == PieceType::Queen || type == PieceType::King ||
                         (type == PieceType::Bishop && diagonal) ||
                         (type == PieceType::Rook && !diagonal))) {
                        enemy_attacks = true;
                    }
                    break;
                }
                scan += scan_step;
            }
            if (enemy_attacks) {
                break;
            }
        }
        if (!enemy_attacks) {
            return from;
        }
    }
    return -1;
}

}  // namespace

int See(const Position& position, const Move& move,
        const EvaluationParameters& parameters) {
    if (!move.IsValid() || move.from == move.to) {
        return 0;
    }
    if (move.HasFlag(MoveFlag::Promotion) ||
        move.promotion != PieceType::None) {
        // Promotion captures have a different best-case shape; the caller
        // handles them (quiescence never prunes promotions).
        return 0;
    }
    const std::size_t target =
        Index0x88(static_cast<std::size_t>(move.to.Index()));
    const std::size_t origin =
        Index0x88(static_cast<std::size_t>(move.from.Index()));
    const Color mover = position.SideToMove();
    const bool en_passant = move.HasFlag(MoveFlag::EnPassant);

    std::array<Piece, 128> board{};
    for (std::size_t index = 0; index < 64; ++index) {
        board[Index0x88(index)] =
            position.PieceAt(Square(static_cast<std::uint8_t>(index)));
    }

    const Piece moving = board[origin];
    const Piece victim = board[target];
    if (moving.IsEmpty() || moving.Color() != mover) {
        return 0;  // Not the mover's piece.
    }
    if (!en_passant &&
        (victim.IsEmpty() || victim.Color() == mover)) {
        return 0;  // Not a capture.
    }

    int gain[kMaxExchangeDepth];
    int depth = 0;
    gain[0] = en_passant
                  ? PieceValue(PieceType::Pawn, parameters)
                  : PieceValue(victim.Type(), parameters);
    board[origin] = Piece{};
    if (en_passant) {
        // The captured pawn sits beside the target, one rank towards the
        // mover's origin: white EP captures remove target-16, black remove
        // target+16.
        const std::size_t captured =
            mover == Color::White ? target - 16 : target + 16;
        board[captured] = Piece{};
    }
    board[target] = moving;

    Color side = Opposite(mover);
    while (depth + 1 < kMaxExchangeDepth) {
        const int attacker =
            CheapestAttacker(board, static_cast<int>(target), side);
        if (attacker < 0) {
            break;
        }
        const Piece attacker_piece = board[static_cast<std::size_t>(attacker)];
        board[static_cast<std::size_t>(attacker)] = Piece{};
        ++depth;
        // The value of the piece standing on the target right now (the
        // previous capturer) minus the running total.
        gain[depth] = PieceValue(board[target].Type(), parameters) -
                      gain[depth - 1];
        board[target] = attacker_piece;
        side = Opposite(side);
    }

    // Backward minimax over the swap sequence: the mover at level d keeps
    // their capture only if gain[d] beats stopping (which values the previous
    // level's board at -gain[d-1]); the negation chain implements it.
    while (depth > 0) {
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
        --depth;
    }
    return gain[0];
}

}  // namespace chess