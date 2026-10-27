# Task 013 — Generate pseudo-legal sliding-piece moves

| Field | Value |
|---|---|
| **ID** | 013 |
| **File** | `task_000000000000000000000000000013.md` |
| **Stage** | 2 — Legal move generation |
| **Depends on** | 011 |
| **Estimate** | 1.5 days |
| **Priority** | P0 |
| **Status** | Done |

## Objective

Add bishop, rook, and queen move generation with correct blocking and
capture behavior.

## Context

Sliding pieces need ray-based generation over the 0x88 board. They remain
pseudo-legal until task 015 filters for king safety.

## Scope

### In scope

- Generate bishop, rook, and queen moves.
- Handle blockers, captures, and board edges.
- Set capture flags correctly.
- Add tests for empty boards, blockers, and edges.

### Out of scope

- Legality filtering.
- Castling, en passant, or promotion.
- Search or evaluation.

## Requirements

1. Use the four diagonal and four orthogonal ray directions.
2. A sliding piece stops before a friendly blocker and includes an enemy blocker as a capture, then stops.
3. Do not wrap around board edges.
4. A queen combines bishop and rook rays without duplicates.
5. Add tests: bishop on an empty board, rook blocked by friendly/enemy pieces, queen rays, corner/edge pieces, and capture flags.
6. Ensure no duplicate moves are emitted.

## Deliverables

- Sliding-piece generation
- Unit tests for rays, blockers, edges, and captures

## Acceptance criteria

- [ ] Bishop, rook, and queen move counts match reference positions on an empty board.
- [ ] Blockers stop rays correctly.
- [ ] Friendly pieces are never captured; enemy pieces are captured with the correct flag.
- [ ] No off-board or wrapped moves are generated.
- [ ] All existing tests pass.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug --output-on-failure
```

Expected: sliding-piece tests pass.

## Constraints and failure modes

- Generate pseudo-legal moves only; pinned pieces are allowed here.
- Do not add magic bitboards.
- Do not duplicate queen rays from bishop and rook generation.

## Completion notes

- Added bishop, rook, and queen ray generation with first-blocker stopping and
  0x88 off-board rejection; no edge wraparound.
- Tests cover all sliding piece types, blocked rays, board edges, and quiet
  versus capture flags.
- `try4_movegen` passes.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
