# Task 023 — Add quiescence search and move ordering

| Field | Value |
|---|---|
| **ID** | 023 |
| **File** | `task_000000000000000000000000000023.md` |
| **Stage** | 4 — Search foundation |
| **Depends on** | 022 |
| **Estimate** | 1.5 days |
| **Priority** | P1 |
| **Status** | Done |

## Objective

Reduce horizon effects with quiescence search and improve alpha-beta
efficiency with deterministic move ordering.

## Context

Alpha-beta alone can stop in the middle of a capture sequence. Quiescence
extends captures and promotions; move ordering makes the search practical.

## Scope

### In scope

- Implement quiescence search with stand-pat and capture/promotion generation.
- Add MVV-LVA, killer moves, and history heuristics.
- Integrate ordering into the main search.
- Add tactical tests that demonstrate the improvement.

### Out of scope

- Transposition table; task 024.
- Advanced pruning such as null move/LMR; task 034.
- Evaluation tuning; tasks 026–028.

## Requirements

1. Quiescence searches only tactical moves (captures and promotions) plus stand-pat, with a documented depth/ply cap.
2. Do not stand pat when in check; search evasions or use a documented alternative.
3. MVV-LVA orders captures by victim value minus attacker value.
4. Killer moves are stored per ply and tried after captures; history scores are updated on beta cutoffs.
5. Ordering must be deterministic for equal scores.
6. Add tests: a hanging queen is captured, a capture sequence is resolved beyond the nominal depth, and the existing search regression suite does not regress.

## Deliverables

- Quiescence search
- Move ordering heuristics
- Search regression tests

## Acceptance criteria

- [ ] Quiescence returns legal moves and terminates.
- [ ] Obvious winning captures are found at shallow depth.
- [ ] Move ordering does not change the minimax result at fixed depth, only efficiency.
- [ ] All existing search tests pass.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug -L search --output-on-failure
```

Expected: search tests pass. The formal tactical suite and its `tactical`
label are added in task 025.

## Constraints and failure modes

- Do not add pruning that changes correctness without tests.
- Do not use nondeterministic tie-breaking.
- Do not let quiescence recurse without a bound.

## Completion notes

- Added quiescence with stand-pat, capture/en-passant/promotion generation, and
  an eight-ply safety cap. In-check nodes do not stand pat; they search all legal
  evasions (falling back to static evaluation at the documented cap).
- Added deterministic move ordering: MVV-LVA captures, then promotions, then
  per-ply killers, then history scores. History and killers are updated only on
  quiet beta cutoffs. `std::stable_sort` preserves generator order on ties.
- Added `useMoveOrdering` and `useQuiescence` test toggles. Tests show a
  recapture beyond nominal depth is resolved, and ordering preserves the
  minimax score while reducing nodes on a tactical reference position.
- All search tests and the full 14-executable suite pass.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
