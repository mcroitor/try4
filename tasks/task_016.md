# Task 016 — Implement checkmate, stalemate, and game result detection

| Field | Value |
|---|---|
| **ID** | 016 |
| **File** | `task_000000000000000000000000000016.md` |
| **Stage** | 2 — Legal move generation |
| **Depends on** | 015 |
| **Estimate** | 1 day |
| **Priority** | P0 |
| **Status** | Done |

## Objective

Determine whether a position is ongoing, checkmate, or stalemate from the
legal move list and check state.

## Context

After task 015 the engine knows legal moves. This task adds the first
game-result layer; draw rules are added in task 019.

## Scope

### In scope

- Add a `GameResult` enum and `Position::Result()`.
- Detect checkmate and stalemate.
- Add tests for known mate and stalemate positions.

### Out of scope

- Fifty-move, repetition, and insufficient-material draws; task 019.
- Search or evaluation.
- UCI output.

## Requirements

1. `Result()` returns `Checkmate` when the side to move is in check and has no legal moves.
2. Return `Stalemate` when the side to move is not in check and has no legal moves.
3. Return `Ongoing` otherwise; leave draw categories for task 019.
4. Add tests: Fool's mate, Scholar's mate, a back-rank mate, a known stalemate FEN, check with an escape, and an ordinary position.

## Deliverables

- `GameResult` enum and `Result()`
- Mate/stalemate tests

## Acceptance criteria

- [ ] Checkmate and stalemate are distinguished correctly.
- [ ] A position with legal moves is never reported as mate or stalemate.
- [ ] The side to move is the side evaluated for mate/stalemate.
- [ ] All existing tests pass.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug --output-on-failure
```

Expected: result-detection tests pass.

## Constraints and failure modes

- Do not implement draw rules here.
- Do not rely on board material alone; use legal moves.
- Do not add search-dependent mate detection.

## Completion notes

- Added `GameResult` and `Position::Result()`; checkmate requires check plus no
  legal moves, while stalemate requires no check and no legal moves.
- `tests/result_tests.cpp` covers Fool's mate, Scholar's mate, back-rank mate,
  a known stalemate, a check with an escape, and ordinary positions.
- Draw categories remain intentionally deferred to task 019.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
