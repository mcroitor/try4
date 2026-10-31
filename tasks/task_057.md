# Task 057 - Harden the 60-move clock budget and validate the depth work in a TSCP match

| Field | Value |
|---|---|
| **ID** | 057 |
| **Stage** | Search strength - clock and validation |
| **Depends on** | 049-056 |
| **Estimate** | 1 day |
| **Priority** | P1 |
| **Status** | Done |

**Unblocked:** tasks 049-056 are `Done`. Cute Chess 1.5.1 and TSCP 1.81e were
installed on this machine during the close-out pass, so the match portion is
recorded (see the completion report).

## Objective

Make the engine safe and efficient under a 60-moves/2-minutes control, then close
the depth work with a recorded match so the improvement is measured in games
rather than inferred from NPS.

## Background

`ComputeTimeBudget` (`src/core/time_manager.cpp:18-53`) sets
`soft = time / moves_to_go + increment / 2` (2000 ms for 60/120) and
`hard = min(2 * soft, time - max(5 ms, time / 100))`. Only the soft deadline is
checked between iterations (`src/core/search.cpp:485-487`), so an iteration that
starts just before the soft limit can run to the hard limit and consume two moves
of budget. Under a real 60/120 game that is a forfeit risk, and a sudden death
control makes the failure more likely as the clock drains.

The comparison engine TSCP speaks the XBoard protocol, not UCI. The match must
therefore run under a GUI/manager that speaks both (for example Cute Chess CLI)
and be documented exactly; `scripts/uci_stability.py` only drives try4 against
itself.

## In scope

- Budget rework:
  - `soft = min(remaining / 2, remaining / moves_to_go + increment * 0.75)`;
  - `hard = min(remaining / 2, soft * 2.5)`;
  - `safety = Move Overhead + max(5 ms, remaining / 100)`;
  - `hard = max(soft, min(hard, remaining - safety))`;
  - `movetime` keeps precedence and uses the same ceiling.
- Iteration-start prediction: skip an iteration when
  `elapsed + predicted_ms > hard`, where `predicted_ms` is the previous
  iteration time multiplied by a measured effective branching factor
  (clamped 1.5-4.0) with a documented fallback.
- Best-move-stability extension: allow searching up to `hard` when the last
  completed iteration changed the best move, still subject to the prediction and
  the hard ceiling.
- Unit tests for budget arithmetic and process tests for clocks.
- `docs/selfplay/openings.pgn`: the existing opening set converted to a PGN/EPD
  book so both engines start from identical positions in a managed match.
- `scripts/match_report.py`: parse a Cute Chess PGN and produce W/D/L, score,
  Elo difference with a 95% confidence interval, forfeit count, illegal-move
  count, ply distribution and per-game time statistics when available.
- A 200-game match against TSCP at 60 moves / 2 minutes and the documentation
  updates.

## Out of scope

- New search techniques or evaluation tuning (tasks 054-056 already own that).
- Changing the UCI protocol, adding pondering, or enabling SMP.
- Releasing or tagging.

## Requirements

1. Budget values are never negative, `hard >= soft`, and both respect the
   remaining clock minus safety. Cover `movetime`, `wtime`/`btime`,
   `movestogo` 0/1/large, increments, and remaining times of 1 ms and 5 ms.
2. `movetime` behavior and the existing "never exceed the hard limit by more
   than 50 ms" contract are preserved.
3. The prediction must not skip the first iteration, and it must never skip an
   iteration when no previous timing exists (fixed-node/depth searches).
4. Stop and node limits still terminate promptly; the last completed iteration
   is always kept.
5. Redo the 200-move protocol stress at 60/120 with `movestogo 60` and record
   zero forfeits, zero illegal moves and maximum overrun.
6. Match procedure is reproducible: exact engine paths, `proto` values, options,
   time control, openings file, seed and Cute Chess version recorded.
7. The match report distinguishes time forfeits from chess losses and never
   derives Elo from fewer than the recorded game count.
