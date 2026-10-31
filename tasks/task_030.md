# Task 030 â€” Implement UCI position/go/stop/quit with asynchronous search

| Field | Value |
|---|---|
| **ID** | 030 |
| **File** | `task_000000000000000000000000000030.md` |
| **Stage** | 6 â€” UCI and playable engine |
| **Depends on** | 029 |
| **Estimate** | 2 days |
| **Priority** | P1 |
| **Status** | Done |

## Objective

Make the UCI engine playable: load positions, start and stop searches on a
worker thread, and emit legal best moves without races or hangs.

## Context

The handshake exists. This task adds the search lifecycle and the commands
a GUI uses to play a game. Keep one UCI thread and one search worker; search
a copy of the position.

## Scope

### In scope

- Implement `position startpos|fen [moves ...]`.
- Implement `go` with depth, movetime, clock, and infinite limits.
- Implement `stop` and `quit` during search.
- Run search on a worker thread with an atomic stop flag.
- Emit `bestmove` and basic `info` lines.

### Out of scope

- Full option handling and advanced `info`; task 031.
- Ponder mode.
- UCI Chess960.

## Requirements

1. `position` must validate FEN and every move; invalid input must be reported and must not corrupt the session.
2. `go` maps supported limits to `SearchLimits`; unknown or missing fields use documented defaults.
3. Search runs on a copy of the current position; the UCI thread never mutates that copy.
4. `stop` sets the atomic flag and waits for the worker to emit `bestmove`; do not detach threads.
5. `quit` stops and joins the worker before exiting.
6. Never start a new search before the previous worker is joined.
7. Emit a legal `bestmove`; if no legal move exists, emit a documented terminal response.
8. Add scripted tests for startpos, FEN, move lists, depth/movetime, stop during search, and quit during search.

## Deliverables

- Position/go/stop/quit handling
- Asynchronous search worker
- Basic info/bestmove output
- Integration tests

## Acceptance criteria

- [ ] A scripted session can load startpos, play moves, search, and receive a legal best move.
- [ ] `stop` produces `bestmove` promptly and does not hang.
- [ ] `quit` during search exits cleanly.
- [ ] No data races are observed in sanitizer/thread tests.
- [ ] All existing tests pass.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug -L uci --output-on-failure
printf 'uci\nisready\nposition startpos\ngo depth 4\nquit\n' | ./build/cmake/try4-uci
```

Expected: handshake, a legal `bestmove`, and clean exit. For stop testing,
use a longer search and send `stop` from the test harness.

## Constraints and failure modes

- Do not use detached threads or shared mutable positions.
- Do not implement ponder in this task.
- Do not let a malformed command crash the process.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
