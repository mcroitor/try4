# Task 052 - Replace the full sort with a score-once staged move picker

| Field | Value |
|---|---|
| **ID** | 052 |
| **Stage** | Search performance - move ordering |
| **Depends on** | 051 |
| **Estimate** | 0.5 day |
| **Priority** | P1 |
| **Status** | Blocked |

**Blocker:** waiting for task 051. Flip to `Ready` when 051 is `Done`.

## Objective

Keep the current ordering policy but stop paying for it: score each move once,
select the next move lazily, and never sort moves that are not searched because
of an early cutoff.

## Background

`OrderMoves` (`src/core/search.cpp:180-192`) runs `std::stable_sort` with a
comparator that calls `MoveOrderScore` twice per comparison. Each call performs
`IsCapture`, `CaptureVictim`, two `PieceAt` lookups, `PieceValue` clamping,
killer comparisons and history lookups (`search.cpp:144-176`). For an interior
node with 30 moves that is roughly 300 score evaluations before the first move
is searched, even when the first move cuts off immediately.

## In scope

- A staged picker: hash move, then winning captures by MVV-LVA, then
  promotions, then killers, then history-ordered quiet moves.
- Compute the ordering key once per move into a parallel array; pick the best
  remaining move per iteration (`select`/swap) rather than sorting.
- Precomputed MVV-LVA table or an equivalent constant-time score that does not
  call `PieceAt` in the comparator.
- Deterministic tie-break by the generator's original index.
- `useMoveOrdering=false` keeps returning the generator order and remains the
  no-ordering A/B path.

## Out of scope

- New ordering heuristics (counter-moves, SEE, continuation history).
- Pruning, reductions or extensions (tasks 055-056).
- TT replacement (task 053).

## Requirements

1. Fixed-depth scores and best moves must be identical on the task 051 golden
   corpus; ordering changes may not change the minimax result.
2. Fixed-depth node counts must not increase on the corpus, and should drop by
   at least 10% in aggregate.
3. The hash move is always tried first when the TT returns one.
4. Quiet moves keep killers before history; the history table update on cutoff
   is unchanged in policy.
5. Ordering remains deterministic for equal scores (original generator index).
6. No allocation and no `std::stable_sort` in the node path.
7. `useMoveOrdering=false` still yields the same minimax score on the small
   oracle fixture used by the existing search tests.

## Acceptance matrix

| Case | Expected result |
|---|---|
| Golden corpus, fixed depth | Identical score, best move and PV |
| Fixed-depth node counts | No increase; aggregate reduction >= 10% |
| Hash-move position | Hash move searched first |
| Equal-score moves | Deterministic order across runs |
| `useMoveOrdering=false` | Oracle score unchanged |
| Allocation check | No allocation in the picker |
| Suites | `search`, `tactical`, `unit` pass |

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
  --cli build/performance/try4.exe --depths 5,6,7 --output build/bench-after-052
```

## Risks and failure modes

- **Silent ordering inversion** (e.g., killers before promotions) may inflate
  nodes without changing scores; compare node counts per position.
- **History saturation.** Keep the existing caps unless a separate, measured
  change is proposed.
- **Non-determinism** from unstable selection; the tie-break rule is part of
  the contract.
- **Score changes** mean a legality or score bug, not a tuning success; revert.

## Completion report

Return:

- **Status:** done or blocked
- **Files changed:** paths and one-line descriptions
- **Commands and results:** builds, suites, node-count and NPS deltas per
  position, golden-corpus diff
- **Acceptance matrix:** each row pass or fail
- **Risks or follow-ups:** any ordering deviations and the smallest next action
