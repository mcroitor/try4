#include "chess/syzygy.hpp"

#include <cstdint>
#include <string>

#include "tbprobe.h"

namespace chess {
namespace {

struct FathomPosition {
    std::uint64_t white = 0;
    std::uint64_t black = 0;
    std::uint64_t kings = 0;
    std::uint64_t queens = 0;
    std::uint64_t rooks = 0;
    std::uint64_t bishops = 0;
    std::uint64_t knights = 0;
    std::uint64_t pawns = 0;
    unsigned ep = 0;
};

FathomPosition BuildFathomPosition(const Position& position) {
    FathomPosition result;
    for (unsigned index = 0; index < 64; ++index) {
        const Piece piece = position.PieceAt(
            Square(static_cast<std::uint8_t>(index)));
        if (piece.IsEmpty()) {
            continue;
        }
        const std::uint64_t bit = std::uint64_t{1} << index;
        if (piece.Color() == Color::White) {
            result.white |= bit;
        } else {
            result.black |= bit;
        }
        switch (piece.Type()) {
            case PieceType::King: result.kings |= bit; break;
            case PieceType::Queen: result.queens |= bit; break;
            case PieceType::Rook: result.rooks |= bit; break;
            case PieceType::Bishop: result.bishops |= bit; break;
            case PieceType::Knight: result.knights |= bit; break;
            case PieceType::Pawn: result.pawns |= bit; break;
            case PieceType::None: break;
        }
    }
    if (position.EnPassant().has_value()) {
        result.ep = position.EnPassant()->Index();
    }
    return result;
}

std::optional<Move> FindLegalMove(const Position& position, unsigned from,
                                  unsigned to, unsigned promotes) {
    PieceType promotion = PieceType::None;
    switch (promotes) {
        case TB_PROMOTES_QUEEN: promotion = PieceType::Queen; break;
        case TB_PROMOTES_ROOK: promotion = PieceType::Rook; break;
        case TB_PROMOTES_BISHOP: promotion = PieceType::Bishop; break;
        case TB_PROMOTES_KNIGHT: promotion = PieceType::Knight; break;
        case TB_PROMOTES_NONE: break;
        default: return std::nullopt;
    }

    MoveList moves;
    position.GenerateLegalMoves(&moves);
    for (const Move& move : moves) {
        if (move.from.Index() == from && move.to.Index() == to &&
            move.promotion == promotion) {
            return move;
        }
    }
    return std::nullopt;
}

}  // namespace

SyzygyTablebase::SyzygyTablebase() = default;

SyzygyTablebase::~SyzygyTablebase() {
    Clear();
}

bool SyzygyTablebase::Load(const std::string& path, std::string* error) {
    if (error != nullptr) {
        error->clear();
    }
    Clear();
    if (path.empty() || path == "<empty>") {
        if (error != nullptr) {
            *error = "SyzygyPath is empty";
        }
        return false;
    }
    if (!tb_init(path.c_str())) {
        if (error != nullptr) {
            *error = "Fathom failed to initialize from " + path;
        }
        return false;
    }
    initialized_ = true;
    if (TB_LARGEST == 0) {
        if (error != nullptr) {
            *error = "no Syzygy tablebase files found in " + path;
        }
        Clear();
        return false;
    }
    path_ = path;
    ready_ = true;
    return true;
}

void SyzygyTablebase::Clear() {
    if (initialized_) {
        tb_free();
        initialized_ = false;
    }
    ready_ = false;
    path_.clear();
}

unsigned SyzygyTablebase::MaxPieces() const {
    return ready_ ? TB_LARGEST : 0;
}

bool SyzygyTablebase::CanProbe(const Position& position) const {
    if (!ready_ || position.CastlingRights() != 0) {
        return false;
    }

    unsigned pieces = 0;
    bool white_king = false;
    bool black_king = false;
    for (unsigned index = 0; index < 64; ++index) {
        const Piece piece = position.PieceAt(
            Square(static_cast<std::uint8_t>(index)));
        if (piece.IsEmpty()) {
            continue;
        }
        ++pieces;
        if (piece.Type() == PieceType::King) {
            if (piece.Color() == Color::White) {
                white_king = true;
            } else {
                black_king = true;
            }
        }
    }
    return pieces >= 3 && pieces <= MaxPieces() && white_king && black_king;
}

bool SyzygyTablebase::ProbeWdl(const Position& position, int* wdl) const {
    if (wdl == nullptr || !CanProbe(position)) {
        return false;
    }
    const FathomPosition bits = BuildFathomPosition(position);
    const unsigned result = tb_probe_wdl(
        bits.white, bits.black, bits.kings, bits.queens, bits.rooks,
        bits.bishops, bits.knights, bits.pawns, 0, 0, bits.ep,
        position.SideToMove() == Color::White);
    if (result == TB_RESULT_FAILED) {
        return false;
    }
    *wdl = static_cast<int>(result) - 2;
    return true;
}

std::optional<SyzygyRootResult> SyzygyTablebase::ProbeRoot(
    const Position& position) const {
    if (!CanProbe(position)) {
        return std::nullopt;
    }
    const FathomPosition bits = BuildFathomPosition(position);
    const unsigned result = tb_probe_root(
        bits.white, bits.black, bits.kings, bits.queens, bits.rooks,
        bits.bishops, bits.knights, bits.pawns,
        static_cast<unsigned>(position.HalfmoveClock()), 0, bits.ep,
        position.SideToMove() == Color::White, nullptr);
    if (result == TB_RESULT_FAILED || result == TB_RESULT_CHECKMATE ||
        result == TB_RESULT_STALEMATE) {
        return std::nullopt;
    }

    const int wdl = static_cast<int>(TB_GET_WDL(result)) - 2;
    const int dtz_abs = static_cast<int>(TB_GET_DTZ(result));
    const std::optional<Move> move = FindLegalMove(
        position, TB_GET_FROM(result), TB_GET_TO(result),
        TB_GET_PROMOTES(result));
    if (!move.has_value()) {
        return std::nullopt;
    }

    SyzygyRootResult output;
    output.bestMove = *move;
    output.wdl = wdl;
    output.dtz = wdl < 0 ? -dtz_abs : dtz_abs;
    output.scoreCp = SyzygyScoreFromWdl(wdl);
    return output;
}

}  // namespace chess
