# Task 012 — Generate pseudo-legal pawn, knight, and king moves

| Field | Value |
|---|---|
| **ID** | 012 |
| **File** | `task_000000000000000000000000000012.md` |
| **Stage** | 2 — Legal move generation |
| **Depends on** | 011 |
| **Estimate** | 2 days |
| **Priority** | P0 |
| **Status** | Done |

## Objective

Implement the first move-generation layer for pawns, knights, and kings,
including promotions and correct flags, without legality filtering.

## Context

The engine currently returns no moves. This task adds a move-list
generator and the non-sliding piece rules. Castling and en passant are
added in task 014; legality filtering is task 015.

## Scope

### In scope

- Add a `MoveList`/generator API in the new core.
- Generate pawn pushes, double pushes, captures, and promotions.
- Generate knight and king moves.
- Set move flags correctly.
- Add tests for counts and edge cases.

### Out of scope

- Castling and en passant.
- Check/legality filtering.
- Sliding pieces; task 013.
- Make/unmake.

## Requirements

1. Pawns move forward one square to an empty square; from their starting rank they may move two squares if both squares are empty.
2. Pawns capture diagonally only onto an enemy piece; never capture forward or onto a friendly piece.
3. Promotion occurs on the last rank and sets the `Promotion` flag plus the chosen piece; generate all four promotion choices.
4. Knights and kings use their offsets and reject off-board destinations; they may capture enemies but not friendly pieces.
5. Do not generate en passant or castling in this task.
6. Use `MoveFlag` consistently: captures set `Capture`, double pawn pushes set `DoublePush`.
7. Add tests from the start position (20 pseudo-legal moves), blocked pawns, edge-rank knights/kings, captures, and promotions.
8. Ensure no duplicate moves are emitted.

## Deliverables

- Move generator module
- Pawn/knight/king generation
- Unit tests for counts, flags, blocking, and promotion
- Updated CMake target sources

## Acceptance criteria

- [ ] From the initial position, exactly 20 pseudo-legal moves are generated.
- [ ] Pawns cannot jump over blockers and promote on the last rank with all four choices.
- [ ] Knights and kings cannot leave the board and cannot capture friendly pieces.
- [ ] Move flags match the generated move type.
- [ ] All existing tests pass.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug --output-on-failure
```

Expected: move-generation tests pass.

## Constraints and failure modes

- Do not filter for check yet; pinned pieces may be generated as pseudo-legal.
- Do not add castling or en passant.
- Do not use board mutation to test moves; task 015 adds make/unmake.

## Completion notes

- Added pseudo-legal pawn, knight, and king generation with captures, double
  pawn pushes, promotions, and capture-promotions.
- `MoveGenerator::GeneratePseudoLegal` fills the fixed-capacity `MoveList`;
  the start position produces 20 moves and has no duplicates.
- `try4_movegen` passes the pawn/knight/king, promotion, and edge tests.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
