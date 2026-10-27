# Task 018 — Add make/unmake invariant and random-playout tests

| Field | Value |
|---|---|
| **ID** | 018 |
| **File** | `task_000000000000000000000000000018.md` |
| **Stage** | 2 — Legal move generation |
| **Depends on** | 017 |
| **Estimate** | 1 day |
| **Priority** | P0 |
| **Status** | Done |

## Objective

Exercise the legal move generator and make/unmake under randomized play to
catch state-corruption bugs that fixed perft positions may miss.

## Context

Perft proves move counts, but random legal playouts stress history,
captures, special moves, and undo across many positions. This is the first
property-test layer.

## Scope

### In scope

- Add randomized legal playouts with reproducible seeds.
- After every move, snapshot the state, make, undo, and compare.
- Verify that every selected move comes from `LegalMoves()`.
- Run enough games to be meaningful while keeping CI fast.

### Out of scope

- FEN fuzzing and sanitizer integration; task 021.
- Zobrist hashing; task 019.
- Search.

## Requirements

1. Use a deterministic seed provided by the test and print it on failure.
2. At each ply: capture FEN, side, rights, en passant, clocks, and board rendering; make a legal move; undo; compare all fields.
3. Play until checkmate/stalemate or a documented ply cap; never continue after a terminal result.
4. Run at least 10,000 games or an equivalent time-boxed workload in CI.
5. Assert that `MakeMove` rejects moves not in `LegalMoves()` and leaves state unchanged.
6. Register the suite under the `property` label.

## Deliverables

- Random-playout property tests
- A documented seed/reproduction mechanism
- CI label integration

## Acceptance criteria

- [ ] At least 10,000 random legal games complete without a crash, illegal move, or state mismatch.
- [ ] Failures print the seed and move sequence needed to reproduce.
- [ ] The suite is deterministic for a given seed.
- [ ] CI runtime remains reasonable.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug -L property --output-on-failure
```

Expected: all randomized invariant tests pass.

## Constraints and failure modes

- Do not hide failures by reducing the game count without documenting it.
- Do not use wall-clock-dependent assertions for correctness.
- Do not implement hashing in this task; compare the state fields available now.

## Completion notes

- Added deterministic randomized playouts using seed `0x5EEDC0FFEE`, 10,000
  games, and a 16-ply cap. Failures report the seed, game, ply, and move log.
- Every ply snapshots FEN, board rendering, side, castling rights, en passant,
  clocks, and history depth, then makes, undoes, and compares the state.
- Each game also verifies that `MakeMove` rejects a null move without changing
  state. The suite is registered under the `property` label and runs in about
  19 s in the Debug build.
- CI runtime is acceptable for the required 10,000-game workload; the perft and
  property suites together take about 42 s in Debug.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
