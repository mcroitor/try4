#ifndef CHESS_SYZYGY_HPP_
#define CHESS_SYZYGY_HPP_

#include <optional>
#include <string>

#include "chess/position.hpp"

namespace chess {

// Tablebase scores are deliberately below the mate band but above every
// static-evaluation score (which is clamped to +/-90000). The search treats
// them as ordinary centipawn values, so no false mate distance is reported.
inline constexpr int kSyzygyWinScore = 95000;
inline constexpr int kSyzygyCursedWinScore = 1;
inline constexpr int kSyzygyBlessedLossScore = -1;
inline constexpr int kSyzygyLossScore = -95000;

// Single source of truth for the WDL-to-engine-score mapping. `wdl` uses the
// usual side-to-move convention described below; unknown values are losses.
inline int SyzygyScoreFromWdl(int wdl) {
    switch (wdl) {
        case 2: return kSyzygyWinScore;
        case 1: return kSyzygyCursedWinScore;
        case 0: return 0;
        case -1: return kSyzygyBlessedLossScore;
        default: return kSyzygyLossScore;
    }
}

// One root-tablebase result. `wdl` uses the usual side-to-move convention:
// -2 loss, -1 blessed loss, 0 draw, +1 cursed win, +2 win. `dtz` is signed
// from the side to move. `scoreCp` is the value the engine reports.
struct SyzygyRootResult {
    Move bestMove;
    int wdl = 0;
    int dtz = 0;
    int scoreCp = 0;
};

// Thin RAII wrapper around Fathom's Syzygy prober. The tablebase files are
// external data and are never bundled. A wrapper owns the process-global
// Fathom state; create at most one ready instance at a time.
class SyzygyTablebase {
public:
    SyzygyTablebase();
    ~SyzygyTablebase();

    SyzygyTablebase(const SyzygyTablebase&) = delete;
    SyzygyTablebase& operator=(const SyzygyTablebase&) = delete;
    SyzygyTablebase(SyzygyTablebase&&) = delete;
    SyzygyTablebase& operator=(SyzygyTablebase&&) = delete;

    // Loads every readable tablebase under `path`. `path` may be a single
    // directory or a Fathom separator-separated list (`;` on Windows, `:` on
    // POSIX). Returns false and fills `error` when no tables were found.
    bool Load(const std::string& path, std::string* error);

    // Releases the mapped tables. Safe to call more than once.
    void Clear();

    bool Ready() const { return ready_; }
    unsigned MaxPieces() const;
    const std::string& Path() const { return path_; }

    // True when the position is inside the loaded tablebase range and can be
    // probed without castling rights. This does not apply the fifty-move rule.
    bool CanProbe(const Position& position) const;

    // Raw WDL probe for search. The caller must only use it when the
    // halfmove clock is zero; `ProbeRoot` handles nonzero clocks through DTZ.
    bool ProbeWdl(const Position& position, int* wdl) const;

    // Root probe. Uses Fathom's DTZ-aware root search with the position's
    // halfmove clock and the fifty-move rule. Returns nullopt when the material
    // is not present or probing fails.
    std::optional<SyzygyRootResult> ProbeRoot(const Position& position) const;

private:
    bool ready_ = false;
    bool initialized_ = false;
    std::string path_;
};

}  // namespace chess

#endif  // CHESS_SYZYGY_HPP_
