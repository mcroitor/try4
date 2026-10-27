# Task 020 — Implement SAN/UCI notation and an interactive CLI; retire the legacy core

| Field | Value |
|---|---|
| **ID** | 020 |
| **File** | `task_000000000000000000000000000020.md` |
| **Stage** | 3 — Complete game state and hardening |
| **Depends on** | 016, 019 |
| **Estimate** | 2 days |
| **Priority** | P1 |
| **Status** | Done |

## Objective

Provide human- and machine-readable move notation plus a CLI for inspecting
positions, playing moves, and running perft, then switch the project to the
new core.

## Context

The demo in `main.cpp` still uses the legacy `Board`/`Engine`. By this
stage the new core supports legal moves, results, hash, and draws. The CLI
becomes the manual debugging surface and proves the migration.

## Scope

### In scope

- Implement UCI move formatting/parsing and SAN formatting with disambiguation.
- Add an interactive CLI using the new core.
- Port the demo and remove the legacy core once parity is proven.
- Add notation and CLI tests.

### Out of scope

- The UCI protocol loop; task 029.
- Search or evaluation.
- PGN database features.

## Requirements

1. UCI notation: `e2e4`, `e7e8q`, castling as king move, and en passant as the target square.
2. SAN: piece letters, pawn captures, disambiguation by file/rank/both, promotion, check `+`, and mate `#`.
3. CLI commands: `startpos`, `fen <FEN>`, `board`, `moves`, `move <uci|san>`, `undo`, `perft <depth>`, `result`, `quit`.
4. The CLI must only accept legal moves and must report errors without crashing.
5. Replace the legacy `main.cpp` demo and remove `Board`/`Engine`/legacy `Move` only after the new path passes all tests.
6. Update CMake target sources and README links if needed.
7. Add tests for SAN disambiguation, check/mate suffixes, UCI parsing, and a scripted CLI session.

## Deliverables

- UCI/SAN notation implementation
- Interactive CLI
- Legacy core removal
- Tests for notation and CLI

## Acceptance criteria

- [ ] A scripted CLI session can load a FEN, show legal moves, play moves, undo, run perft, and report the result.
- [ ] SAN handles disambiguation and check/mate suffixes correctly for the test corpus.
- [ ] UCI move parsing rejects illegal or malformed moves.
- [ ] The legacy `Board`/`Engine` implementation is removed or clearly isolated with no duplicate game state.
- [ ] All core, FEN, perft, and property tests pass.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug --output-on-failure
printf 'startpos\nmoves\nmove e2e4\nboard\nundo\nresult\nquit\n' | ./build/cmake/try4
```

Expected: the CLI session completes and reports legal moves/results; adapt
the executable path to the generator. The CLI target may keep the name
`try4`; the UCI executable is added separately in task 029.

## Constraints and failure modes

- Do not implement the UCI network/protocol loop here.
- Do not keep two authoritative game states during the migration.
- Do not add GUI code.

## Completion notes

- Added `chess/notation.hpp`: `ParseUci`, `ToSan`, and `ParseSan`. UCI parsing
  returns canonical legal moves, including castling, en passant, and promotion.
  SAN covers piece letters, pawn captures, promotion, file/rank/both
  disambiguation, check `+`, and mate `#`.
- Added a testable `chess::Cli` command loop and the `try4` executable. Commands
  are `startpos`, `fen`, `board`, `moves`, `move`, `undo`, `perft`, `result`,
  `help`, and `quit`; illegal input is reported without terminating.
- Removed the legacy `Board`, `Engine`, `Piece`, `Move`, `defines.h`, old demo,
  and characterization target. The new core is the only game state.
- Added notation tests, in-process scripted CLI tests, and updated CMake and
  README/test documentation.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
