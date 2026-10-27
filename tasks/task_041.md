# Task 041 — Parameterize evaluation with immutable settings

| Field | Value |
|---|---|
| ID | 041 |
| Stage | Engine configuration |
| Depends on | 040 |
| Estimate | 1.5–2 days |
| Priority | P1 |
| Status | Done |

## Objective

Replace fixed evaluation tuning constants with an explicitly owned settings
snapshot while reproducing existing default scores and search behavior.

## Scope

In scope: `evaluation.hpp/.cpp`, search parameter ownership, evaluator tests,
and default-equivalence verification. Parser/startup wiring belongs to 042/043.
PST cells are in scope only if approved in 040. No tuning or strength claims.

## Requirements

1. Define one typed `EvaluationParameters` (or consistent equivalent) and one
   default factory; preserve existing call sites via default-compatible APIs.
2. Cover MG/EG pawn, knight, bishop, rook, queen values; mobility; bishop pair;
   rook open/semi-open files; doubled/isolated pawns; passed-pawn rank arrays;
   shield-one/two/missing and attacker penalties; tempo; approved PST controls.
   Inventory all constants in evaluation.cpp rather than assuming this list is
   exhaustive. Phase rules stay fixed unless 040 explicitly approves changing them.
3. Preserve centipawn units, color mirroring, tapering, breakdown accounting,
   and zero king material. Specify whether penalty settings are nonnegative
   magnitudes subtracted by evaluation (recommended).
4. Make material-only evaluation and `PieceValue`/move ordering consistent by
   documented policy. Keep the A/B material baseline meaningful; never silently
   apply half of the configured values to one search path.
5. Each search observes an immutable snapshot for its lifetime; no mutable global
   settings and no cross-engine-instance interference.
6. Invalidate TT scores whenever evaluation settings or evaluation mode change.
   Test reusing a Searcher with differing snapshots against fresh Searchers.
7. Specify bounds for every field, use safe intermediate arithmetic, and keep
   static evaluation strictly outside the mate-score band. Validate worst-case
   allowed combinations; avoid overflow even with extreme FENs and weights.

## Deliverables

- Typed default parameters and explicit evaluator/search integration.
- Meaningful extensions to evaluation/search tests.
- Parameter inventory with names, units, defaults and ranges for task 042.

## Test cases and expected results

| Case | Expected |
|---|---|
| Current corpus with default snapshot | Existing scores/breakdowns and deterministic search results unchanged |
| Change one material value | Correct MG/EG and material-baseline delta under documented policy |
| Change one positional term | Targeted contribution changes; unrelated breakdown terms do not |
| Mirrored colors/side to move | Existing symmetry conventions preserved |
| Two engines with different parameters | No shared-state leakage |
| Reused TT across parameter changes | Same score as a fresh searcher; stale scores rejected |
| Boundary/worst-case parameters | No overflow or false mate-band scores |

## Acceptance criteria and validation

- [x] Default compatibility, per-term effects, snapshot isolation, and TT tests pass.
- [x] Every tunable parameter has a type/default/range and documented semantics.
- [x] Full existing suite passes without warnings.

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug -R "try4_(evaluation|search|search_time|tactical)" --output-on-failure
ctest --test-dir build/cmake -C Debug --output-on-failure
```

## Failure modes and completion report

Watch for sign reversal, PST asymmetry, hidden hard-coded constants, stale TT
entries and lifetime errors. Report files, commands/results, score-equivalence
evidence and deviations. Do not claim stronger play from parameterization alone.

### Completion — 2026-10-26

**Status:** Done.

- `EvaluationParameters` owns material MG/EG, PST scales, positional scalars,
  passed-pawn rank arrays, tempo, phase rules and bounds. `DefaultEvaluationParameters()`
  reproduces legacy values.
- Every search copies an immutable snapshot; no mutable global settings.
- TT keys/validity include evaluation mode/parameters and tactical policy;
  reusing a searcher after a change matches a fresh searcher.
- Default-vs-explicit-default fixed-depth searches matched score, depth, nodes,
  PV and bestmove on four positions. A +100 pawn-value change moved the real UCI
  score by +100.
- Extreme/combined parameters remain bounded below the mate band; evaluator,
  search and full suites pass with warnings-as-errors.
- Test counts and commands: `docs/CONFIGURATION_VALIDATION.md`.

Files: `include/chess/evaluation.hpp`, `src/core/evaluation.cpp`,
`include/chess/search.hpp`, `src/core/search.cpp`, `tests/evaluation_tests.cpp`,
`tests/configuration_tests.cpp`, `tests/search_tests.cpp`.
