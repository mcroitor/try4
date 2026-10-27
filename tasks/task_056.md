# Task 056 - Add guarded null-move, LMR, check extensions and delta pruning

| Field | Value |
|---|---|
| **ID** | 056 |
| **Stage** | Search strength - selective pruning |
| **Depends on** | 055 |
| **Estimate** | 1.5-2 days |
| **Priority** | P2 |
| **Status** | Blocked |

**Blocker:** waiting for task 055. Flip to `Ready` when 055 is `Done`.

## Objective

Buy depth with selective pruning that is safe by construction: each technique is
guarded, individually switchable, and validated against the tactical corpus, so
a regression can be attributed and reverted.

## Background

The search has no null-move pruning, late-move reductions, check extensions,
futility or delta pruning. `docs/ROADMAP.md` Stage 7 lists null-move, LMR,
futility and SEE as deferred; delta pruning was optional in Stage 4 and was not
implemented; check extensions were never planned. Task 051 removed the per-node
overhead and task 055 removed window waste, so the next gains come from
searching a narrower tree rather than a faster one.

## In scope

- A `PruningSettings` block in `SearchLimits`: null-move on/off and reduction
  depth, LMR on/off with threshold, check extension on/off and cap, delta
  pruning on/off and margin. Defaults chosen from the A/B results and recorded.
- Null-move pruning: disabled in check, at the root, at depth < 3, when the side
  to move has only king and pawns, and when the window contains mate scores.
- Late-move reductions: reduce quiet, non-first moves after a depth/move-number
  threshold; re-search the full depth on fail-high; never reduce the hash move,
  captures, promotions, checks or moves made while in check.
- Check extension: extend when in check, bounded by a per-line cap and the
  existing `kMaxPly` guard.
- Delta pruning in quiescence: skip captures that cannot raise alpha even with
  a margin; never in check and never near mate scores.
- New settings included in the TT policy comparison so stale entries cannot
  leak across configurations.
- Optional: SEE-based ordering/pruning behind a separate flag, only if the
  four required techniques are complete and validated.

## Out of scope

- Bitboards, evaluation changes, TT replacement, SMP.
- Changing the tactical-horizon (`ForcingSearchSettings`) contract from task
  044; pruning must not silently truncate required forcing candidates.
- Opening book or tablebase work.

## Requirements

1. Every technique is individually switchable and defaults are documented with
   the A/B evidence that selected them.
2. Guards are tested explicitly: null move in check, all-pawn endings
   (zugzwang), depth below threshold, mate-band windows, LMR on the hash move
   and on captures/checks, extension cap, delta pruning at alpha near mate.
3. The fixed-depth exactness test from tasks 051/055 runs with all new pruning
   disabled and must still match the golden corpus.
4. With pruning enabled, record per-position fixed-depth score changes and a
   rationale for each accepted change; no silent score drift.
5. Tactical suite, mate-in-1/2/3, stalemate traps and the forcing tests must
   remain green. A tactical regression blocks the task.
6. No node explosion: total nodes at fixed time must not increase on any 049
   benchmark position.
7. Depth at 2 s/move in Release improves by at least 2 plies on at least four
   of the six benchmark positions relative to the task 049 baseline.
8. Warning-clean under `/W4 /WX`; ASan/UBSan clean.

## Acceptance matrix

| Case | Expected result |
|---|---|
| All pruning off | Golden corpus identical to task 055 |
| Tactical suite and mate suite | 100% pass; no missed mates |
| Zugzwang/all-pawn fixture | Null move disabled; correct move retained |
| Null move in check | Never attempted (`InCheck` guard test) |
| LMR fail-high | Re-search returns the full-depth score |
| Check extension cap | No unbounded extension chain; ply bound respected |
| Delta pruning near mate | Mate score not pruned |
| Fixed-time benchmark | No position regresses in nodes or depth |
| Depth at 2 s | +2 plies on >= 4 of 6 positions |
| Sanitizers and warnings | Clean |

## Validation commands

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug -L "search|tactical|forcing|unit" \
  --output-on-failure

cmake -S . -B build/performance -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/performance
python scripts/search_bench.py --engine build/performance/try4_uci.exe \
  --cli build/performance/try4.exe --depths 6,7,8,9 --movetime-ms 2000 \
  --repeats 3 --output build/bench-after-056
```

## Risks and failure modes

- **Zugzwang blunders** from null move in pawn endings; the guard and fixture
  are mandatory.
- **Missed mates** from delta/LMR/mate-band interactions; run the mate suite
  after every threshold change.
- **Pruning inside forced lines** that task 044 promised to enumerate; keep the
  forcing contract authoritative and test it.
- **Node explosion** from LMR re-searches without a move-count cap; measure.
- **Settings/TT mismatch** producing stale scores; include all new fields in
  the policy comparison and add a config test.
- **Threshold tuning by cherry-picking**; record every attempted configuration,
  including failures.

## Completion report

Return status, files changed, defaults and the A/B evidence, per-position depth
and node deltas, tactical results, commands/results, acceptance matrix and
follow-ups.
