# Task 028 — Build the self-play/SPRT harness and establish an evaluation baseline

| Field | Value |
|---|---|
| **ID** | 028 |
| **File** | `task_000000000000000000000000000028.md` |
| **Stage** | 5 — Evaluation |
| **Depends on** | 023, 024, 027 |
| **Estimate** | 2 days |
| **Priority** | P1 |
| **Status** | Done |

## Objective

Measure evaluation changes with reproducible self-play and document a
baseline before any tuning or release claims.

## Context

The engine can search, evaluate, and manage time. Strength work needs a
harness with fixed conditions, color swapping, a varied opening set, and
recorded results. This task produces the baseline used by later tuning.

## Scope

### In scope

- Build a self-play harness using the search API.
- Support fixed nodes or fixed time and color swapping.
- Record PGN or structured results and compute Elo with error margins.
- Run a material-only vs full-evaluation baseline.
- Document the protocol and results.

### Out of scope

- UCI integration; tasks 029–032.
- Automated weight tuning; optional follow-up.
- Claiming a specific Elo without measurement.

## Requirements

1. Use a small, varied opening set to avoid identical games; alternate colors.
2. Make the time control, node limit, hardware, compiler, and build type explicit.
3. Store results and PGN/structured logs under a documented directory.
4. Compute win/loss/draw rates and an Elo difference with an error margin; use SPRT if implemented.
5. Run enough games for a meaningful result; document the sample size.
6. Compare full evaluation against a material-only baseline; report the result even if it is neutral or negative.

## Deliverables

- Self-play harness
- Opening set
- Baseline report
- Reproduction instructions

## Acceptance criteria

- [ ] The harness runs reproducibly with recorded settings.
- [ ] The baseline report includes games, W/D/L, Elo difference, and error margin.
- [ ] Results are stored in the repository or a documented artifact location.
- [ ] No tactical or perft regression.

## Validation

```sh
cmake --build build/cmake --config Release
# Run the documented self-play command with the recorded settings.
```

Expected: the harness produces the same result summary for the same seed
and settings.

## Constraints and failure modes

- Do not cherry-pick favorable games.
- Do not claim strength without a recorded sample size and conditions.
- Do not use the tactical suite as a strength benchmark.

## Completion notes

- Added the `try4_selfplay` harness and a `EvaluationMode` switch on
  `SearchLimits` so the production evaluator can be compared with
  `EvaluateMaterial` using identical search code.
- Added 24 varied opening sequences, each played with colors reversed (48
  games in the recorded baseline).
- The harness records `config.txt`, `games.csv`, `games.pgn`, `summary.txt`,
  and `summary.json`, including per-game node counts, termination reasons,
  color splits, Elo with a normal-approximation 95% margin, and an approximate
  score-normal SPRT diagnostic.
- Ran the baseline on Release, fixed 5,000 nodes per move, 160-ply maximum,
  16 MB table per searcher. Full evaluation scored 42/5/1 (92.71%, +441.72
  Elo, ±149.77) against material-only. All games, including the loss, are in
  the recorded artifacts.
- Verified opening legality and byte-identical output for a repeated
  two-opening run with the same fixed-node settings.

## Completion report

**Status:** done

**Files changed:**
- `include/chess/search.hpp` — `EvaluationMode` and the mode selector on `SearchLimits`.
- `src/core/search.cpp` — full/material leaf evaluation selection.
- `bench/selfplay.cpp` — self-play harness, opening replay, result/PGN/CSV/JSON output, Elo, and SPRT diagnostic.
- `CMakeLists.txt` — `try4_selfplay` target and build metadata definitions.
- `docs/selfplay/openings.txt` — 24-opening UCI opening set.
- `docs/selfplay/README.md` — protocol, usage, output formats, Elo/SPRT definitions, and limitations.
- `docs/selfplay/BASELINE.md` — recorded baseline report and reproduction command.
- `docs/selfplay/results/baseline-2026-10-24/` — config, summary, CSV, and PGN artifacts.
- `README.md`, `docs/HARDENING.md` — status and sanitizer record updates.
- `tasks/task_028.md` — completion notes and report.

**Commands run and results:**
- `cmake --build build/cmake --config Debug` — pass.
- `ctest --test-dir build/cmake -C Debug --output-on-failure` — 17/17 pass (73.53 s).
- `cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DTRY4_BUILD_TESTS=OFF -DTRY4_WARNINGS_AS_ERRORS=ON` — configure pass.
- `cmake --build build/release --config Release` — pass.
- Release baseline command from `docs/selfplay/BASELINE.md` — 48 games, 42/5/1, 92.71%, +441.72 Elo, ±149.77, 376.03 s.
- `try4_selfplay --validate-openings` — 24/24 openings valid.
- Repeated two-opening fixed-node runs — all output files byte-identical.
- WSL2 ASan/UBSan full suite — 17/17 pass (183.90 s), no reports.

**Acceptance criteria:**
- Harness runs reproducibly with recorded settings — pass (fixed nodes; recorded config; repeated output identical).
- Baseline report includes games, W/D/L, Elo difference, and error margin — pass.
- Results stored in the repository under `docs/selfplay/results/` — pass.
- No tactical or perft regression — pass (full Debug and ASan suites; tactical 7/7; perft reference tests pass).

**Risks or follow-ups:** The baseline is a 48-game, 5,000-node match against one material-only opponent; the Elo margin is wide and the approximate SPRT diagnostic did not reach a stopping boundary. Fixed-time mode is supported but not bit-reproducible. No weights were tuned against the tactical corpus. Next action: task 029 (UCI adapter skeleton).

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
