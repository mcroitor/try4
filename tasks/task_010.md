# Task 010 — Harden the core and FEN regression suite

| Field | Value |
|---|---|
| **ID** | 010 |
| **File** | `task_000000000000000000000000000010.md` |
| **Stage** | 1 — Domain model and FEN |
| **Depends on** | 009 |
| **Estimate** | 1 day |
| **Priority** | P0 |
| **Status** | Done |

## Objective

Expand unit coverage for the new types and FEN so Stage 2 starts from a
trustworthy, well-tested state.

## Context

Tasks 006–009 add the new types, Position, and FEN with basic tests. This
task turns them into a durable regression suite with table-driven cases,
labels, and coverage of edge conditions.

## Scope

### In scope

- Expand tests for `Color`, `PieceType`, `Piece`, `Square`, `MoveFlag`, `Move`, and `Position`.
- Add a table-driven FEN corpus with valid and invalid cases.
- Add CTest labels such as `unit` and `fen`.
- Add a board-rendering golden test.
- Verify copy independence and clock/rights preservation.

### Out of scope

- Production refactors beyond minimal bug fixes.
- Legal move generation.
- Performance tests.

## Requirements

1. Cover all 64 square round-trips, invalid square inputs, and file/rank boundaries.
2. Cover every move flag and promotion piece.
3. Cover FEN: start position, Kiwipete, positions 3–6 from the architecture doc, empty castling/en passant, both sides to move, and clock extremes.
4. Cover invalid FEN: missing king, duplicate king, bad rank count, bad file count, invalid piece, invalid side, invalid castling, invalid en passant, negative halfmove, zero fullmove.
5. Use `tests/data/` for reusable FEN fixtures rather than embedding long strings everywhere.
6. Register fast tests under `unit` and FEN tests under `fen`.
7. If a production bug is found, add a focused regression test and the smallest fix.

## Deliverables

- Expanded test suite
- FEN fixture corpus
- CTest labels
- A short coverage or test-count summary

## Acceptance criteria

- [ ] All new tests pass.
- [ ] `ctest -L unit` and `ctest -L fen` both pass.
- [ ] The FEN corpus covers all six perft positions and the listed invalid cases.
- [ ] No existing test regresses.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug -L unit --output-on-failure
ctest --test-dir build/cmake -C Debug -L fen --output-on-failure
```

Expected: both labeled suites pass.

## Constraints and failure modes

- Do not weaken validation to make a test pass.
- Do not add tests for unimplemented move generation.
- Keep tests fast enough for pull-request CI.

## Completion notes

- The suite now has 36 Catch2 test cases across four CTest tests:
  6 characterization, 15 type/move, 9 position, and 6 FEN cases.
- `ctest -L unit` runs the characterization, type/move, and position suites;
  `ctest -L fen` runs the FEN suite.
- The FEN corpus is table-driven and lives in `tests/data/fen/`; the test
  target receives an absolute data path at compile time.
- No production defects were found during the expanded coverage.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