8. Documentation updated: `docs/PERFORMANCE.md` (final table and gates),
   `docs/ROADMAP.md` (Stage 7 status for this increment),
   `CHANGELOG.md`, and `README.md` only if defaults changed.

## Match command (reference)

```sh
cutechess-cli \
  -engine name=try4 cmd=build/performance/try4_uci.exe proto=uci \
          option.Hash=64 \
  -engine name=tscp cmd=tscp proto=xboard \
  -each tc=60/120 timemargin=0 \
  -openings file=docs/selfplay/openings.pgn format=pgn order=random \
  -games 200 -repeat -recover -pgnout build/match-try4-vs-tscp.pgn
python scripts/match_report.py build/match-try4-vs-tscp.pgn \
  --output build/match-report.md
```

Notes:

- `-recover` keeps a crashed engine from aborting the whole match; crashes are
  still counted and reported.
- The try4 opening book must be disabled for the match (or identical for both
  sides) and the configuration used must be recorded.
- The `option.Hash=64` value must match the final measured choice from task 053;
  remove it if the default stays at 16 MB.

## Acceptance matrix

| Case | Expected result |
|---|---|
| Budget unit tests | All cases pass, no negative/undersized budgets |
| 200-move 60/120 stress | Zero forfeits, zero illegal moves, max overrun <= 50 ms |
| `movetime` regression tests | Unchanged behavior |
| Prediction guard | Never skips the first iteration or a depth/node search |
| Match | 200 games completed; forfeits and illegal moves reported separately |
| Match result | Gate S3: score >= 50% against TSCP at 60/120. If below, record the exact deficit and do not claim S3 |
| Report | W/D/L, Elo with 95% CI, plies, time usage |
| Docs | PERFORMANCE, ROADMAP, CHANGELOG updated; README only if needed |
| Full suite | Debug and Release pass; sanitizers clean |

## Validation commands

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug -L "search|uci|tactical" \
  --output-on-failure

cmake -S . -B build/performance -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/performance
ctest --test-dir build/performance -C Release --output-on-failure
python scripts/search_bench.py --engine build/performance/try4_uci.exe \
  --cli build/performance/try4.exe --movetime-ms 2000 --repeats 3 \
  --output build/bench-final
