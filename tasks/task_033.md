# Task 033 — Migrate move generation to bitboards behind the public API

| Field | Value |
|---|---|
| **ID** | 033 |
| **File** | `task_000000000000000000000000000033.md` |
| **Stage** | 7 — Performance and strength (optional) |
| **Depends on** | 032 |
| **Estimate** | 3 days |
| **Priority** | P2 |
| **Status** | Ready |

**Deferral note (2026-10, remaining-task chain pass).** This is the only task
left open in the 001-058 chain. It was not started in the late-October
strength pass (tasks 057, 058, 034, 048 and 035's close-out) because it is a
multi-day full representation rewrite (movegen, position, incremental state)
where a half-landed state is worse than a clean deferral. Two findings from
this pass raise its expected value and belong in its plan:
1. the SEE measurements (`tasks/task_034.md`) now provide a concrete
   re-land target: SEE pruning is implemented and switchable
   (`useStaticExchangePruning`), but its per-node cost is roughly double on
   0x88 board copies; bitboards drop that cost and the A/B should be re-run
   immediately after migration;
2. `scripts/smp_benchmark.py` and the PoC build give a ready NPS harness for
   the per-node-rate comparison.
Safety net stays as documented: the perft suite plus the fixed-depth/tactical
corpus exactness gates.

## Objective

Replace the 0x88 internals with bitboards for speed while keeping the
public Position/MoveGenerator API and all rule behavior unchanged.

## Context

This is optional Stage 7 work. Start only if profiling shows move
generation/search is the bottleneck or a strength target requires it.
The perft suite is the safety net.

## Scope

### In scope

- Add occupancy bitboards and attack tables.
- Implement sliding attacks with magic/PEXT or a documented fallback.
- Keep the public API and move semantics unchanged.
- Re-run perft and benchmarks; compare NPS and strength.

### Out of scope

- Changing evaluation or search heuristics in the same task.
- Adding new chess rules.
- Removing the 0x88 path until bitboards pass every test.

## Requirements

1. Keep bitboard indices consistent with `Square` (a1 = 0, h8 = 63).
2. Maintain occupancy incrementally in make/unmake.
3. Implement knight/king attack tables and pawn attack tables.
4. Implement slider attacks with magic bitboards or PEXT with a portable fallback; document the choice.
5. The public `Position`/`MoveGenerator` API must not expose bitboards.
6. Run the full perft suite at depths 1–5 and the property suite after the migration.
7. Benchmark before/after on identical hardware and build settings; record NPS.
8. If a rule mismatch appears, keep the 0x88 implementation until it is fixed.

## Deliverables

- Bitboard implementation
- Attack table generation
- Perft/benchmark comparison
- Migration notes

## Acceptance criteria

- [ ] All perft counts remain exact.
- [ ] All core/property/tactical tests pass.
- [ ] Measured NPS improves; the result is recorded.
- [ ] No public API change is required for callers.
- [ ] No rule regression is introduced.

## Validation

```sh
cmake --build build/cmake --config Release
ctest --test-dir build/cmake -C Release -L perft --output-on-failure
ctest --test-dir build/cmake -C Release --output-on-failure
# Run the documented before/after perft benchmark.
```

Expected: exact perft counts and a recorded performance comparison.

## Constraints and failure modes

- Do not combine this with search/eval tuning.
- Do not keep two divergent rule implementations indefinitely; remove the old path only after parity.
- Do not sacrifice correctness for NPS.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
