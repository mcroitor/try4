#include "chess/transposition_table.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "chess/search.hpp"

namespace chess {
namespace {

constexpr std::size_t kMegabyte = 1024 * 1024;
constexpr std::size_t kMaximumMegabytes = 1024;

}  // namespace

int ScoreToTT(int score, int ply) {
    if (score >= kMateThreshold) {
        return score + ply;
    }
    if (score <= -kMateThreshold) {
        return score - ply;
    }
    return score;
}

int ScoreFromTT(int score, int ply) {
    if (score >= kMateThreshold) {
        return score - ply;
    }
    if (score <= -kMateThreshold) {
        return score + ply;
    }
    return score;
}

std::size_t TranspositionTable::BucketCountForMegabytes(
    std::size_t megabytes) {
    const std::size_t bounded = std::min(megabytes, kMaximumMegabytes);
    const std::size_t bytes = std::max<std::size_t>(1, bounded) * kMegabyte;
    const std::size_t desired =
        std::max<std::size_t>(1, bytes / sizeof(TTEntry));

    std::size_t count = 1;
    while (count < desired && count <= (desired / 2)) {
        count <<= 1;
    }
    return count;
}

TranspositionTable::TranspositionTable(std::size_t megabytes) {
    Resize(megabytes);
}

void TranspositionTable::Resize(std::size_t megabytes) {
    const std::size_t bounded = std::min(megabytes, kMaximumMegabytes);
    const std::size_t count = BucketCountForMegabytes(bounded);
    entries_.assign(count, TTEntry{});
    mask_ = count - 1;
    megabytes_ = bounded;
}

void TranspositionTable::Clear() {
    std::fill(entries_.begin(), entries_.end(), TTEntry{});
}

bool TranspositionTable::Probe(std::uint64_t key, TTEntry* entry) const {
    if (entry == nullptr || entries_.empty()) {
        return false;
    }
    const TTEntry& candidate =
        entries_[static_cast<std::size_t>(key) & mask_];
    if (candidate.depth < 0 || candidate.key != key) {
        return false;
    }
    *entry = candidate;
    return true;
}

void TranspositionTable::Store(std::uint64_t key, int depth, int score,
                               TTBound bound, Move bestMove) {
    if (entries_.empty() || depth < 0) {
        return;
    }
    TTEntry& entry = entries_[static_cast<std::size_t>(key) & mask_];
    entry.key = key;
    entry.depth = depth;
    entry.score = score;
    entry.bound = bound;
    entry.bestMove = bestMove;
}

}  // namespace chess
