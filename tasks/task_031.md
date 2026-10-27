# Task 031 — Add UCI options, detailed info output, and conformance tests

| Field | Value |
|---|---|
| **ID** | 031 |
| **File** | `task_000000000000000000000000000031.md` |
| **Stage** | 6 — UCI and playable engine |
| **Depends on** | 030 |
| **Estimate** | 1.5 days |
| **Priority** | P1 |
| **Status** | Ready |

## Objective

Complete the UCI surface with configurable hash/move overhead and
parseable search information, backed by conformance tests.

## Context

The engine can play through UCI. This task exposes the supported options,
formats `info` lines for GUIs, and verifies the protocol with scripted
tests.

## Scope

### In scope

- Add `Hash` and `Move Overhead` options.
- Implement `setoption` before and after `ucinewgame`.
- Emit detailed `info depth seldepth score cp/mate nodes nps time pv` lines.
- Add UCI conformance/integration tests.

### Out of scope

- Ponder, Chess960, or multi-threaded options.
- GUI validation; task 032.
- Strength tuning.

## Requirements

1. `Hash` controls TT size within documented bounds; validate and report invalid values.
2. `Move Overhead` adjusts time allocation safely.
3. If `Threads` is exposed, keep the only supported value at 1 and report it; otherwise omit it.
4. `info` lines must be well-formed and use the correct mate/centipawn score convention.
5. `bestmove` is emitted exactly once per search.
6. Add scripted tests for handshake, option setting, startpos/FEN, depth/movetime, stop, quit, and malformed commands.
7. Register UCI tests under the `uci` label.

## Deliverables

- UCI options
- Detailed info output
- Conformance test suite
- Updated usage notes for task 032

## Acceptance criteria

- [ ] All scripted UCI tests pass.
- [ ] Options can be set before and after `ucinewgame` without crashing.
- [ ] `info` lines are parseable and scores use the documented convention.
- [ ] `bestmove` is emitted once and is legal.
- [ ] All existing tests pass.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug -L uci --output-on-failure
```

Expected: full UCI integration suite passes.

## Constraints and failure modes

- Do not claim support for options that are not implemented.
- Do not emit malformed or duplicate `bestmove` lines.
- Do not add threading options before multithreading exists.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
