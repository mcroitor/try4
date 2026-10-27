# Task 034 — Add advanced search pruning with measured SPRT gains

| Field | Value |
|---|---|
| **ID** | 034 |
| **File** | `task_000000000000000000000000000034.md` |
| **Stage** | 7 — Performance and strength (optional) |
| **Depends on** | 032 |
| **Estimate** | 2 days |
| **Priority** | P2 |
| **Status** | Ready |

## Objective

Improve search efficiency and strength with null-move pruning, late move
reductions, aspiration windows, futility pruning, and SEE, landing each
change only with measurement.

## Context

Optional Stage 7 work. The engine is already playable and stable. Add
techniques one at a time, verify tactics/perft, and use the self-play
harness to measure each change.

## Scope

### In scope

- Implement selected advanced pruning techniques one at a time.
- Add SEE for capture ordering/pruning.
- Run tactical and perft regressions after each change.
- Measure Elo and NPS with SPRT or a documented fixed-game protocol.

### Out of scope

- NNUE or machine-learned evaluation.
- Opening book/tablebases; task 035.
- Multithreading; task 035.

## Requirements

1. Start with the technique best supported by profiling; do not stack multiple unmeasured changes.
2. Null-move pruning must be disabled in check and in likely zugzwang endgames.
3. LMR must not reduce tactical moves without verification.
4. SEE must be tested against known capture sequences.
5. For each change, record tactical pass rate, perft status, NPS, and SPRT/fixed-game result.
6. Revert any change that fails correctness or does not show a measurable benefit.

## Deliverables

- Advanced search implementation
- SEE
- Per-change measurement reports

## Acceptance criteria

- [ ] All perft and tactical tests remain green.
- [ ] Each landed change has a recorded measurement.
- [ ] No stability regression in self-play.
- [ ] The final configuration is documented.

## Validation

```sh
cmake --build build/cmake --config Release
ctest --test-dir build/cmake -C Release --output-on-failure
# Run the documented SPRT/self-play command for the changed build.
```

Expected: correctness suites pass and the measurement report shows the
effect of the change.

## Constraints and failure modes

- Do not land unmeasured pruning.
- Do not tune against the tactical suite.
- Do not change evaluation and search in the same measurement.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
