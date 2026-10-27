# Task 006 — Introduce the chess namespace, core enums, Piece, and Square

| Field | Value |
|---|---|
| **ID** | 006 |
| **File** | `task_000000000000000000000000000006.md` |
| **Stage** | 1 — Domain model and FEN |
| **Depends on** | 003, 004 |
| **Estimate** | 1.5 days |
| **Priority** | P0 |
| **Status** | Done |

## Objective

Create the new domain foundation: compact scoped enums, a self-contained
`Piece`, and a validated `Square` type with algebraic parsing and
formatting.

## Context

The legacy code uses `color_t`, `piece_t`, and `Field` with unchecked
character arithmetic. The new types live in a `chess` namespace and are
introduced alongside the legacy code so the project keeps building.

## Scope

### In scope

- Add `include/chess/types.hpp` and `src/core/types.cpp` (or the equivalent target-tree convention).
- Define `Color`, `PieceType`, `Piece`, and `Square` in `namespace chess`.
- Implement algebraic square parsing/formatting and validity checks.
- Add unit tests for all 64 squares and invalid inputs.
- Add the new sources to `try4_core`.

### Out of scope

- Board representation, FEN, move generation, or search.
- Deleting the legacy `Field`, `color_t`, or `piece_t`; migration happens later.
- Global 64-square constant objects.

## Requirements

1. `Color` has exactly `White` and `Black`; `PieceType` has `None`, `Pawn`, `Knight`, `Bishop`, `Rook`, `Queen`, `King`.
2. `Piece` stores color and type, defaults to an empty piece, and provides `IsEmpty()`, `Color()`, `Type()`, equality, and `ToString()`.
3. `Square` stores an index `0..63` with `a1 = 0`, and provides `IsValid()`, `File()`, `Rank()`, `ToString()`, `Parse()`/`FromAlgebraic()`, and equality/ordering.
4. Parsing accepts exactly two characters in the range `a1`..`h8`, case-insensitively, and returns `std::optional<Square>` or a documented error for invalid input.
5. Use `constexpr`/`inline` helpers instead of 64 global square objects.
6. Keep the legacy code compiling until the migration task.
7. Add tests: all 64 squares round-trip, invalid strings rejected, piece rendering and equality, default empty piece.

## Deliverables

- `include/chess/types.hpp`
- `src/core/types.cpp`
- Unit tests for `Piece` and `Square`
- Updated CMake target sources

## Acceptance criteria

- [ ] All 64 valid squares round-trip through string parsing/formatting.
- [ ] Invalid squares such as `a0`, `i1`, `a9`, and empty strings are rejected.
- [ ] White/black pieces render correctly and compare by color and type.
- [ ] The project builds and all existing tests pass.

## Validation

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug --output-on-failure
```

Expected: the new type tests pass alongside the characterization suite.

## Constraints and failure modes

- Do not index a board with an unchecked `Square`; validate at API boundaries.
- Do not change the legacy API in this task.
- Avoid adding a dependency on a third-party chess library.

## Completion notes

- Added `include/chess/types.hpp` and `src/core/types.cpp` with `Color`,
  `PieceType`, `Piece`, `Square`, and the supporting parsing/formatting
  helpers.
- `Square` uses `a1 = 0`, validates exactly `a1`..`h8` case-insensitively, and
  supports round-trip formatting and ordering.
- The legacy code remains untouched and the full suite passes with warnings
  treated as errors.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
