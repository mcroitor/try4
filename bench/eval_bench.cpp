#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "chess/evaluation.hpp"
#include "chess/position.hpp"

// Reference implementation of the pre-054 evaluator, copied verbatim from the
// main branch's src/core/evaluation.cpp. It uses the public Position API and
// validates parameters on every call, which is exactly what this benchmark
// measures the improvement against. Keeping the copy here makes the
// comparison apples-to-apples: same corpus, same process, same build flags.
namespace baseline {
namespace {

using chess::Color;
using chess::EvalWeight;
using chess::EvaluationParameters;
using chess::Piece;
using chess::PieceType;
using chess::Position;
using chess::Square;

struct LocalScore {
    int mg = 0;
    int eg = 0;
};

using Pst = std::array<std::array<int, 8>, 8>;  // [rank 1..8][file a..h]

constexpr Pst kZeroPst = {{
    {{0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0}},
}};

constexpr Pst kPawnMg = {{
    {{0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 5, 10, 10, 5, 0, 0}},
    {{5, 5, 10, 20, 20, 10, 5, 5}},
    {{10, 15, 20, 30, 30, 20, 15, 10}},
    {{20, 30, 40, 55, 55, 40, 30, 20}},
    {{50, 60, 70, 80, 80, 70, 60, 50}},
    {{0, 0, 0, 0, 0, 0, 0, 0}},
}};

constexpr Pst kPawnEg = {{
    {{0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0}},
    {{5, 5, 10, 15, 15, 10, 5, 5}},
    {{10, 10, 20, 30, 30, 20, 10, 10}},
    {{20, 25, 35, 45, 45, 35, 25, 20}},
    {{35, 45, 60, 75, 75, 60, 45, 35}},
    {{70, 85, 100, 115, 115, 100, 85, 70}},
    {{0, 0, 0, 0, 0, 0, 0, 0}},
}};

constexpr Pst kKnightMg = {{
    {{-50, -40, -30, -30, -30, -30, -40, -50}},
    {{-40, -20, 0, 0, 0, 0, -20, -40}},
    {{-30, 0, 10, 15, 15, 10, 0, -30}},
    {{-30, 5, 15, 20, 20, 15, 5, -30}},
    {{-30, 5, 15, 20, 20, 15, 5, -30}},
    {{-30, 0, 10, 15, 15, 10, 0, -30}},
    {{-40, -20, 0, 5, 5, 0, -20, -40}},
    {{-50, -40, -30, -30, -30, -30, -40, -50}},
}};

constexpr Pst kKnightEg = {{
    {{-50, -40, -30, -30, -30, -30, -40, -50}},
    {{-40, -20, 0, 5, 5, 0, -20, -40}},
    {{-30, 5, 10, 15, 15, 10, 5, -30}},
    {{-30, 10, 15, 20, 20, 15, 10, -30}},
    {{-30, 10, 15, 20, 20, 15, 10, -30}},
    {{-30, 5, 10, 15, 15, 10, 5, -30}},
    {{-40, -20, 0, 5, 5, 0, -20, -40}},
    {{-50, -40, -30, -30, -30, -30, -40, -50}},
}};

constexpr Pst kBishopMg = {{
    {{-20, -10, -10, -10, -10, -10, -10, -20}},
    {{-10, 0, 0, 0, 0, 0, 0, -10}},
    {{-10, 0, 5, 10, 10, 5, 0, -10}},
    {{-10, 5, 5, 10, 10, 5, 5, -10}},
    {{-10, 0, 10, 10, 10, 10, 0, -10}},
    {{-10, 10, 10, 10, 10, 10, 10, -10}},
    {{-10, 5, 0, 0, 0, 0, 5, -10}},
    {{-20, -10, -10, -10, -10, -10, -10, -20}},
}};

constexpr Pst kBishopEg = {{
    {{-20, -10, -10, -10, -10, -10, -10, -20}},
    {{-10, 0, 0, 5, 5, 0, 0, -10}},
    {{-10, 5, 5, 10, 10, 5, 5, -10}},
    {{-10, 0, 10, 15, 15, 10, 0, -10}},
    {{-10, 5, 10, 15, 15, 10, 5, -10}},
    {{-10, 5, 10, 10, 10, 10, 5, -10}},
    {{-10, 0, 5, 5, 5, 5, 0, -10}},
    {{-20, -10, -10, -10, -10, -10, -10, -20}},
}};

constexpr Pst kRookMg = {{
    {{0, 0, 0, 5, 5, 0, 0, 0}},
    {{-5, 0, 0, 0, 0, 0, 0, -5}},
    {{-5, 0, 0, 0, 0, 0, 0, -5}},
    {{-5, 0, 0, 0, 0, 0, 0, -5}},
    {{-5, 0, 0, 0, 0, 0, 0, -5}},
    {{-5, 0, 0, 0, 0, 0, 0, -5}},
    {{5, 10, 10, 10, 10, 10, 10, 5}},
    {{0, 0, 0, 5, 5, 0, 0, 0}},
}};

constexpr Pst kRookEg = {{
    {{0, 0, 5, 5, 5, 5, 0, 0}},
    {{0, 5, 5, 5, 5, 5, 5, 0}},
    {{0, 5, 5, 10, 10, 5, 5, 0}},
    {{0, 5, 10, 10, 10, 10, 5, 0}},
    {{0, 5, 10, 10, 10, 10, 5, 0}},
    {{0, 5, 5, 10, 10, 5, 5, 0}},
    {{5, 10, 10, 15, 15, 10, 10, 5}},
    {{0, 5, 5, 5, 5, 5, 5, 0}},
}};

constexpr Pst kQueenMg = {{
    {{-20, -10, -10, -5, -5, -10, -10, -20}},
    {{-10, 0, 0, 0, 0, 0, 0, -10}},
    {{-10, 0, 5, 5, 5, 5, 0, -10}},
    {{-5, 0, 5, 10, 10, 5, 0, -5}},
    {{-5, 0, 5, 10, 10, 5, 0, -5}},
    {{-10, 0, 5, 5, 5, 5, 0, -10}},
    {{-10, 0, 0, 0, 0, 0, 0, -10}},
    {{-20, -10, -10, -5, -5, -10, -10, -20}},
}};

constexpr Pst kQueenEg = {{
    {{-10, -5, -5, -5, -5, -5, -5, -10}},
    {{-5, 0, 0, 0, 0, 0, 0, -5}},
    {{-5, 0, 5, 10, 10, 5, 0, -5}},
    {{-5, 0, 10, 15, 15, 10, 0, -5}},
    {{-5, 0, 10, 15, 15, 10, 0, -5}},
    {{-5, 0, 5, 10, 10, 5, 0, -5}},
    {{-5, 0, 0, 0, 0, 0, 0, -5}},
    {{-10, -5, -5, -5, -5, -5, -5, -10}},
}};

constexpr Pst kKingMg = {{
    {{20, 30, 10, 0, 0, 10, 30, 20}},
    {{20, 20, 0, 0, 0, 0, 20, 20}},
    {{-10, -20, -20, -20, -20, -20, -20, -10}},
    {{-20, -30, -30, -40, -40, -30, -30, -20}},
    {{-30, -40, -40, -50, -50, -40, -40, -30}},
    {{-30, -40, -40, -50, -50, -40, -40, -30}},
    {{-30, -40, -40, -50, -50, -40, -40, -30}},
    {{-30, -40, -40, -50, -50, -40, -40, -30}},
}};

constexpr Pst kKingEg = {{
    {{-50, -30, -30, -30, -30, -30, -30, -50}},
    {{-30, -10, 0, 0, 0, 0, -10, -30}},
    {{-30, 0, 20, 30, 30, 20, 0, -30}},
    {{-30, 0, 30, 40, 40, 30, 0, -30}},
    {{-30, 0, 30, 40, 40, 30, 0, -30}},
    {{-30, 0, 20, 30, 30, 20, 0, -30}},
    {{-30, -10, 0, 0, 0, 0, -10, -30}},
    {{-50, -30, -30, -30, -30, -30, -30, -50}},
}};

int PstValue(const Pst& table, Square square, Color color) {
    const int rank = square.Rank();
    const int file = square.File();
    return table[color == Color::White ? rank : 7 - rank][file];
}

const Pst& MiddlegamePst(PieceType type) {
    switch (type) {
        case PieceType::Pawn:
            return kPawnMg;
        case PieceType::Knight:
            return kKnightMg;
        case PieceType::Bishop:
            return kBishopMg;
        case PieceType::Rook:
            return kRookMg;
        case PieceType::Queen:
            return kQueenMg;
        case PieceType::King:
            return kKingMg;
        case PieceType::None:
            break;
    }
    return kZeroPst;
}

const Pst& EndgamePst(PieceType type) {
    switch (type) {
        case PieceType::Pawn:
            return kPawnEg;
        case PieceType::Knight:
            return kKnightEg;
        case PieceType::Bishop:
            return kBishopEg;
        case PieceType::Rook:
            return kRookEg;
        case PieceType::Queen:
            return kQueenEg;
        case PieceType::King:
            return kKingEg;
        case PieceType::None:
            break;
    }
    return kZeroPst;
}

int PhaseWeight(PieceType type) {
    switch (type) {
        case PieceType::Knight:
        case PieceType::Bishop:
            return 1;
        case PieceType::Rook:
            return 2;
        case PieceType::Queen:
            return 4;
        case PieceType::Pawn:
        case PieceType::King:
        case PieceType::None:
            return 0;
    }
    return 0;
}

Square FindKing(const Position& position, Color color) {
    for (std::uint8_t index = 0; index < 64; ++index) {
        const Square square(index);
        const Piece piece = position.PieceAt(square);
        if (piece.Type() == PieceType::King && piece.Color() == color) {
            return square;
        }
    }
    return Square(Square::kInvalidIndex);
}

bool HasPawnAt(const Position& position, int rank, int file, Color color) {
    if (rank < 0 || rank > 7 || file < 0 || file > 7) {
        return false;
    }
    const Piece piece =
        position.PieceAt(Square(static_cast<std::uint8_t>(rank * 8 + file)));
    return piece.Type() == PieceType::Pawn && piece.Color() == color;
}

bool IsPassedPawn(const Position& position, Square square, Color color) {
    const Color enemy = chess::Opposite(color);
    const int direction = color == Color::White ? 1 : -1;
    const int file = square.File();
    const int first_file = std::max(0, file - 1);
    const int last_file = std::min(7, file + 1);

    for (int f = first_file; f <= last_file; ++f) {
        for (int rank = square.Rank() + direction; rank >= 0 && rank <= 7;
             rank += direction) {
            const Piece piece = position.PieceAt(
                Square(static_cast<std::uint8_t>(rank * 8 + f)));
            if (piece.Type() == PieceType::Pawn && piece.Color() == enemy) {
                return false;
            }
        }
    }
    return true;
}

LocalScore PawnStructure(const Position& position, Color color,
                         const EvaluationParameters& p) {
    LocalScore score;
    std::array<int, 8> files{};

    for (std::uint8_t index = 0; index < 64; ++index) {
        const Piece piece = position.PieceAt(Square(index));
        if (piece.Type() == PieceType::Pawn && piece.Color() == color) {
            ++files[Square(index).File()];
        }
    }
    for (int count : files) {
        if (count > 1) {
            score.mg -= (count - 1) * p[EvalWeight::DoubledPawnMg];
            score.eg -= (count - 1) * p[EvalWeight::DoubledPawnEg];
        }
    }

    for (std::uint8_t index = 0; index < 64; ++index) {
        const Square square(index);
        const Piece piece = position.PieceAt(square);
        if (piece.Type() != PieceType::Pawn || piece.Color() != color) {
            continue;
        }
        const int file = square.File();
        const bool isolated =
            (file == 0 || files[file - 1] == 0) &&
            (file == 7 || files[file + 1] == 0);
        if (isolated) {
            score.mg -= p[EvalWeight::IsolatedPawnMg];
            score.eg -= p[EvalWeight::IsolatedPawnEg];
        }
        if (IsPassedPawn(position, square, color)) {
            const int relative_rank =
                color == Color::White ? square.Rank() : 7 - square.Rank();
            score.mg += p.passed_mg[relative_rank];
            score.eg += p.passed_eg[relative_rank];
        }
    }
    return score;
}

int SlidingMobility(const Position& position, Square square, Color color,
                    PieceType type) {
    static constexpr int kDirections[8][2] = {
        {1, 0},  {-1, 0}, {0, 1},  {0, -1},
        {1, 1},  {1, -1}, {-1, 1}, {-1, -1},
    };
    const int first_direction = type == PieceType::Bishop ? 4 : 0;
    const int last_direction = type == PieceType::Rook ? 3 : 7;
    int moves = 0;
    for (int direction = first_direction; direction <= last_direction;
         ++direction) {
        int rank = square.Rank() + kDirections[direction][0];
        int file = square.File() + kDirections[direction][1];
        while (rank >= 0 && rank <= 7 && file >= 0 && file <= 7) {
            const Piece target = position.PieceAt(
                Square(static_cast<std::uint8_t>(rank * 8 + file)));
            if (target.IsEmpty()) {
                ++moves;
            } else {
                if (target.Color() != color) {
                    ++moves;
                }
                break;
            }
            rank += kDirections[direction][0];
            file += kDirections[direction][1];
        }
    }
    return moves;
}

LocalScore Mobility(const Position& position, Color color,
                    const EvaluationParameters& p) {
    int moves = 0;
    for (std::uint8_t index = 0; index < 64; ++index) {
        const Square square(index);
        const Piece piece = position.PieceAt(square);
        if (piece.Color() != color) {
            continue;
        }
        if (piece.Type() == PieceType::Bishop ||
            piece.Type() == PieceType::Rook ||
            piece.Type() == PieceType::Queen) {
            moves += SlidingMobility(position, square, color, piece.Type());
        }
    }
    return {moves * p[EvalWeight::MobilityMg], moves * p[EvalWeight::MobilityEg]};
}

LocalScore BishopPair(const Position& position, Color color,
                      const EvaluationParameters& p) {
    int bishops = 0;
    for (std::uint8_t index = 0; index < 64; ++index) {
        const Piece piece = position.PieceAt(Square(index));
        if (piece.Type() == PieceType::Bishop && piece.Color() == color) {
            ++bishops;
        }
    }
    if (bishops >= 2) {
        return {p[EvalWeight::BishopPairMg], p[EvalWeight::BishopPairEg]};
    }
    return {};
}

LocalScore RookFiles(const Position& position, Color color,
                     const EvaluationParameters& p) {
    LocalScore score;
    for (std::uint8_t index = 0; index < 64; ++index) {
        const Square square(index);
        const Piece piece = position.PieceAt(square);
        if (piece.Type() != PieceType::Rook || piece.Color() != color) {
            continue;
        }
        bool own_pawn = false;
        bool enemy_pawn = false;
        for (int rank = 0; rank < 8; ++rank) {
            const Piece target = position.PieceAt(
                Square(static_cast<std::uint8_t>(rank * 8 + square.File())));
            if (target.Type() != PieceType::Pawn) {
                continue;
            }
            if (target.Color() == color) {
                own_pawn = true;
            } else {
                enemy_pawn = true;
            }
        }
        if (!own_pawn) {
            score.mg += enemy_pawn ? p[EvalWeight::RookSemiOpenMg]
                                   : p[EvalWeight::RookOpenMg];
            score.eg += enemy_pawn ? p[EvalWeight::RookSemiOpenEg]
                                   : p[EvalWeight::RookOpenEg];
        }
    }
    return score;
}

bool SliderPathClear(const Position& position, Square from, Square to) {
    const int rank_step =
        (to.Rank() > from.Rank()) - (to.Rank() < from.Rank());
    const int file_step =
        (to.File() > from.File()) - (to.File() < from.File());
    int rank = from.Rank() + rank_step;
    int file = from.File() + file_step;
    while (rank != to.Rank() || file != to.File()) {
        if (!position
                 .PieceAt(Square(static_cast<std::uint8_t>(rank * 8 + file)))
                 .IsEmpty()) {
            return false;
        }
        rank += rank_step;
        file += file_step;
    }
    return true;
}

bool AttacksSquare(const Position& position, Square from, PieceType type,
                   Color color, Square target) {
    const int rank_delta = target.Rank() - from.Rank();
    const int file_delta = target.File() - from.File();
    const int abs_rank = std::abs(rank_delta);
    const int abs_file = std::abs(file_delta);

    switch (type) {
        case PieceType::Pawn:
            return rank_delta == (color == Color::White ? 1 : -1) &&
                   abs_file == 1;
        case PieceType::Knight:
            return (abs_rank == 2 && abs_file == 1) ||
                   (abs_rank == 1 && abs_file == 2);
        case PieceType::King:
            return abs_rank <= 1 && abs_file <= 1 &&
                   (abs_rank != 0 || abs_file != 0);
        case PieceType::Bishop:
            return abs_rank == abs_file && abs_rank != 0 &&
                   SliderPathClear(position, from, target);
        case PieceType::Rook:
            return (rank_delta == 0 || file_delta == 0) &&
                   (rank_delta != 0 || file_delta != 0) &&
                   SliderPathClear(position, from, target);
        case PieceType::Queen:
            return (abs_rank == abs_file || rank_delta == 0 ||
                    file_delta == 0) &&
                   (rank_delta != 0 || file_delta != 0) &&
                   SliderPathClear(position, from, target);
        case PieceType::None:
            return false;
    }
    return false;
}

int CountKingAttackers(const Position& position, Color defender) {
    const Square king = FindKing(position, defender);
    if (!king.IsValid()) {
        return 0;
    }
    const Color enemy = chess::Opposite(defender);
    int attackers = 0;
    for (std::uint8_t index = 0; index < 64; ++index) {
        const Square from(index);
        const Piece piece = position.PieceAt(from);
        if (piece.IsEmpty() || piece.Color() != enemy ||
            piece.Type() == PieceType::None) {
            continue;
        }
        bool attacks_zone = false;
        for (int rank_delta = -1; rank_delta <= 1 && !attacks_zone;
             ++rank_delta) {
            for (int file_delta = -1; file_delta <= 1 && !attacks_zone;
                 ++file_delta) {
                const int rank = king.Rank() + rank_delta;
                const int file = king.File() + file_delta;
                if (rank < 0 || rank > 7 || file < 0 || file > 7) {
                    continue;
                }
                const Square target(
                    static_cast<std::uint8_t>(rank * 8 + file));
                attacks_zone =
                    AttacksSquare(position, from, piece.Type(), enemy, target);
            }
        }
        if (attacks_zone) {
            ++attackers;
        }
    }
    return attackers;
}

LocalScore KingSafety(const Position& position, Color color,
                      const EvaluationParameters& p) {
    const Square king = FindKing(position, color);
    if (!king.IsValid()) {
        return {};
    }
    LocalScore score;
    const int relative_rank =
        color == Color::White ? king.Rank() : 7 - king.Rank();
    if (relative_rank <= 1) {
        const int direction = color == Color::White ? 1 : -1;
        const int first_file = std::max(0, king.File() - 1);
        const int last_file = std::min(7, king.File() + 1);
        for (int file = first_file; file <= last_file; ++file) {
            if (HasPawnAt(position, king.Rank() + direction, file, color)) {
                score.mg += p[EvalWeight::ShieldOneMg];
                score.eg += p[EvalWeight::ShieldOneEg];
            } else if (HasPawnAt(position, king.Rank() + 2 * direction, file,
                                 color)) {
                score.mg += p[EvalWeight::ShieldTwoMg];
                score.eg += p[EvalWeight::ShieldTwoEg];
            } else {
                score.mg -= p[EvalWeight::ShieldMissingMg];
                score.eg -= p[EvalWeight::ShieldMissingEg];
            }
        }
    }
    const int attackers = CountKingAttackers(position, color);
    score.mg -= attackers * p[EvalWeight::KingAttackerMg];
    score.eg -= attackers * p[EvalWeight::KingAttackerEg];
    return score;
}

LocalScore FromColorPerspective(LocalScore white, LocalScore black, int sign) {
    return {sign * (white.mg - black.mg), sign * (white.eg - black.eg)};
}

}  // namespace
}  // namespace baseline

