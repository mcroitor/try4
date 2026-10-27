# Task 009 — Implement strict FEN parsing, serialization, and validation

| Field | Value |
|---|---|
| **ID** | 009 |
| **File** | `task_000000000000000000000000000009.md` |
| **Stage** | 1 — Domain model and FEN |
| **Depends on** | 008 |
| **Estimate** | 1.5 days |
| **Priority** | P0 |
| **Status** | Done |

## Objective

Make every position reproducible from a standard FEN string and emit a
canonical FEN back, with useful errors for malformed input.

## Context

`Board::SetBoard` and `Board::GetFen` are stubs. FEN is required for test
fixtures, UCI `position fen`, and debugging. The six perft positions in
`docs/ARCHITECTURE.md` are the first corpus.

## Scope

### In scope

- Implement `Position::FromFen(std::string_view, std::string* error)`.
- Implement `Position::ToFen() const`.
- Validate all six FEN fields and provide field-specific error messages.
- Add round-trip tests for the six perft positions.
- Add tests for malformed inputs.

### Out of scope

- Move generation or legality.
- Accepting or silently repairing impossible positions beyond the documented validation rules.
- PGN or SAN.

## Requirements

1. Parse piece placement, side to move, castling rights, en passant target, halfmove clock, and fullmove number.
2. Require exactly eight ranks and eight files per rank; support digits `1`–`8` and the standard piece letters.
3. Require exactly one white king and one black king; reject missing or duplicate kings.
4. Accept `w`/`b` for side to move and `-` for empty castling/en passant fields.
5. Validate castling rights as a subset of `KQkq` with no duplicates; reject invalid combinations.
6. Validate en passant as `-` or a square on rank 3 or 6; do not invent a legality check that rejects valid FENs.
7. Require non-negative halfmove clock and fullmove number of at least 1.
8. Return `std::nullopt` and populate a useful error on failure; never partially mutate a position.
9. Round-trip canonical FENs exactly.

## Deliverables

- `include/chess/fen.hpp` (or Position methods)
- `src/core/fen.cpp`
- Round-trip and invalid-input tests
- A small FEN test corpus under `tests/data/fen/`

## Acceptance criteria

- [ ] All six perft FENs parse and round-trip exactly.
- [ ] Missing kings, duplicate kings, wrong rank/file counts, invalid pieces, invalid side, invalid rights, invalid en passant, negative clocks, and zero fullmove are rejected.
- [ ] Errors identify the failing field or a clear reason.
- [ ] A failed parse does not leave a partially modified position.
- [ ] All tests pass.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug --output-on-failure
```

Expected: FEN round-trip and invalid-input tests pass.

## Constraints and failure modes

- Do not silently accept malformed FEN.
- Do not validate full legality (e.g., side not to move in check) beyond the stated rules.
- Do not add move generation dependencies to the parser.

## Completion notes

- Added `Position::FromFen` and `Position::ToFen`, implemented in
  `src/core/fen.cpp`, with field-specific error messages and no partial
  mutation on failure.
- The parser validates rank/file counts, exactly one king per side, side,
  castling rights, en passant rank, halfmove clock, and fullmove number.
- Nine canonical fixtures under `tests/data/fen/` round-trip, including the
  six perft positions, both sides, clock extremes, and a non-empty en passant
  target.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
