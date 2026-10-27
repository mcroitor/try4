# Task 027 — Add positional evaluation terms

| Field | Value |
|---|---|
| **ID** | 027 |
| **File** | `task_000000000000000000000000000027.md` |
| **Stage** | 5 — Evaluation |
| **Depends on** | 026 |
| **Estimate** | 1.5 days |
| **Priority** | P1 |
| **Status** | Done |

## Objective

Add the positional features that make the engine play coherent chess:
pawn structure, mobility, bishop pair, rook files, king safety, and tempo.

## Context

Material and PSTs are in place. This task adds interpretable terms with
tunable constants and isolated tests. Avoid double counting and keep the
evaluation symmetric.

## Scope

### In scope

- Implement pawn structure terms: doubled, isolated, and passed pawns.
- Implement mobility, bishop pair, rook on open/semi-open files, king safety, and tempo.
- Add tests for each term and overall symmetry.
- Verify no tactical regression.

### Out of scope

- Automated weight tuning; task 028.
- NNUE.
- Search changes.

## Requirements

1. Pawn structure: penalize doubled and isolated pawns; reward passed pawns with a rank-scaled bonus.
2. Mobility: count legal pseudo-moves for bishops/rooks/queens or a documented approximation; keep it cheap.
3. Bishop pair: bonus when both bishops remain.
4. Rook files: bonus on open and semi-open files.
5. King safety: pawn shield and attacker count in the middlegame; reduce the term in the endgame.
6. Tempo: a small side-to-move bonus, applied consistently.
7. Add isolated tests for each term and a color-mirror symmetry test for the full evaluation.
8. Run the tactical suite to confirm no regression.

## Deliverables

- Positional evaluation terms
- Term-level tests
- Evaluation documentation

## Acceptance criteria

- [ ] Each positional term has a test that demonstrates its sign and boundary behavior.
- [ ] Full evaluation remains symmetric under color swap.
- [ ] The tactical suite does not regress.
- [ ] The terms are documented enough for later tuning.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug --output-on-failure
ctest --test-dir build/cmake -C Debug -L tactical --output-on-failure
```

Expected: evaluation and tactical suites pass.

## Constraints and failure modes

- Keep terms simple and measurable; do not add speculative complexity.
- Do not tune constants without the harness from task 028.
- Avoid duplicate counting of the same feature.

## Completion notes

- Added interpretable, centralized positional terms to the evaluator:
  doubled/isolated/passed pawns, sliding-piece mobility, bishop pair,
  open/semi-open rook files, king safety (pawn shield and king-zone attacker
  count), and a flat side-to-move tempo bonus.
- Extended `EvaluationBreakdown` with per-term middlegame/endgame fields and
  tempo. All fields remain side-to-move relative; `score` is
  `taper(middlegame, endgame) + tempo`.
- Mobility is a documented pseudo-mobility approximation: bishops, rooks, and
  queens count destination squares including captures, stopping at blockers,
  without pin/check legality filtering. Knight/pawn mobility is left to
  material and PSTs.
- King safety applies only when the king is on its relative first or second
  rank. Endgame shield and attacker weights are smaller by construction.
- Added ten evaluation test cases covering every term's sign and boundary,
  plus a rich color-swapped/rank-mirrored symmetry check and the same-side
  color-swap relation.
- Updated tactical, sanitizer, test-inventory, and evaluation documentation.
  The tactical best moves are unchanged; the richer score reduces Kiwipete
  nodes and the corpus runtime.

## Completion report

**Status:** done

**Files changed:**
- `include/chess/evaluation.hpp` — positional term breakdown fields and tempo constant.
- `src/core/evaluation.cpp` — pawn structure, mobility, bishop pair, rook files, king safety, tempo, and integration into the tapered score.
- `tests/evaluation_tests.cpp` — ten term/symmetry/phase tests.
- `docs/EVALUATION.md` — scale, term constants, phase formula, symmetry, and test coverage.
- `docs/TACTICAL.md`, `docs/HARDENING.md` — updated baselines and sanitizer record.
- `README.md`, `tests/README.md` — current status and test inventory.

**Commands and results:**
- `cmake --build build/cmake --config Debug` with `TRY4_WARNINGS_AS_ERRORS=ON` — pass.
- `./build/cmake/try4_evaluation.exe --durations yes` — 10 cases, 180 assertions, pass.
- `ctest --test-dir build/cmake -C Debug --output-on-failure` — 17/17 pass (61.55 s).
- `ctest --test-dir build/cmake -C Debug -L tactical --output-on-failure` — 7/7 pass (9.13 s).
- WSL2 ASan/UBSan full suite — 17/17 pass (182.77 s), no reports.

**Acceptance criteria:**
- Each positional term has a sign/boundary test — pass (pawn structure, mobility, bishop pair, rook files, king safety, tempo).
- Full evaluation remains symmetric under color swap — pass (rich rank-mirrored position plus same-side color-swap relation).
- Tactical suite does not regress — pass (7/7, same best moves).
- Terms documented for tuning — pass (`docs/EVALUATION.md` lists constants, approximations, and formulas).

**Risks or follow-ups:** The constants are deliberately untuned and task 028 should establish a measurable baseline before changing them. Tempo means a perfectly symmetric board now scores +10 rather than zero. Mobility and king-attacker counting are approximations rather than legal-move analysis. Next action: task 028.

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