// The pre-054 public entry point, including the per-call validation.
int BaselineEvaluateDetailed(const chess::Position& position,
                             const chess::EvaluationParameters& p) {
    if (!chess::ValidateEvaluation(p)) {
        return 0;
    }
    baseline::LocalScore white_base;
    baseline::LocalScore black_base;
    int phase = 0;
    constexpr int kMaxPhase = 24;

    for (std::uint8_t index = 0; index < 64; ++index) {
        const chess::Square square(index);
        const chess::Piece piece = position.PieceAt(square);
        if (piece.IsEmpty()) {
            continue;
        }

        const chess::PieceType type = piece.Type();
        const baseline::LocalScore value = {
            p.material_mg[static_cast<std::size_t>(type)] +
                baseline::PstValue(baseline::MiddlegamePst(type), square,
                                   piece.Color()) *
                    p[chess::EvalWeight::PstMgPercent] / 100,
            p.material_eg[static_cast<std::size_t>(type)] +
                baseline::PstValue(baseline::EndgamePst(type), square,
                                   piece.Color()) *
                    p[chess::EvalWeight::PstEgPercent] / 100,
        };
        if (piece.Color() == chess::Color::White) {
            white_base.mg += value.mg;
            white_base.eg += value.eg;
        } else {
            black_base.mg += value.mg;
            black_base.eg += value.eg;
        }
        phase += baseline::PhaseWeight(type);
    }

    const int sign = position.SideToMove() == chess::Color::White ? 1 : -1;
    const baseline::LocalScore base =
        baseline::FromColorPerspective(white_base, black_base, sign);
    const baseline::LocalScore pawn_structure = baseline::FromColorPerspective(
        baseline::PawnStructure(position, chess::Color::White, p),
        baseline::PawnStructure(position, chess::Color::Black, p), sign);
    const baseline::LocalScore mobility = baseline::FromColorPerspective(
        baseline::Mobility(position, chess::Color::White, p),
        baseline::Mobility(position, chess::Color::Black, p), sign);
    const baseline::LocalScore bishop_pair = baseline::FromColorPerspective(
        baseline::BishopPair(position, chess::Color::White, p),
        baseline::BishopPair(position, chess::Color::Black, p), sign);
    const baseline::LocalScore rook_files = baseline::FromColorPerspective(
        baseline::RookFiles(position, chess::Color::White, p),
        baseline::RookFiles(position, chess::Color::Black, p), sign);
    const baseline::LocalScore king_safety = baseline::FromColorPerspective(
        baseline::KingSafety(position, chess::Color::White, p),
        baseline::KingSafety(position, chess::Color::Black, p), sign);

    const int middlegame = base.mg + pawn_structure.mg + mobility.mg +
                           bishop_pair.mg + rook_files.mg + king_safety.mg;
    const int endgame = base.eg + pawn_structure.eg + mobility.eg +
                        bishop_pair.eg + rook_files.eg + king_safety.eg;
    const int capped_phase = std::min(phase, kMaxPhase);
    const int tempo = p[chess::EvalWeight::Tempo];
    const int score = (middlegame * capped_phase +
                       endgame * (kMaxPhase - capped_phase)) /
                          kMaxPhase +
                      tempo;
    return std::clamp(score, -chess::kMaxStaticScore, chess::kMaxStaticScore);
}

