# Task 008 — Implement Position state and standard setup

| Field | Value |
|---|---|
| **ID** | 008 |
| **File** | `task_000000000000000000000000000008.md` |
| **Stage** | 1 — Domain model and FEN |
| **Depends on** | 006, 007 |
| **Estimate** | 1.5 days |
| **Priority** | P0 |
| **Status** | Done |

## Objective

Create a single value type that owns all chess state: board, side to move,
castling rights, en passant target, and clocks.

## Context

The legacy `Board` owns only pieces and a dead `is_white_move` flag.
`Position` becomes the authoritative game state. It uses a private
simple 64-square board for now; task 011 migrates the representation to
0x88 without changing the public API.

## Scope

### In scope

- Add `Position` to the new core.
- Implement standard setup, clear, piece access, side to move, castling rights, en passant, and clocks.
- Implement ASCII board rendering for debugging.
- Add tests for setup, accessors, copying, and rendering.

### Out of scope

- FEN parsing/serialization; task 009.
- Legal move generation, make/unmake, and result detection.
- Removing the legacy `Board`/`Engine` yet.

## Requirements

1. Store the board privately as `std::array<Piece, 64>` for this task; do not expose indices or representation details.
2. Represent castling rights as a small bitmask or enum set for white/black king-side and queen-side.
3. Represent the en passant target as `std::optional<Square>`.
4. Provide `Clear()`, `SetupInitial()`, `PieceAt(Square)`, `SetPiece(Square, Piece)`, `SideToMove()`, `CastlingRights()`, `EnPassant()`, `HalfmoveClock()`, and `FullmoveNumber()`.
5. Validate `Square` at the public boundary; invalid squares must not index the array.
6. Implement `ToString()` with rank 8 at the top and rank 1 at the bottom, using piece characters and spaces for empty squares.
7. Ensure default copy semantics produce independent positions.
8. Add tests for initial placement, side to move, all castling rights, empty en passant, clocks, copy independence, and board rendering.

## Deliverables

- `include/chess/position.hpp`
- `src/core/position.cpp`
- Unit tests for setup and state access
- Updated CMake target sources

## Acceptance criteria

- [ ] The initial position matches the standard arrangement exactly.
- [ ] White is to move; all four castling rights are present; en passant is empty; halfmove clock is 0; fullmove number is 1.
- [ ] `Clear()` produces an empty board with no rights and no en passant target.
- [ ] Copying a position and mutating the copy does not affect the original.
- [ ] ASCII rendering matches the known initial board.
- [ ] All tests pass.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug --output-on-failure
```

Expected: Position setup and rendering tests pass.

## Constraints and failure modes

- Do not add legal move logic in this task.
- Do not leak the internal board layout through public accessors.
- Do not parse FEN yet; use direct setup calls in tests.

## Completion notes

- Added `include/chess/position.hpp` and `src/core/position.cpp` with a private
  64-square board, side to move, castling rights, en passant, and clocks.
- `Clear`, `SetupInitial`, validated piece access, setters, copy independence,
  equality, and ASCII rendering are covered by tests.
- The internal array remains private so task 011 can migrate to 0x88 without
  changing the public contract.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
