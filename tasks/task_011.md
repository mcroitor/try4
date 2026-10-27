# Task 011 — Migrate to a 0x88 board and implement attack detection

| Field | Value |
|---|---|
| **ID** | 011 |
| **File** | `task_000000000000000000000000000011.md` |
| **Stage** | 2 — Legal move generation |
| **Depends on** | 008 |
| **Estimate** | 2 days |
| **Priority** | P0 |
| **Status** | Done |

## Objective

Replace the private 64-square board with a 0x88 representation and provide
reliable attack queries that move generation and legality will use.

## Context

`Position` currently uses a private 64-square array. 0x88 gives cheap
off-board detection and a clean basis for move generation. The public API
must not expose 0x88 indices.

## Scope

### In scope

- Store the board in a private 128-entry 0x88 array.
- Map public `Square` indices to internal 0x88 indices.
- Implement `IsSquareAttacked(Square, Color)` and `InCheck(Color)`.
- Add attack tests for pawns, knights, kings, bishops, rooks, and queens.
- Ensure all existing Position and FEN tests still pass.

### Out of scope

- Legal move generation; that is task 015.
- Search or evaluation.
- Changing the public `Square` representation.

## Requirements

1. Use the mapping `index = (rank << 4) | file` internally and `(index & 0x88) == 0` as the on-board test.
2. Keep `Square` 0..63 in the public API; convert at the boundary.
3. Implement pawn attack directions separately for white and black.
4. Implement knight and king offsets with off-board rejection.
5. Implement sliding attacks with ray offsets that stop at the first occupied square.
6. `InCheck(color)` returns whether that side's king is attacked; handle a missing king as an invalid position in debug/assert builds.
7. Add tests for blocked sliders, board edges, both colors, and attacked empty squares.

## Deliverables

- 0x88 board implementation
- Attack detection functions
- Attack test suite
- A note confirming the public API is unchanged

## Acceptance criteria

- [ ] All existing tests pass after the representation migration.
- [ ] Attack detection is correct for all six piece types on both colors.
- [ ] Sliding attacks stop at blockers and cannot wrap around edges.
- [ ] No public API exposes 0x88 indices.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug --output-on-failure
```

Expected: existing core/FEN tests plus the new attack tests pass.

## Constraints and failure modes

- Do not implement move generation in this task.
- Do not leak 0x88 details into headers or tests.
- Do not use undefined behavior for off-board indices.

## Completion notes

- `Position` now stores a private 128-entry 0x88 board with public `Square`
  indices still mapped at the boundary.
- Added `IsSquareAttacked()` and `InCheck()` with pawn, knight, king, and
  sliding attack detection. Missing kings assert in debug builds.
- `tests/attack_tests.cpp` covers blocked sliders, edges, both colors, and
  attacked empty squares (10 cases, 126 assertions); `try4_attack` passes.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
