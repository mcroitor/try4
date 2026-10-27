# Task 015 — Implement make/unmake history and legal move filtering

| Field | Value |
|---|---|
| **ID** | 015 |
| **File** | `task_000000000000000000000000000015.md` |
| **Stage** | 2 — Legal move generation |
| **Depends on** | 014 |
| **Estimate** | 2.5 days |
| **Priority** | P0 |
| **Status** | Done |

## Objective

Make and undo every move exactly, then filter pseudo-legal moves so the
public API only exposes legal moves.

## Context

The legacy make/undo restores only two squares. The new `Position` needs a
history record covering captures, promotion, castling, en passant, rights,
clocks, and side to move. Legal filtering uses make/check/undo.

## Scope

### In scope

- Implement a history record and `MakeMove`/`UndoMove`.
- Implement `LegalMoves()` and `IsLegal(Move)`.
- Handle castling through check, en passant discovered check, and promotion.
- Add tests for state restoration and legal move counts.

### Out of scope

- Checkmate/stalemate result detection; task 016.
- Zobrist hashing; task 019.
- Search.

## Requirements

1. Record at minimum: moving and captured pieces, previous castling rights, previous en passant target, previous clocks, and side to move.
2. Provide a private unchecked make used only by the legal-move filter; the public `MakeMove(Move)` validates legality first.
3. Public `MakeMove(Move)` applies a legal move and returns `false` for illegal moves without changing state.
4. `UndoMove()` takes no arguments and restores the exact previous position.
5. Castling moves the rook as well as the king and updates rights.
6. En passant removes the captured pawn from behind the target square.
7. Promotion replaces the pawn with the selected piece.
8. `LegalMoves()` filters pseudo-legal moves by making the move and checking that the mover's king is not attacked; undo afterward.
9. Castling is illegal while in check, through check, or into check.
10. En passant is illegal if it exposes the king to a rook/queen attack along the rank.
11. Add tests: initial position has 20 legal moves; make/undo restores FEN and state for quiet, capture, double push, castling, en passant, promotion, and capture-promotion; illegal moves are rejected.

## Deliverables

- History record and make/unmake implementation
- Legal move filtering
- Tests for special moves and state restoration

## Acceptance criteria

- [ ] `MakeMove` followed by `UndoMove` restores the exact FEN and all state fields for every move type.
- [ ] The initial position has exactly 20 legal moves.
- [ ] King capture never appears in `LegalMoves()`.
- [ ] Castling through check and en passant discovered check are rejected.
- [ ] Illegal `MakeMove` calls leave the position unchanged.
- [ ] All existing tests pass.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug --output-on-failure
```

Expected: make/unmake and legal-move tests pass.

## Constraints and failure modes

- Do not add result detection or draw rules in this task.
- Do not add hashing until task 019.
- Do not expose pseudo-legal moves through the public API once legal filtering exists.

## Completion notes

- Added a history record containing the move, captured piece, castling rights,
  en passant target, clocks, and side to move. `MakeMove` validates through
  `LegalMoves()` and stores the canonical move; `UndoMove` restores all state.
- Implemented rook movement for castling, pawn removal for en passant,
  promotion replacement, rights updates on king/rook moves and rook captures,
  en passant target creation, and halfmove/fullmove updates.
- `LegalMoves()` filters pseudo-legal moves by make/undo king safety, including
  castling through check and en passant discovered check.
- `tests/legal_tests.cpp` covers make/undo for every move type, illegal-move
  rejection, castling restrictions, pins, king safety, and Kiwipete's 48 moves.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
