# Task 026 — Implement material, piece-square, and tapered evaluation

| Field | Value |
|---|---|
| **ID** | 026 |
| **File** | `task_000000000000000000000000000026.md` |
| **Stage** | 5 — Evaluation |
| **Depends on** | 022 |
| **Estimate** | 1.5 days |
| **Priority** | P1 |
| **Status** | Done |

## Objective

Replace the temporary material-only evaluator with a real tapered
evaluation based on material and piece-square tables.

## Context

Search currently uses a temporary material evaluator. This task establishes
the evaluation scale, middlegame/endgame tables, and phase interpolation.
Positional terms are added in task 027.

## Scope

### In scope

- Implement material values and piece-square tables.
- Implement middlegame/endgame phase and tapered scoring.
- Replace the temporary evaluator in search.
- Add evaluation symmetry and material tests.

### Out of scope

- Pawn structure, mobility, king safety, and other positional terms; task 027.
- Weight tuning; task 028.
- NNUE or machine-learned evaluation.

## Requirements

1. Use a documented centipawn scale and standard material values.
2. Provide separate middlegame and endgame PSTs for every piece.
3. Compute a phase value from remaining material and interpolate between middlegame and endgame scores.
4. Evaluation must be from the side-to-move perspective for negamax.
5. Ensure color mirroring is correct: a position and its color-swapped mirror evaluate symmetrically.
6. Add tests: equal material and mirrored position evaluates to zero; a queen advantage is positive for the stronger side; PST color symmetry.

## Deliverables

- Evaluator implementation
- PST data
- Evaluation tests
- Search integration

## Acceptance criteria

- [ ] Evaluation is symmetric under color swap and board mirroring.
- [ ] Material imbalances produce the expected sign.
- [ ] The tapered score transitions correctly between phases.
- [ ] Search still returns legal, deterministic moves.
- [ ] All existing tests pass.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug --output-on-failure
```

Expected: evaluation and search tests pass.

## Constraints and failure modes

- Do not add positional terms in this task.
- Do not tune weights against the tactical suite.
- Keep the evaluation free of side effects and global mutable state.

## Completion notes

- Replaced the search leaf/quiescence evaluator with `chess::Evaluate`, a
  side-to-move tapered score built from separate middlegame/endgame material and
  piece-square tables for all six pieces.
- Material scale: P100/N320/B330/R500/Q900 in the middlegame and
  P120/N300/B320/R540/Q900 in the endgame. Phase weights are N/B=1, R=2, Q=4,
  capped at 24; the score is `(mg*phase + eg*(24-phase))/24`.
- Kept `EvaluateMaterial` and `PieceValue` as documented diagnostics/order
  baselines. The evaluator is pure, has no global mutable state, and uses a
  vertical rank mirror for Black.
- Added `EvaluateDetailed` for testable MG/EG/phase breakdowns and
  `tests/evaluation_tests.cpp` with five cases covering symmetry, signs, phase
  transitions, material values, and PST color/file symmetry.
- Updated the tactical baseline; all seven tactical cases keep the same best
  moves. The PST changes increase Kiwipete node count, so the tactical baseline
  runtime is now about 12 s for the full suite.
- Reduced the hard-deadline poll interval from 256 to 32 nodes after the full
  ASan/UBSan run exposed a 255 ms clock result against a 250 ms test ceiling.
  The sanitizer suite now passes with the 50 ms tolerance intact.

## Completion report

**Status:** done

**Files changed:**
- `include/chess/evaluation.hpp`, `src/core/evaluation.cpp` — tapered material, MG/EG PSTs, phase API, material baseline.
- `src/core/search.cpp` — switched leaf, quiescence, and fallback scores to `Evaluate`; hard-deadline poll reduced to 32 nodes.
- `tests/evaluation_tests.cpp`, `CMakeLists.txt` — five evaluation tests and the `unit` target.
- `docs/EVALUATION.md`, `docs/TACTICAL.md`, `docs/HARDENING.md`, `README.md`, `tests/README.md` — evaluation design, updated baselines, counts, and sanitizer record.
- `include/chess/search.hpp`, `docs/ARCHITECTURE.md`, `tasks/task_024.md` — poll interval and behavior documentation.

**Commands and results:**
- `cmake --build build/cmake --config Debug` with `TRY4_WARNINGS_AS_ERRORS=ON` — pass.
- `./build/cmake/try4_evaluation.exe` — 5 cases, 83 assertions, pass.
- `ctest --test-dir build/cmake -C Debug --output-on-failure` — 17/17 pass (64.87 s).
- WSL2 ASan/UBSan full suite — 17/17 pass (191.44 s), no reports.

**Acceptance criteria:**
- Symmetric under color swap and board mirroring — pass (initial/rook-symmetric positions, color-swapped mirror with and without side swap, file mirror, all PSTs).
- Material imbalances produce the expected sign — pass (queen advantage both perspectives and black-queen mirror).
- Tapered score transitions correctly — pass (phase 24, phase 0, phase 12, formula checks).
- Search returns legal, deterministic moves — pass (existing search and tactical suites).
- All existing tests pass — pass (17/17 Debug and sanitizer).

**Risks or follow-ups:** The PST values are a hand-authored baseline, not tuned; task 027 adds positional terms and task 028 measures strength changes. Fixed-depth tactical best moves remain unchanged, but node counts/timings are higher. Next action: task 027.

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
