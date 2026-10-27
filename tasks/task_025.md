# Task 025 — Create the tactical suite and deterministic search regression tests

| Field | Value |
|---|---|
| **ID** | 025 |
| **File** | `task_000000000000000000000000000025.md` |
| **Stage** | 4 — Search foundation |
| **Depends on** | 024 |
| **Estimate** | 1.5 days |
| **Priority** | P1 |
| **Status** | Done |

## Objective

Establish a curated tactical benchmark that catches search regressions
before UCI and evaluation work.

## Context

The tactical suite is the search correctness gate. It should cover mates,
winning captures, and stalemate avoidance, and run deterministically in CI.

## Scope

### In scope

- Add a curated tactical position suite.
- Test mate-in-1, mate-in-2, mate-in-3, winning captures, and stalemate avoidance.
- Add deterministic fixed-depth regression tests.
- Register the suite under the `tactical` label.

### Out of scope

- Strength measurement or SPRT; task 028.
- UCI integration.
- Overfitting to a small set of positions.

## Requirements

1. Store positions and expected outcomes in a source-controlled data file (FEN, best move or mate score, depth limit).
2. Include a mix of opening, middlegame, and endgame tactics; avoid duplicating the same motif.
3. Require mate-in-1/2 to pass at the documented depth; document any known exceptions for mate-in-3.
4. Add tests that a stalemate trap is not chosen when a winning move exists.
5. Run fixed-depth tests without wall-clock limits; timing behavior belongs to task 024.
6. Record the suite pass rate and any per-position runtime.

## Deliverables

- Tactical position corpus
- Tactical test runner
- Deterministic regression tests

## Acceptance criteria

- [ ] The curated mate-in-1/2 suite passes completely.
- [ ] The mate-in-3 suite meets its documented threshold.
- [ ] No test is flaky across repeated runs.
- [ ] All existing perft/property/search tests pass.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug -L tactical --output-on-failure
```

Expected: tactical suite passes at the documented depths.

## Constraints and failure modes

- Do not tune the engine to individual test positions.
- Do not use time-based limits for pass/fail.
- Do not silently remove failing positions; document and justify exceptions.

## Completion notes

- Added `tests/data/tactical/positions.tsv` with seven tab-separated cases
  spanning opening, middlegame, and endgame: mate-in-1, mate-in-2, mate-in-3,
  a hanging-queen capture, a verified stalemate-avoidance position, and two
  fixed-depth regressions.
- Added `tests/tactical_tests.cpp`, registered as `try4_tactical` with the
  `tactical` label. It parses the corpus, runs every case twice with fresh
  searchers, compares depth/score/nodes/best move, replays the PV, enforces
  `avoid` moves, and reports per-position runtime plus pass rate.
- Mate expectations are stored in plies (`mate:3` is a chess mate-in-2), so the
  documented depths match the alpha-beta horizon.
- Added `docs/TACTICAL.md` with the data contract, baseline timings, and the
  1/1 mate-in-3 threshold. No mate-in-3 exception is needed.
- Updated test inventories to 16 executables and 120 cases.

## Completion report

**Status:** done

**Files changed:**
- `tests/data/tactical/positions.tsv` — curated tactical corpus.
- `tests/tactical_tests.cpp`, `CMakeLists.txt` — runner, data path, and `tactical` label.
- `docs/TACTICAL.md`, `tests/README.md`, `README.md` — corpus contract, baseline, and inventory.

**Commands and results:**
- `cmake --build build/cmake --config Debug` — pass with `TRY4_WARNINGS_AS_ERRORS=ON`.
- `ctest --test-dir build/cmake -C Debug -L tactical --output-on-failure` — 1/1 pass (3.74 s).
- `ctest --test-dir build/cmake -C Debug --output-on-failure` — 16/16 pass (55.16 s).
- WSL2 ASan/UBSan full suite — 16/16 pass (165.92 s), no reports.

**Acceptance criteria:**
- Mate-in-1/2 suite — pass (2/2).
- Mate-in-3 threshold — pass (1/1; thresholds documented in `docs/TACTICAL.md`).
- No flakiness — pass (each corpus position searched twice with equal depth, score, node count, and best move).
- Existing perft/property/search tests — pass (full Debug suite 16/16).

**Risks or follow-ups:** Fixed-depth best-move baselines are intentionally sensitive to evaluation changes in tasks 026-028; update the corpus with review when behavior changes, rather than relaxing assertions. `regression-kiwipete` dominates runtime at about 1.1 s per search. Next action: task 026.

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
