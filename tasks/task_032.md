# Task 032 — Validate GUI play and 1,000-game stability; write UCI usage docs

| Field | Value |
|---|---|
| **ID** | 032 |
| **File** | `task_000000000000000000000000000032.md` |
| **Stage** | 6 — UCI and playable engine |
| **Depends on** | 031, 028 |
| **Estimate** | 1.5 days |
| **Priority** | P1 |
| **Status** | Done |

## Objective

Prove the engine is playable in a real GUI and stable across a large
self-play sample, then document actual usage and limitations.

## Context

The UCI implementation is complete. This task is the Stage 6 gate: run it
in CuteChess or Arena, complete games, and run 1,000 self-play games at a
short time control. Record failures precisely.

## Scope

### In scope

- Test the engine manually in CuteChess or Arena.
- Run 1,000 self-play games at a documented short time control.
- Log illegal moves, hangs, crashes, and time forfeits.
- Document UCI usage, options, and known limitations.

### Out of scope

- Fixing deep strength issues; report them as follow-ups.
- Stage 7 performance work.
- GUI development.

## Requirements

1. Verify the GUI can start the engine, set options, start a game, and finish it.
2. Run 1,000 games with the documented time control and build; record W/D/L and any failures.
3. Capture engine logs for any illegal move, hang, or forfeit.
4. Document build, `try4-uci` invocation, supported commands/options, time controls, and limitations.
5. Update `README.md` with the UCI usage section.
6. If a GUI is unavailable, run the equivalent scripted match and state exactly what was not verified.

## Deliverables

- GUI test report
- 1,000-game stability report
- UCI usage documentation
- Updated README

## Acceptance criteria

- [x] A complete GUI game is played without illegal moves or crashes.
- [x] 1,000 self-play games complete without crashes, hangs, or illegal moves.
- [x] No time forfeits occur in the documented test control.
- [x] The docs accurately describe supported commands/options and limitations.

## Validation

```sh
cmake --build build/cmake --config Release
# Launch build/cmake/Release/try4-uci (or the generator equivalent) in CuteChess/Arena.
# Run the documented 1,000-game self-play script and archive the report.
```

Expected: a completed GUI game, a clean 1,000-game report, and no
unverified claims in the docs.

## Constraints and failure modes

- Do not claim GUI compatibility that was not tested.
- Do not hide time forfeits or illegal moves; report them with logs.
- Do not conflate this stability check with a strength claim.

## Completion report

### 2026-10-26 completion

**Status:** Done.

- The owner reported complete, stable CuteChess play. No GUI transcript was
  archived, so this remains an owner report rather than a captured GUI log.
- Archived 1,000-game UCI protocol stability run:
  [`docs/stability/uci-1000/`](../docs/stability/uci-1000/). Two real UCI
  processes, 1000 ms + 50 ms, `movestogo 200`, max 120 plies, four workers,
  every move replayed through the CLI legal-move judge.
- Result: 1000/1000 games in 1290.4 s; W/D/L 248/488/264; 99,293 plies;
  zero illegal moves, crashes, hangs, time forfeits, deadline violations or
  unsupported-command diagnostics.
- Maximum move wait 67.745 ms; maximum `stop` latency 0.516 ms; maximum
  `isready` latency 4.776 ms.
- UCI usage/options/limitations are documented in `docs/UCI.md` and the README.
- This is identical-engine stability evidence, not a strength or GUI-latency
  claim.

Files: `docs/UCI.md`, `docs/stability/uci-1000/*`, `scripts/uci_stability.py`,
`tests/README.md`, `README.md`, `docs/RELEASE_VALIDATION.md`.

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
