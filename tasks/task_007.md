# Task 007 — Implement the Move type with explicit flags and promotion

| Field | Value |
|---|---|
| **ID** | 007 |
| **File** | `task_000000000000000000000000000007.md` |
| **Stage** | 1 — Domain model and FEN |
| **Depends on** | 006 |
| **Estimate** | 1 day |
| **Priority** | P0 |
| **Status** | Done |

## Objective

Represent every chess move explicitly, including captures, pawn specials,
castling, and promotion, so later move generation and make/unmake never
infer intent from the board after the fact.

## Context

The legacy `Move` contains `from`, `to`, `piece`, and `taked`; it cannot
represent promotion, castling, or en passant and relies on the caller to
supply the captured piece. The new `Move` is a value type used by the new
core.

## Scope

### In scope

- Define `MoveFlag` as independent bit flags.
- Define `Move` with `from`, `to`, `flags`, and `promotion`.
- Add null-move support, equality, UCI formatting, and optional compact encoding/decoding.
- Add tests for every flag combination and promotion piece.

### Out of scope

- Move generation, legality, or make/unmake.
- SAN formatting; that belongs to task 020.
- Replacing the legacy `Move` struct yet.

## Requirements

1. Use bit flags for `Capture`, `DoublePush`, `EnPassant`, `CastleKing`, `CastleQueen`, and `Promotion`; provide `HasFlag()` and flag composition.
2. `promotion` is `PieceType::None` unless the `Promotion` flag is set; validate allowed promotion pieces (knight, bishop, rook, queen).
3. Provide a null move and `IsNull()`; a null move must not be treated as a legal board move.
4. Provide `ToUci()` in the form `e2e4` and `e7e8q`.
5. If compact encoding is implemented, round-trip from/to/promotion/flags through a fixed-width integer and test all fields.
6. Add tests for quiet, capture, double push, en passant, both castling sides, all four promotions, capture-promotion, and null move.

## Deliverables

- `Move` and `MoveFlag` definitions
- `Move` implementation and tests
- Updated CMake target sources

## Acceptance criteria

- [ ] Every flag can be set and detected independently.
- [ ] Promotion moves carry the correct promotion piece and round-trip through UCI formatting.
- [ ] Null move is distinct from `a1a1` or any ordinary move.
- [ ] Encoding, if added, round-trips without loss.
- [ ] The project builds and all tests pass.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug --output-on-failure
```

Expected: move encoding/formatting tests pass.

## Constraints and failure modes

- Do not infer capture state from the board in this type.
- Do not add SAN parsing in this task.
- Keep the type trivially copyable if practical.

## Completion notes

- Added bit-flag `MoveFlag`, `Move`, null-move detection, validation, and UCI
  formatting (`e2e4`, `e7e8q`, `0000`).
- Promotion state is normalized when the promotion flag is absent and
  validated as knight, bishop, rook, or queen when present.
- Compact move encoding was intentionally deferred; it is optional and can be
  added with the transposition-table work if profiling needs it.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
