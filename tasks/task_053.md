# Task 053 - Improve transposition-table reuse without changing fixed-depth results

| Field | Value |
|---|---|
| **ID** | 053 |
| **Stage** | Search performance - transposition table |
| **Depends on** | 051 |
| **Estimate** | 0.5 day |
| **Priority** | P2 |
| **Status** | Done |

**Blocker:** task 051 is `Done`, so this task is now `Done` too.

## Objective

Increase transposition-table hit quality and reduce collisions with a replacement
policy that keeps deeper entries, while preserving the existing rule that the
reported PV always starts with a real root move.

## Background

`TranspositionTable` is direct-mapped and overwrites unconditionally
(`src/core/transposition_table.cpp:67-91`). The search probes only when
`ply > 0` (`src/core/search.cpp:315`), so the root move is never ordered by the
previous iteration's best move, and entries are stored only when a best line
exists (`search.cpp:374-384`). The default is 16 MB
(`include/chess/uci.hpp:27`, `include/chess/search.hpp:108`), which is small for
a 2 s/move search.

## In scope

- Two-way buckets (or an equivalent small cluster) with depth-preferred
  replacement and a generation/age counter.
- Root probing for move ordering only: no root cutoff, so `SearchInfo::pv`
  still begins with a searched root move.
- Optional `Hash` default review based on a measured A/B at 16/64 MB; change
  defaults only if the measurement supports it and update tests/docs with it.
- TT policy invalidation: new settings must continue to clear entries that were
  produced under a different evaluation/forcing/pruning policy.
- Counters for probes, hits and cutoffs, exposed only in debug/bench builds (or
  through a documented accessor) so hit-rate claims are measurable.

## Out of scope

- Lock-free or concurrent TT (task 048's decision governs that).
- Changing mate-score or halfmove-key semantics.
- Changing pruning or move ordering beyond the root hash move.

## Requirements

1. Fixed-depth scores, best moves and PVs must be unchanged on the task 051
   golden corpus.
2. Fixed-depth node counts must not increase; record the reduction on the 049
   benchmark positions.
3. Replacement is deterministic: with the same search sequence, the table ends
   in the same state.
4. Stored entries keep the current bound/depth rules; an entry from a shallower
   or differently configured search must not be used as exact.
5. Mate scores survive store/probe with the existing conversion.
6. Root probing must not leak a move from a different root or a stale policy.
7. The direct-mapped/2-way table must remain compatible with `Clear()` and
   `Resize()`; resize behavior stays bounded by the documented maximum.

## Acceptance matrix

| Case | Expected result |
|---|---|
| Golden corpus, fixed depth | Identical score, best move and PV |
| TT round-trip, collision and resize tests | Pass |
| Mate-score conversion | Pass |
| Policy change (evaluation/forcing/pruning settings) | Table cleared as before; no stale exact score |
| Node counts | No increase; measured reduction recorded |
| Determinism | Identical table state and result on repeat runs |
| Suites | `search`, `search_time`, `config`, `tactical` pass |

## Validation commands

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug -L "search|config|tactical" \
  --output-on-failure
python scripts/search_bench.py --engine build/performance/try4_uci.exe \
  --cli build/performance/try4.exe --depths 5,6,7 --output build/bench-after-053
```

## Risks and failure modes

- **Stale exact scores** after a policy change is the dangerous failure; keep
  the existing invalidation comparison exhaustive and test each new setting.
- **Root cutoffs** would let the engine print a PV it never searched; root
  probing is ordering-only for that reason.
- **Aging that hides a real regression** by favoring new entries; keep
  depth-preferred ordering and record hit-rate evidence.
- **Benchmark-only wins** without a game-level box score; do not claim strength
  from node counts alone.

## Completion report

Return status, files changed, hit-rate and node-count deltas, golden diff,
commands/results, acceptance matrix and follow-ups.
