# Task 017 — Add the perft harness and reference regression suite

| Field | Value |
|---|---|
| **ID** | 017 |
| **File** | `task_000000000000000000000000000017.md` |
| **Stage** | 2 — Legal move generation |
| **Depends on** | 015, 016 |
| **Estimate** | 1.5 days |
| **Priority** | P0 |
| **Status** | Done |

## Objective

Prove move generation and make/unmake with perft against the six standard
reference positions, and record a performance baseline.

## Context

Perft is the hard correctness gate before search. The reference counts are
listed in `docs/ARCHITECTURE.md` §5.2. Default CI must stay fast; deep
runs can be a separate script or workflow.

## Scope

### In scope

- Implement `Perft(Position&, int depth)`.
- Add a perft executable/bench target and/or test entry point.
- Add the six reference positions and expected counts.
- Run depths 1–4 in CI and provide a script for depths 5–6.
- Record NPS baseline with hardware/compiler notes.

### Out of scope

- Changing move-generation rules to match counts.
- Search benchmarks.
- Running depth 6 in default pull-request CI.

## Requirements

1. Count leaf nodes at the requested depth; optionally use bulk counting at depth 1, but verify it against the straightforward implementation.
2. Add the start position, Kiwipete, and positions 3–6 with exact FENs and counts from the architecture document.
3. Register perft tests with the `perft` label.
4. CI runs depths 1–4 for all six positions.
5. Add a documented command or script for depths 5–6.
6. Record nodes/second for at least the start position and Kiwipete at a fixed depth, including compiler/build type and hardware.

## Deliverables

- Perft implementation
- Reference data and tests
- Deep-perft script or workflow
- Performance baseline note

## Acceptance criteria

- [ ] Perft matches all six reference positions at depths 1–4.
- [ ] The implementation is free of undefined behavior and does not mutate the position after completion.
- [ ] The `perft` test label passes in CI.
- [ ] A deep-run command is documented and reproduces depths 5–6.
- [ ] The NPS baseline is recorded.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug -L perft --output-on-failure
```

Expected: exact counts for all six positions at depths 1–4.

## Constraints and failure modes

- If a count mismatches, fix the root cause; never special-case a position.
- Do not run depth 6 in default CI.
- Do not add search code to the perft target.

## Completion notes

- Added `chess::Perft(Position&, int)` with bulk counting at depth 1 and a
  make/unmake bulk-count verification test.
- Added six reference positions and depths 1–6 counts in
  `tests/data/perft/reference.tsv`; `try4_perft` covers depths 1–4 under the
  `perft` label and passes on all six positions.
- Added `try4_perft_bench` and `scripts/perft_deep.ps1`; depth 5 was run for all
  six positions and matched. Depth 6 remains a documented deep run.
- Recorded the Release NPS baseline and hardware/compiler details in
  `docs/PERFORMANCE.md`. Debug perft suite runtime is about 22.5 s.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
