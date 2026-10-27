# Task 014 — Add castling, en passant, and special-move integration

| Field | Value |
|---|---|
| **ID** | 014 |
| **File** | `task_000000000000000000000000000014.md` |
| **Stage** | 2 — Legal move generation |
| **Depends on** | 012, 013 |
| **Estimate** | 2 days |
| **Priority** | P0 |
| **Status** | Done |

## Objective

Complete pseudo-legal move generation with castling and en passant, and
verify that promotion flags compose correctly with captures.

## Context

Previous tasks cover normal pawn, knight, king, and sliding moves. The
remaining special moves are castling and en passant. Castling is generated
only when rights and path conditions hold; check-related restrictions are
enforced by legality filtering in task 015.

## Scope

### In scope

- Generate both castling moves when rights and path are available.
- Generate en passant captures from the position's en passant target.
- Verify promotion and capture-promotion flags.
- Add tests for rights, blocked paths, missing rooks, and en passant.

### Out of scope

- Legality filtering for castling through check or en passant discovered check; task 015.
- Make/unmake.
- Search.

## Requirements

1. Castling requires the correct right, the king and rook on their home squares, and all squares between them empty.
2. Generate `CastleKing` and `CastleQueen` flags; do not use a normal king move for castling.
3. En passant requires a valid en passant target, a pawn of the side to move on the adjacent rank, and the target square empty.
4. Set `EnPassant` and `Capture` flags for en passant; make/unmake will remove the pawn behind the target in task 015.
5. Promotion moves may also capture; set both `Promotion` and `Capture` flags and carry the promotion piece.
6. Do not generate castling when the king or rook is missing, even if a rights bit is set; report inconsistent state in debug builds.
7. Add tests for both castling sides, blocked paths, missing rights, missing rook, en passant from both colors, and capture-promotion.

## Deliverables

- Special-move generation
- Tests for castling, en passant, and promotion integration

## Acceptance criteria

- [ ] Castling is generated only with rights, home pieces, and an empty path.
- [ ] En passant is generated only from a valid target and adjacent pawn.
- [ ] Promotion and capture-promotion carry the correct flags and piece.
- [ ] No duplicate special moves are generated.
- [ ] All existing tests pass.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug --output-on-failure
```

Expected: special-move generation tests pass.

## Constraints and failure modes

- Do not enforce check restrictions here; task 015 owns legality.
- Do not implement make/unmake in this task.
- Do not silently ignore inconsistent castling rights; report them.

## Completion notes

- Castling is generated for both colors only with the matching right, king and
  rook on home squares, and an empty path; `CastleKing`/`CastleQueen` are used.
- En passant is generated from both colors with `Capture | EnPassant`, requires
  an adjacent enemy pawn and an empty target, and capture-promotions carry both
  flags plus the chosen piece.
- `Position::CastlingRightsConsistent()` reports rights whose king or rook is
  missing; generation never emits castling in that state. Tests cover missing
  rights, missing rooks, blocked paths, both en passant colors, and
  capture-promotion.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