namespace {

// Same generator as tests/evaluation_equiv_tests.cpp so the benchmark corpus
// and the equality-gate corpus are identical.
std::vector<std::string> BuildCorpus(int positions) {
    constexpr std::uint64_t kSeed = 0x05400010ULL;
    constexpr int kMaxPliesPerGame = 300;
    std::mt19937_64 rng(kSeed);
    std::vector<std::string> corpus;
    corpus.reserve(static_cast<std::size_t>(positions) + 7);

    const char* fixtures[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3P4/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/P6k/8/8/8/8/6p1/K7 w - - 0 1",
        "8/2p5/3p4/KP5r/8/4p1k1/8/8 w - - 0 1",
        "8/8/8/R6k/8/8/5r1K/8 w - - 0 1",
        "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQ1RK1 b - - 5 4",
        "8/8/8/bqk5/8/3K4/8/4R3 w - - 0 1",
    };
    for (const char* fen : fixtures) {
        corpus.emplace_back(fen);
    }

    chess::Position position;
    position.SetupInitial();
    chess::MoveList moves;
    int plies = 0;
    while (static_cast<int>(corpus.size()) < positions + 7) {
        corpus.push_back(position.ToFen());
        position.GenerateLegalMoves(&moves);
        if (moves.Empty() || plies >= kMaxPliesPerGame) {
            position.SetupInitial();
            plies = 0;
            continue;
        }
        const chess::Move move = moves[rng() % moves.Size()];
        position.MakeMoveAssumeLegal(move);
        ++plies;
    }
    return corpus;
}

double Measure(std::vector<chess::Position>& positions,
               const chess::EvaluationParameters& parameters,
               bool use_fast, int repetitions, std::int64_t* checksum) {
    std::int64_t sink = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        for (chess::Position& position : positions) {
            sink += use_fast ? chess::EvaluateFast(position, parameters)
                             : BaselineEvaluateDetailed(position, parameters);
        }
    }
    const auto finish = std::chrono::steady_clock::now();
    *checksum = sink;
    return std::chrono::duration<double>(finish - start).count();
}

