# Task 055 - Add principal-variation search and aspiration windows

| Field | Value |
|---|---|
| **ID** | 055 |
| **Stage** | Search strength - exact reductions |
| **Depends on** | 052, 053 |
| **Estimate** | 1 day |
| **Priority** | P2 |
| **Status** | Blocked |

**Blocker:** waiting for tasks 052 and 053. Flip to `Ready` when both are
`Done`.

## Objective

Reduce the number of nodes needed for the same exact score by searching the
first move with a full window, the remainder with a zero window, and the root
iteration inside a narrow aspiration window, with mandatory re-searches when a
window fails.

## Background

The current `Negamax` (`src/core/search.cpp:290-388`) searches every move with
the full `(-beta, -alpha)` window, and the iterative-deepening loop
(`search.cpp:481-509`) restarts each depth with `(-kInfinity, kInfinity)`.
Both are exact but waste nodes once a good first move exists or the score is
stable across depths.

## In scope

- Principal-variation search (PVS):
  - first move: full window;
  - later moves: zero window `(-alpha-1, -alpha)`;
  - re-search with the full window when the zero-window search fails high.
- Aspiration windows at the root for depths >= 3 (configurable):
  - start around the previous iteration's score (for example +/-25 cp);
  - widen on fail-low/fail-high (for example x4 per attempt) and fall back to
    the full window after two failures or when the score is in the mate band.
- `SearchLimits::usePrincipalVariation` and `useAspirationWindows` (default
  `true`), included in the policy comparison that clears the TT
  (`search.cpp:410-416`).
- Tests for the window arithmetic, fail-high re-search and fail-low widening.
- Benchmark evidence on the task 049 positions.

## Out of scope

- Pruning that changes the minimax value (null move, LMR, futility, delta):
  tasks 056.
- Evaluation, ordering policy or TT replacement changes.
- Multi-PV, pondering or SMP.

## Requirements

1. PVS and aspiration are exact: for every fixed-depth search in the golden
   corpus the score, best move and PV must match the pre-change engine. A
   mismatch is a bug, not a tuning result.
2. No score may be reported from an unfinished or failed window. A failed
   aspiration search is re-searched with a wider window; an interrupted
   re-search discards the iteration and keeps the last completed one.
3. Mate scores stay exact across window narrowing and widening; the zero-window
   fail-high test must include a mate score.
4. TT entries produced under a zero-window search are stored with the correct
   bound (lower/upper), never as exact.
5. With `usePrincipalVariation=false` and `useAspirationWindows=false` the
   search must reproduce the task 051 behavior, so the A/B oracle in the tests
   stays valid.
6. Stop/time/node checks remain at least every 32 nodes; an aspiration
   re-search must not double-count nodes or reset the node limit.
7. Fixed-depth node counts must show at least a 15% aggregate reduction at
   depth 7, with no position regressing by more than 5%.

## Acceptance matrix

| Case | Expected result |
|---|---|
| Golden corpus, fixed depth | Identical score, best move and PV |
| Depth-7 node counts, 049 positions | >= 15% aggregate reduction; no position regresses > 5% |
| Mate-in-1/2/3 fixtures | Solved; exact mate scores preserved |
| Fail-low widening fixture | Score found after widening; iteration completes or is discarded cleanly |
| Zero-window fail-high | Full re-search returns the same score as a full-window search |
| Flags off | Task 051 behavior reproduced exactly |
| Suites | `search`, `tactical`, `unit` pass |
| Determinism | Identical node counts and PVs on repeat runs |

## Validation commands

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug -L "search|tactical|unit" \
  --output-on-failure

cmake -S . -B build/performance -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/performance
python scripts/search_bench.py --engine build/performance/try4_uci.exe \
  --cli build/performance/try4.exe --depths 5,6,7,8 --movetime-ms 2000 \
  --repeats 3 --output build/bench-after-055
```

## Risks and failure modes

- **Reporting a window-clipped score** as exact is the classic aspiration bug;
  the iteration is only complete after a search inside the final window.
- **TT bound confusion** after zero-window searches; rely on the existing
  bound/alpha-original logic and add a targeted test.
- **Search instability** from re-search loops; cap widening attempts and fall
  back to the full window.
- **Stop handling during re-search**; keep node/time accounting in one place.

## Completion report

Return status, files changed, golden diff, depth-7 node reduction per position,
mate tests, commands/results, acceptance matrix and follow-ups.
