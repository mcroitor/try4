#include "chess/transposition_table.hpp"

#include <algorithm>
#include <cassert>
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
    const std::size_t desired_buckets =
        std::max<std::size_t>(1, bytes / sizeof(TTBucket));

    std::size_t count = 1;
    while (count < desired_buckets) {
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
    entries_.assign(count, TTBucket{});
    mask_ = count - 1;
    megabytes_ = bounded;
    stats_ = TTStats{};
}

void TranspositionTable::Clear() {
    for (TTBucket& bucket : entries_) {
        for (TTEntry& entry : bucket.entries) {
            entry = TTEntry{};
        }
    }
    generation_ = 0;
    stats_ = TTStats{};
}

bool TranspositionTable::Probe(std::uint64_t key, TTEntry* entry) const {
    if (entry == nullptr || entries_.empty()) {
        return false;
    }
    const TTBucket& bucket =
        entries_[static_cast<std::size_t>(key) & mask_];
    for (const TTEntry& candidate : bucket.entries) {
        if (candidate.depth < 0 || candidate.key != key) {
            continue;
        }
        *entry = candidate;
        return true;
    }
    return false;
}

void TranspositionTable::Store(std::uint64_t key, int depth, int score,
                               TTBound bound, Move bestMove) {
    if (entries_.empty() || depth < 0) {
        return;
    }
    TTBucket& bucket = entries_[static_cast<std::size_t>(key) & mask_];

    const TTEntry incoming{
        key,
        depth,
        score,
        bound,
        bestMove,
        ++generation_,
    };

    // Two-way buckets with full-key match. Prefer to keep an entry if it is
    // deeper; otherwise use the shallower slot first and then the older slot.
    for (std::size_t slot = 0; slot < kEntriesPerBucket; ++slot) {
        TTEntry& candidate = bucket.entries[slot];
        if (candidate.depth < 0) {
            candidate = incoming;
            return;
        }
        if (candidate.key == key) {
            if (incoming.depth >= candidate.depth) {
                candidate = incoming;
                return;
            }
            break;
        }
    }

    std::size_t target = 0;
    if (bucket.entries[0].depth < 0) {
        target = 0;
    } else if (bucket.entries[1].depth < 0) {
        target = 1;
    } else if (bucket.entries[0].depth != bucket.entries[1].depth) {
        target =
            bucket.entries[0].depth < bucket.entries[1].depth ? 0 : 1;
    } else {
        target = bucket.entries[0].generation <= bucket.entries[1].generation
                     ? 0
                     : 1;
    }
    bucket.entries[target] = incoming;
}

}  // namespace chess
