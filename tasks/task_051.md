# Task 051 - Restructure the search hot path: one generation per node, fixed PV, no heap

| Field | Value |
|---|---|
| **ID** | 051 |
| **Stage** | Search performance - hot path |
| **Depends on** | 050 |
| **Estimate** | 1.5 days |
| **Priority** | P1 |
| **Status** | Blocked |

**Blocker:** waiting for task 050. Flip to `Ready` when 050 is `Done`.

## Objective

Remove the duplicated legal-move generation, `Position` copies, `Result()`
calls and per-node vector allocations from `Negamax` and `Quiescence`, while
keeping the search tree, scores and node counts unchanged.

## Background

Today every interior search node performs roughly `2 + N` full legal-move
generations (`N` = legal moves):

- `Negamax`: `position.Result()` (`src/core/search.cpp:298`), then
  `position.LegalMoves()` (329), then `position.MakeMove(move)` (344), which
  internally regenerates legal moves through `FindLegalMove`.
- `Quiescence`: `position.Result()` (202), `position.LegalMoves()` (221),
  `position.MakeMove(move)` (257), same cost.
- Each `Result()` also runs `IsThreefoldRepetition()` and a 64-square
  `HasInsufficientMaterial()` scan.
- The PV is propagated with `std::vector<Move>` per node and per child, so
  every node allocates and copies.

Task 049 measured ~20-24k nodes/s in a Debug build where the same build does
~1.1M perft nodes/s. Task 050 provides the allocation-free primitives; this
task wires them into the search.

## In scope

- One `GenerateLegalMoves` per node; unchecked make/unmake per child.
- Cheap terminal and draw detection inside the search:
  - mate/stalemate from the generated list plus `InCheck`;
  - fifty-move from `HalfmoveClock() >= 100`;
  - repetition from `IsRepetition()`;
  - insufficient material only at the root (documented), plus a test for the
    resulting score behavior.
- Triangular PV table (`Move pv[kMaxPly][kMaxPly]` plus per-ply lengths) inside
  `SearchContext`; convert to `SearchInfo::pv` only at the root.
- `MoveList` and in-place compaction instead of `std::vector<Move>` in
  quiescence candidate filtering.
- Allocate `SearchContext` once per `Search()` call (heap or member); allocate
  nothing per node.
- Keep `SearchInfo`, `SearchLimits`, UCI output and stop/abort behavior.

## Out of scope

- Move-ordering algorithm changes (task 052).
- TT replacement/aging and root probing (task 053).
- Evaluation cost (task 054).
- Pruning or extensions (tasks 055-056).
- Time budgeting (task 057).

## Requirements

1. Fixed-depth scores, best moves and PVs must match the pre-change engine on a
   frozen golden corpus: all tactical fixtures, the task 049 benchmark
   positions, and at least 40 seeded random-playout positions. Store the corpus
   and expected outputs in `tests/data/search/golden.tsv`.
2. Fixed-depth node counts must be identical. A difference above 5% means
   ordering or TT behavior drifted and must be explained before the task can be
   completed.
3. Node accounting semantics stay unchanged: one node per `Negamax` entry and
   one per `Quiescence` entry, including the root.
4. Abort semantics preserved: a stopped iteration is discarded, the last
   completed iteration is reported, and make/unmake is restored on every abort
   path. Assert `HistoryDepth()` returns to its pre-node value in debug.
5. No `Position` copy and no heap allocation inside the recursive search loop.
   Provide evidence: an allocation counter in a debug-only test hook, or a
   profiler capture recorded in the completion notes.
6. Draw semantics must be preserved and explicitly tested: checkmate over
   fifty-move, stalemate, threefold repetition inside the search, and the root
   draw contract from `tests/search_tests.cpp`.
7. `kMaxPly` recursion bound preserved; no stack overflow or out-of-bounds PV
   access at the deepest allowed ply.
8. Warning-clean under `/W4 /WX` and `-Wall -Wextra -Wpedantic -Werror`;
   ASan/UBSan clean.

## Acceptance matrix

| Case | Expected result |
|---|---|
| Golden corpus, fixed depth | Identical score, best move and PV |
| Task 049 benchmark, fixed depth | Node counts identical (<= 5% deviation requires justification) |
| Release NPS, start position depth 5-7 | >= 3x the task 049 Release baseline |
| Release `go movetime 2000`, start position | Depth >= 6 |
| Tactical, search and draw suites | All pass |
| Repetition fixture | Search returns the draw score and does not walk into a repetition loss |
| Fifty-move boundary | Score and move unchanged from the baseline |
| `stop` during search | Legal bestmove, last completed iteration reported, position restored |
| Allocation evidence | Zero allocations per node, recorded |
| ASan/UBSan and `/W4 /WX` | Clean |

## Validation commands

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug -L "search|tactical|unit|draw|perft" \
  --output-on-failure

cmake -S . -B build/performance -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/performance
python scripts/search_bench.py --engine build/performance/try4_uci.exe \
  --cli build/performance/try4.exe --depths 5,6,7 --movetime-ms 2000 \
  --repeats 3 --output build/bench-after-051
```

Expected: golden corpus identical, node counts identical, NPS at least 3x the
049 baseline.

## Risks and failure modes

- **Silent draw-rule regressions.** Moving detection out of `Result()` can drop
  fifty-move or repetition handling. Keep the root `Result()` call, add the
  explicit tests above, and never trade a draw test for speed.
- **Unchecked make corrupting state on abort.** Validate restoration after every
  abort path in debug.
- **Node-count drift** from reordered generation or accidental TT changes;
  compare per position, not only in aggregate.
- **Golden corpus overfitting.** Freeze it before the change; do not regenerate
  expected values from the new engine.
- **Stack growth** from the triangular PV table. `kMaxPly` is 128, so the table
  is about 64 KB; keep `SearchContext` off the recursion stack or verify the
  default stack budget on Windows.

## Completion report

Return:

- **Status:** done or blocked
- **Files changed:** paths and one-line descriptions
- **Commands and results:** build/suites, golden-corpus diff, node-count diff,
  NPS and depth before/after, allocation evidence
- **Acceptance matrix:** each row pass or fail
- **Risks or follow-ups:** deviations, deferred items and the smallest next
  action
