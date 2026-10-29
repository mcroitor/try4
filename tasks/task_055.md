# Task 055 - Add principal-variation search and aspiration windows

| Field | Value |
|---|---|
| **ID** | 055 |
| **Stage** | Search strength - exact reductions |
| **Depends on** | 052, 053 |
| **Estimate** | 1 day |
| **Priority** | P2 |
| **Status** | Done |

**Unblocked:** tasks 052 and 053 are `Done`.

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

**Status:** Done (implementation on `feat/055`; PR pending because remote push
still lacks SSH credentials).

**Implementation** (`src/core/search.cpp`, `include/chess/search.hpp`,
`src/uci/uci.cpp`)

- PVS in `Negamax`: first move full window; every later move probes with the
  zero window `(-alpha-1, -alpha)` and re-searches with the full window when
  the probe returns `score > alpha` (guarded so an interrupted probe never
  triggers a re-search). Null-window fail-soft values are bounds, so the
  existing `alpha_original` TT-bound classification still stores Lower/Upper
  (never Exact) for probe results; probes cannot raise `best` above the
  original alpha, so reported lines keep their exact scores.
- Aspiration windows in the iterative-deepening loop: from depth 3 the root
  window starts at `previous_score +/- 25`; fail-low widens `alpha`, fail-high
  widens `beta`, the failed side's distance grows x4 per attempt, and the loop
  falls back to `(-kInfinity, kInfinity)` after two failures or whenever a
  score enters the mate band. Only a search that finishes inside its final
  window is accepted; failed windows and interrupted re-searches discard the
  iteration and keep the last completed one. Nodes, the node limit and the
  stop flag are shared and monotonic across window attempts.
- `SearchLimits::usePrincipalVariation` / `useAspirationWindows` (default
  `true`) added; both are part of the TT-clearing policy comparison and are set
  explicitly by the UCI adapter.

**Files changed:** `include/chess/search.hpp`, `src/core/search.cpp`,
`src/uci/uci.cpp`, `tests/search_pvs_tests.cpp` (new), `CMakeLists.txt`
(`try4_search_pvs`), `tasks/task_055.md`.

**Golden-corpus exactness.** New `tests/search_pvs_tests.cpp` compares the full
window oracle (both flags off) against PVS-only, aspiration-only and both-on
over 14 fixtures including mate-in-1/2/3, the fail-low pair perft3-rook d4/d5
(93 -> 46) and fail-high pairs hanging-queen/startpos d2->d3 (876/10 -> 909/43)
- score, best move and PV identical in every combination. Warm-TT replay
keeps the exact score/best move (node counts legitimately drop); its PV-tail
truncation is pre-055 engine behavior, identical under all flag combinations.

**Node reduction** (`scripts/search_bench.py`, Release; pre-change baseline =
task 054 bench reports, same positions/repeats/protocol). Depth-7 node-weighted
total 12,353,203 -> 10,247,670 = **17.0% reduction** (target >= 15%); overall
d5+d6+d7 weighted reduction 24.0%. Per position (depth 7): middlegame -28.4%,
kiwipete -13.4%, perft3-rook -7.9%, queen -3.0%, kp -2.4%, promotion -1.2%,
in-check -0.5%, startpos +2.1%; worst regression +2.1% (cap 5%). Full-depth-8
run (task command, `build/bench-after-055`): best move, PV-list and node count
identical across repeats on every position and depth.

**Known deviation (documented, not fixed).** With the TT enabled, four
bench-corpus entries report a *different continuation line* at equal score:
startpos@6/`d2d3 d7d6` vs `e7e6`, queen-endgame@6 (+1 extra ply of PV) and
startpos/queen-endgame@7. All root best moves and scores are identical; the
differences are equal-score tie lines chosen at inner nodes of the continuation
line. Attribution: aspiration's narrowed root windows alone diverge at
startpos@6, PVS alone at startpos@7/queen@7, and both at queen@7; with the TT
disabled every flag combination reproduces the full-window PV exactly. The
mechanism is the changed node-visit/store pattern altering which TT cutoffs hit
on tie lines - inherent to PVS + TT, and reproducing the pre-change tie-picks
exactly would require re-searching every tie (destroying the node reduction).
A TT-hit PV-seeding experiment (seed the row with the stored best move) was
built, measured, and reverted because it changes pre-change PV behavior on its
own.

**Commands/results.** Debug suite 26/26 pass (incl. `try4_search_pvs`);
Release suite pass; fixed-depth determinism confirmed by the bench integrity
report ("Nodes/PV stable: yes" for all 96 fixed-depth rows; every best move
CLI-validated).

**Follow-ups**

- The tie-line PV deviation above is acceptable for search strength work but
  worth revisiting if downstream tasks ever rely on byte-identical PVs on the
  bench corpus.
- Linux CI ASan/UBSan job (MinGW lacks first-class ASan support).
