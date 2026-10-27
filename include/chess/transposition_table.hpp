#ifndef CHESS_TRANSPOSITION_TABLE_HPP_
#define CHESS_TRANSPOSITION_TABLE_HPP_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "chess/types.hpp"

namespace chess {

enum class TTBound : std::uint8_t {
    None = 0,
    Exact,
    Lower,
    Upper,
};

struct TTEntry {
    std::uint64_t key = 0;
    int depth = -1;
    int score = 0;
    TTBound bound = TTBound::None;
    Move bestMove;
};

// Mate scores are stored relative to the node instead of the root. The
// conversion is the standard ply adjustment and is exposed for regression
// tests.
int ScoreToTT(int score, int ply);
int ScoreFromTT(int score, int ply);

// Fixed-size, direct-mapped transposition table. It is owned by one Searcher;
// concurrent searches must use separate tables (threading is added later).
class TranspositionTable {
public:
    explicit TranspositionTable(std::size_t megabytes = 16);

    // Replaces all entries and allocates a power-of-two number of buckets.
    void Resize(std::size_t megabytes);

    // Keeps the allocation but clears every entry.
    void Clear();

    std::size_t Capacity() const { return entries_.size(); }
    std::size_t Megabytes() const { return megabytes_; }

    // Returns true only for a full-key match. `entry` is left unchanged on a
    // miss and must not be null.
    bool Probe(std::uint64_t key, TTEntry* entry) const;

    // Replaces the bucket for `key`. Advancing/aging replacement is deliberately
    // deferred; callers should clear or resize when reusing an engine.
    void Store(std::uint64_t key, int depth, int score, TTBound bound,
               Move bestMove);

private:
    static std::size_t BucketCountForMegabytes(std::size_t megabytes);

    std::vector<TTEntry> entries_;
    std::size_t mask_ = 0;
    std::size_t megabytes_ = 0;
};

}  // namespace chess

#endif  // CHESS_TRANSPOSITION_TABLE_HPP_
