#include "chess/evaluation.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>

#include "chess/types.hpp"

namespace chess {
namespace {

constexpr int kMaxPhase = 24;

using Pst = std::array<std::array<int, 8>, 8>;  // [rank 1..8][file a..h]

// The tables are written from White's perspective, with rank 1 first. Black
// pieces look up the vertically mirrored rank. Values are centipawns and are
// deliberately modest: material remains the dominant term.
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

struct EvalScore {
    int mg = 0;
    int eg = 0;
};

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
    const Color enemy = Opposite(color);
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

EvalScore PawnStructure(const Position& position, Color color, const EvaluationParameters& p) {
    EvalScore score;
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

EvalScore Mobility(const Position& position, Color color, const EvaluationParameters& p) {
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

EvalScore BishopPair(const Position& position, Color color, const EvaluationParameters& p) {
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

EvalScore RookFiles(const Position& position, Color color, const EvaluationParameters& p) {
    EvalScore score;
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
            score.mg += enemy_pawn ? p[EvalWeight::RookSemiOpenMg] : p[EvalWeight::RookOpenMg];
            score.eg += enemy_pawn ? p[EvalWeight::RookSemiOpenEg] : p[EvalWeight::RookOpenEg];
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
    const Color enemy = Opposite(defender);
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

EvalScore KingSafety(const Position& position, Color color, const EvaluationParameters& p) {
    const Square king = FindKing(position, color);
    if (!king.IsValid()) {
        return {};
    }
    EvalScore score;
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

EvalScore FromColorPerspective(EvalScore white, EvalScore black, int sign) {
    return {sign * (white.mg - black.mg), sign * (white.eg - black.eg)};
}

}  // namespace

bool ValidateEvaluation(const EvaluationParameters& p, std::string* error) {
    const auto bounded = [](const auto& values, int maximum) {
        return std::all_of(values.begin(), values.end(), [maximum](int v) { return v >= 0 && v <= maximum; });
    };
    const bool valid = bounded(p.material_mg, 10000) && bounded(p.material_eg, 10000) &&
        bounded(p.passed_mg, 1000) && bounded(p.passed_eg, 1000) && bounded(p.weights, 1000) &&
        p.material_mg[0] == 0 && p.material_mg[6] == 0 &&
        p.material_eg[0] == 0 && p.material_eg[6] == 0;
    if (error) { *error = valid ? "" : "evaluation values out of range or nonzero king/empty material"; }
    return valid;
}

int PieceValue(PieceType type, const EvaluationParameters& p) {
    const auto index = static_cast<std::size_t>(type);
    return index < p.material_mg.size() ? std::clamp(p.material_mg[index], 0, 10000) : 0;
}

int EvaluateMaterial(const Position& position, const EvaluationParameters& p) {
    if (!ValidateEvaluation(p)) { return 0; }
    int white = 0;
    int black = 0;
    for (std::uint8_t index = 0; index < 64; ++index) {
        const Piece piece = position.PieceAt(Square(index));
        if (piece.IsEmpty()) {
            continue;
        }
        const int value = PieceValue(piece.Type(), p);
        if (piece.Color() == Color::White) {
            white += value;
        } else {
            black += value;
        }
    }
    const int score = white - black;
    return std::clamp(position.SideToMove() == Color::White ? score : -score,
                      -kMaxStaticScore, kMaxStaticScore);
}

EvaluationBreakdown EvaluateDetailed(const Position& position, const EvaluationParameters& p) {
    if (!ValidateEvaluation(p)) { return {}; }
    EvalScore white_base;
    EvalScore black_base;
    int phase = 0;

    for (std::uint8_t index = 0; index < 64; ++index) {
        const Square square(index);
        const Piece piece = position.PieceAt(square);
        if (piece.IsEmpty()) {
            continue;
        }

        const PieceType type = piece.Type();
        const EvalScore value = {
            p.material_mg[static_cast<std::size_t>(type)] +
                PstValue(MiddlegamePst(type), square, piece.Color()) * p[EvalWeight::PstMgPercent] / 100,
            p.material_eg[static_cast<std::size_t>(type)] +
                PstValue(EndgamePst(type), square, piece.Color()) * p[EvalWeight::PstEgPercent] / 100,
        };
        if (piece.Color() == Color::White) {
            white_base.mg += value.mg;
            white_base.eg += value.eg;
        } else {
            black_base.mg += value.mg;
            black_base.eg += value.eg;
        }
        phase += PhaseWeight(type);
    }

    const int sign = position.SideToMove() == Color::White ? 1 : -1;
    const EvalScore base = FromColorPerspective(white_base, black_base, sign);
    const EvalScore pawn_structure =
        FromColorPerspective(PawnStructure(position, Color::White, p),
                             PawnStructure(position, Color::Black, p), sign);
    const EvalScore mobility =
        FromColorPerspective(Mobility(position, Color::White, p),
                             Mobility(position, Color::Black, p), sign);
    const EvalScore bishop_pair =
        FromColorPerspective(BishopPair(position, Color::White, p),
                             BishopPair(position, Color::Black, p), sign);
    const EvalScore rook_files =
        FromColorPerspective(RookFiles(position, Color::White, p),
                             RookFiles(position, Color::Black, p), sign);
    const EvalScore king_safety =
        FromColorPerspective(KingSafety(position, Color::White, p),
                             KingSafety(position, Color::Black, p), sign);

    EvaluationBreakdown result;
    result.pawn_structure_mg = pawn_structure.mg;
    result.pawn_structure_eg = pawn_structure.eg;
    result.mobility_mg = mobility.mg;
    result.mobility_eg = mobility.eg;
    result.bishop_pair_mg = bishop_pair.mg;
    result.bishop_pair_eg = bishop_pair.eg;
    result.rook_files_mg = rook_files.mg;
    result.rook_files_eg = rook_files.eg;
    result.king_safety_mg = king_safety.mg;
    result.king_safety_eg = king_safety.eg;

    result.middlegame = base.mg + pawn_structure.mg + mobility.mg +
                        bishop_pair.mg + rook_files.mg + king_safety.mg;
    result.endgame = base.eg + pawn_structure.eg + mobility.eg +
                     bishop_pair.eg + rook_files.eg + king_safety.eg;
    result.phase = std::min(phase, kMaxPhase);
    result.tempo = p[EvalWeight::Tempo];
    result.score =
        (result.middlegame * result.phase +
         result.endgame * (kMaxPhase - result.phase)) /
            kMaxPhase +
        result.tempo;
    result.score = std::clamp(result.score, -kMaxStaticScore, kMaxStaticScore);
    return result;
}

int Evaluate(const Position& position, const EvaluationParameters& p) {
    return EvaluateDetailed(position, p).score;
}

}  // namespace chess
