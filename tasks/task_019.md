# Task 019 — Implement Zobrist hashing and draw detection

| Field | Value |
|---|---|
| **ID** | 019 |
| **File** | `task_000000000000000000000000000019.md` |
| **Stage** | 3 — Complete game state and hardening |
| **Depends on** | 015, 016 |
| **Estimate** | 2 days |
| **Priority** | P1 |
| **Status** | Done |

## Objective

Add a deterministic position hash for repetition detection and later
transposition tables, then implement the remaining draw rules.

## Context

Zobrist hashing must be incremental and verified against full recomputation.
Draw detection covers fifty-move, threefold repetition, and insufficient
material. The hash will later key the transposition table.

## Scope

### In scope

- Implement deterministic Zobrist keys and incremental updates.
- Maintain a position-key history for repetition.
- Implement fifty-move, threefold repetition, and insufficient material.
- Integrate draw categories into `GameResult`.
- Add tests for hash and draw rules.

### Out of scope

- Transposition table; task 024.
- Search draw scoring; task 022.
- UCI output.

## Requirements

1. Use fixed keys generated deterministically (no runtime `rand()` without a fixed seed).
2. Hash piece-square, side to move, castling rights, and en passant state.
3. Update the hash incrementally in make/unmake; verify it against a full recomputation at every move in tests.
4. Store position keys in history and count repetitions correctly, including the side to move and rights.
5. Fifty-move draw applies when the halfmove clock reaches 100 plies.
6. Insufficient material covers K vs K, K+B vs K, K+N vs K, and K+B vs K+B with bishops on the same color.
7. Return draw results from `Result()` when the side to move is not checkmated.
8. Add tests for hash restoration after undo, repetition across moves, fifty-move boundary, and each insufficient-material case.

## Deliverables

- Zobrist implementation
- Draw detection
- Tests for hash and draw rules

## Acceptance criteria

- [ ] Incremental hash equals full recomputation after every move in randomized tests.
- [ ] Threefold repetition is detected across reversible move sequences.
- [ ] Fifty-move draw triggers at the correct boundary.
- [ ] Insufficient-material cases are detected and non-cases are not.
- [ ] All existing tests pass.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug --output-on-failure
```

Expected: hash and draw-rule tests pass.

## Constraints and failure modes

- Do not add the transposition table yet.
- Do not use a hash collision as proof of equality without a verification strategy.
- Do not change perft behavior; write a dedicated test for any interaction.

## Completion notes

- Added deterministic SplitMix64 keys for piece-square, side, castling, and
  effective en passant state. `Position::Hash()` is maintained incrementally in
  make and restored exactly on undo; `ComputeHash()` is the test oracle.
- History entries store the position hash. Repetition counts matching prior
  positions and verifies each candidate by reconstructing the old board, so a
  hash collision cannot by itself produce a draw.
- Added `DrawFiftyMove`, `DrawRepetition`, and `DrawInsufficientMaterial`
  categories (with short aliases). `Result()` gives checkmate/stalemate
  precedence, then applies the 100-halfmove, repetition, and insufficient
  material rules.
- Insufficient material covers K vs K, K+B vs K, K+N vs K, and same-colored
  K+B vs K+B. Tests cover hash restoration, special-move hashes, repetition
  across reversible cycles, the 50-move boundary, mate precedence, and
  positive/negative material cases. The property suite now checks hash
  recomputation after every make and undo.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