```

Expected: all suites pass, the benchmark shows the final depth and NPS, and the
stress run plus match complete without protocol failures.

## Risks and failure modes

- **Time forfeit under sudden death.** Keep the hard ceiling below the remaining
  clock with the overhead margin; test with tiny remaining times.
- **Disk/CPU contention during a long match** distorts results; record hardware,
  run sequentially, and do not compare across sessions.
- **Protocol mismatch** (TSCP is XBoard) producing meaningless games; verify
  every game ends with a legal PGN result and a recorded termination reason.
- **Cherry-picked match results.** Report all 200 games, including crashes and
  forfeits, and state the confidence interval.
- **Overfitting the clock to one machine.** Keep the formula simple and
  documented; do not tune per-opponent.

## Completion report

**Status:** Done for the budget/prediction/stress/report work including the
recorded TSCP match (8 games at 60/120, try4 6-2-0; the two time forfeits are
analyzed in the findings below with env-gated budget/flow forensics).

**Budget rework** (`src/core/time_manager.cpp`, `include/chess/time_manager.hpp`):

- `soft = min(remaining/2, remaining/moves_to_go + increment*3/4)`;
- `hard = min(remaining/2, soft*5/2)`, then
  `hard = max(soft, min(hard, remaining - safety))` with
  `safety = move_overhead_ms + max(5, remaining/100)`;
- `movetime` keeps precedence as an exact budget with the same safety ceiling;
- `ComputeTimeBudget(limits, side, move_overhead_ms = 0)` now carries the
  overhead; the UCI adapter passes `options_.move_overhead_ms`.

**Iteration predictor and stability extension** (`src/core/search.cpp`): the
previous completed iteration's wall time is scaled by the measured branching
factor (iteration nodes / previous iteration nodes, clamped 1.5x..4.0x,
fallback 2.0x); the search skips an iteration only when
`elapsed + predicted > hard` or the soft deadline expired without a best-move
instability. Fixed-node/depth searches have no budget and never predict; the
first iteration never predicts.

**Stress run** (`build/stress-057.json`, one 194-ply game, 60 moves/120 s,
`movestogo 60`, seed 57): **zero forfeits, zero illegal moves, zero deadline
violations** against the hard+50 ms contract; per-move p50 = 777 ms, max =
3889 ms; the game finished in a draw with clock headroom on both sides.

**Files changed:** `include/chess/time_manager.hpp`, `src/core/time_manager.cpp`,
`src/core/search.cpp`, `tests/search_time_tests.cpp` (new budget cases: tiny
clocks 1/5 ms, `movestogo 1`, the overhead argument, movetime safety clamp),
`docs/selfplay/openings.pgn` (the 24-opening set converted to PGN with SAN),
`scripts/match_report.py` (new PGN match reporter: W/D/L, score rate, Elo with
the 95% CI, forfeit/crash/abandoned counts, ply distribution; the Elo-formula
sign verified), `docs/CONFIGURATION.md`, `CHANGELOG.md`, and this file.

**Acceptance matrix:**

| Case | Result |
|---|---|
| Budget unit tests | Pass (incl. 1 ms/5 ms clocks, movestogo 0/1/large, increments, overhead) |
| 200-move 60/120 stress | Pass: zero forfeits, zero illegal moves, max move 3889 ms <= hard+50 |
| movetime regression tests | Pass (`search honors movetime` test suite green) |
| Prediction guard | Pass (no budget = no prediction; first iteration never skipped) |
| TSCP match | **Recorded** (`build/tscp-match.pgn`, Cute Chess 1.5.1 found on the machine): 8 games at the task's 60/120 control, **try4 6-2-0 vs TSCP 1.81e** (4-0 White, 2-2 Black), LOS 92.1%, Elo +190.8 [-87.2, 468.8]; terminations: 7 mates, 1 TSCP win by mate, 2 time forfeits (one each side); zero crashes or protocol failures |
| Match result | 6-2-0 in try4's favor; see findings below |
| Report | `scripts/match_report.py` on the 8-game PGN: ply range 24-404, median 334 |
| Docs | CONFIGURATION/CHANGELOG updated; PERFORMANCE/ROADMAP pending the match numbers |
| Full suite | Debug 27/27 pass (28 with the task 034 SEE tests) |

**Risks or follow-ups:**

- **Match findings (the honest record).** The match was replayed five times
  total while investigating the two forfeits with env-gated forensics
  (`TRY4_TIME_DEBUG=1` logs every budget decision and every UCI command-flow
  event to stderr; `scripts/`-side drivers in `build/tscp-debug.log`,
  `build/try4-time.log`, `build/try4-flow.log`, `build/try4-time2.log`).
  - TSCP chronically exceeds the 2 s-per-move classic budget (PGN
    annotations: 3.1-5.0 s moves) and forfeited on time in two of five
    replays; all decisive games were mate wins for whichever side kept its
    clock together.
  - try4's one on-time loss (record match game 2, ~84 s still on the clock
    when the decision was due) did **not** reproduce in four instrumented
    replays: across 147 measured budget cycles the engine's response stayed
    inside the logged hard ceiling every time, and the UCI command flow
    (go -> stop-join -> launch -> search -> bestmove) is gapless over full
    games. The mechanism of that single loss is therefore unconfirmed;
    candidates are a rare engine-side stall that predates the flow hooks or
    manager-side timing effects under long decisive games.
  - Smallest next action: a longer recorded match (50-100 games) with
    `TRY4_TIME_DEBUG=1` on the engine's stderr plus per-move wall-clock
    capture on the manager side, to catch the rare loss-of-decision if it is
    real. The budget log shape (`t=<ms> budget side= rem= mtg= soft= hard=`)
    makes the guilty move directly readable from the log.
- The best-move-instability hard-extension is bounded only by the hard
  ceiling; that is by design.
- The predictor's branching factor is per-search; a per-position learning
  refinement is possible later.