void PrintUsage(const char* program) {
    std::cerr << "usage: " << program << " [--positions N] [--reps N]\n";
}

}  // namespace

int main(int argc, char** argv) {
    int corpus_size = 10000;
    int repetitions = 30;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (index + 1 >= argc) {
            PrintUsage(argv[0]);
            return 2;
        }
        if (argument == "--positions") {
            corpus_size = std::atoi(argv[++index]);
        } else if (argument == "--reps") {
            repetitions = std::atoi(argv[++index]);
        } else {
            PrintUsage(argv[0]);
            return 2;
        }
        if (corpus_size <= 0 || repetitions <= 0) {
            PrintUsage(argv[0]);
            return 2;
        }
    }

    const chess::EvaluationParameters parameters;
    const std::vector<std::string> corpus = BuildCorpus(corpus_size);
    std::vector<chess::Position> positions;
    positions.reserve(corpus.size());
    for (const std::string& fen : corpus) {
        std::string error;
        const auto parsed = chess::Position::FromFen(fen, &error);
        if (!parsed.has_value()) {
            std::cerr << "corpus FEN rejected: " << fen << " (" << error
                      << ")\n";
            return 2;
        }
        positions.push_back(*parsed);
    }
    const std::size_t count = positions.size();

    // Equality sanity gate between the baseline and the fast path; a mismatch
    // would invalidate the throughput comparison.
    std::int64_t baseline_check = 0;
    std::int64_t fast_check = 0;
    for (const chess::Position& position : positions) {
        baseline_check += BaselineEvaluateDetailed(position, parameters);
        fast_check += chess::EvaluateFast(position, parameters);
    }
    if (baseline_check != fast_check) {
        std::cerr << "corpus mismatch: baseline=" << baseline_check
                  << " fast=" << fast_check << '\n';
        return 2;
    }

    double baseline_seconds = 0.0;
    std::int64_t baseline_sink = 0;
    double fast_seconds = 0.0;
    std::int64_t fast_sink = 0;
    for (int warmup = 0; warmup < 2; ++warmup) {
        Measure(positions, parameters, false, 2, &baseline_sink);
        Measure(positions, parameters, true, 2, &fast_sink);
    }
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        baseline_seconds +=
            Measure(positions, parameters, false, 1, &baseline_sink);
        fast_seconds += Measure(positions, parameters, true, 1, &fast_sink);
    }

    const double baseline_per_eval =
        baseline_seconds / (static_cast<double>(repetitions) *
                            static_cast<double>(count));
    const double fast_per_eval =
        fast_seconds / (static_cast<double>(repetitions) *
                        static_cast<double>(count));

    std::cout << "corpus=" << count << '\n';
    std::cout << "repetitions=" << repetitions << '\n';
    std::cout << "baseline_evals_per_second=" << (baseline_per_eval > 0.0
              ? std::to_string(static_cast<std::int64_t>(1.0 / baseline_per_eval))
              : "0") << '\n';
    std::cout << "fast_evals_per_second=" << (fast_per_eval > 0.0
              ? std::to_string(static_cast<std::int64_t>(1.0 / fast_per_eval))
              : "0") << '\n';
    std::cout << "baseline_ns_per_eval="
              << (baseline_per_eval * 1e9) << '\n';
    std::cout << "fast_ns_per_eval=" << (fast_per_eval * 1e9) << '\n';
    if (fast_per_eval > 0.0) {
        std::cout << "speedup=" << (baseline_per_eval / fast_per_eval)
                  << '\n';
    }
    std::cout << "checksum=" << baseline_sink << ',' << fast_sink << '\n';
    return 0;
}