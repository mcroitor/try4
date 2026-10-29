# Task 029 â€” Implement the UCI adapter skeleton and handshake

| Field | Value |
|---|---|
| **ID** | 029 |
| **File** | `task_000000000000000000000000000029.md` |
| **Stage** | 6 â€” UCI and playable engine |
| **Depends on** | 024, 025 |
| **Estimate** | 1 day |
| **Priority** | P1 |
| **Status** | Done |

## Objective

Add a UCI-facing executable that performs the handshake and command loop
without embedding chess rules in the adapter.

## Context

UCI is the external contract. The adapter parses commands, owns the
session, and delegates to the core. Search wiring and `go` are completed in
task 030; this task establishes the process and handshake.

## Scope

### In scope

- Add a `UciSession` adapter with input/output streams.
- Implement `uci`, `isready`, `quit`, and `ucinewgame`.
- Add a `setoption` dispatch stub for later options.
- Add a `try4-uci` executable target.
- Add scripted handshake tests.

### Out of scope

- Search execution and `go`; task 030.
- Options and `info` formatting; task 031.
- GUI testing; task 032.

## Requirements

1. `uci` emits `id name`, `id author`, available options (initially none), and `uciok`.
2. `isready` emits `readyok` immediately.
3. `quit` exits the process cleanly.
4. `ucinewgame` resets session state without crashing.
5. Unknown commands are ignored or logged; they must not terminate the process.
6. Keep parsing and formatting in the adapter; do not duplicate chess rules.
7. Add scripted tests that feed commands through an input stream and assert the output lines.

## Deliverables

- `UciSession` adapter
- `try4-uci` executable
- Handshake integration tests

## Acceptance criteria

- [ ] The handshake produces the required `uciok` response.
- [ ] `isready` works before and after `ucinewgame`.
- [ ] `quit` terminates cleanly.
- [ ] Malformed/unknown input does not crash.
- [ ] All existing tests pass.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug -L uci --output-on-failure
printf 'uci\nisready\nquit\n' | ./build/cmake/try4-uci
```

Expected: `id`, `uciok`, and `readyok` lines followed by a clean exit.

## Constraints and failure modes

- Do not implement search or `go` in this task.
- Do not put board logic in the UCI parser.
- Do not add a GUI dependency.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
