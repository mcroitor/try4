# Task 058 - Add a configurable resignation threshold

| Field | Value |
|---|---|
| **ID** | 058 |
| **Stage** | Post-release engine behavior |
| **Depends on** | 039, 043 |
| **Estimate** | 0.5-1 day |
| **Priority** | P1 |
| **Status** | Ready |

## Objective

Make the UCI engine resign when the root score of its last completed search is
below a configurable centipawn threshold. The default is `-600` cp. The value is
configurable in the startup file and through a UCI spin option; `0` disables
resignation. Search, evaluation and benchmark results must otherwise be
unchanged.

## Source requirement

Owner request, 2026-10-27:

> Resign option must be implemented. If the evaluation function returns value
> less then defined, engine must resign. The default value of resign is -600,
> option is configurable in the config file.

## Background

- `Searcher::Search` returns `SearchInfo::scoreCp` from the last completed
  iterative-deepening iteration, from the side-to-move perspective.
- `UciEngine::SearchWorker` currently always emits `bestmove <move>`, or
  `bestmove 0000` only when the root position is terminal.
- `UciOptions` and the INI parser already carry startup defaults for `[uci]` and
  `[search]`; later `setoption` values override startup values.
- UCI has no resign command. `bestmove 0000` is the protocol nullmove and the
  only portable no-move signal. The engine will emit an explicit
  `info string resign ...` diagnostic immediately before it so consumers can
  distinguish resignation from a terminal position.

## Decisions and assumptions

These are the task contract. The score-source and UCI-signal choices are
recorded assumptions; if the owner intended raw static evaluation or a different
resignation signal, revise this task before implementation.

1. **Score source.** "Evaluation value" means `SearchInfo::scoreCp` for the last
   fully completed search iteration. It is not a standalone `Evaluate()` call.
   The search score includes quiescence and tactical lines and is the score the
   engine reports to the GUI.
2. **Comparison.** Resign when `scoreCp < resign_cp` (strictly less than).
   Equality does not resign. Scores are relative to the side to move.
3. **Threshold.** Default `-600`; valid range `-100000..0`; `0` disables
   resignation. Negative mate scores are below the threshold and resign;
   positive mate scores never do.
4. **Interfaces and precedence.** Startup key `[search] resign_cp`; UCI option
   `Resign` type spin default `-600` min `-100000` max `0`. Precedence is
   built-in < startup file < later `setoption`, consistent with task 043.
   `ucinewgame` retains session overrides. No hot reload.
5. **When not to resign.** No resignation for `go infinite` analysis, opening
   book moves, terminal roots, a search with no completed iteration
   (`info.depth == 0`), or a search cancelled by `stop`/`quit`. Return the best
   legal move in those cases. A zero-node budget and an immediate stop therefore
   return a legal fallback.
6. **Output.** On resignation emit exactly one diagnostic and one bestmove:

   ```text
   info string resign score cp <signed-cp> threshold <signed-cp>
   bestmove 0000
   ```

   For a negative mate score, use the existing mate formatting:

   ```text
   info string resign score mate <-N> threshold <signed-cp>
   bestmove 0000
   ```

   Emit no `info string resign` for a terminal `bestmove 0000`.
7. **Stateless.** No persistent "already resigned" flag. Each ordinary `go` is
   evaluated independently; the GUI ends the game. `ucinewgame` needs no special
   reset.
8. **Scope.** Core `Searcher`, the CLI and the self-play harness do not resign.
   This is a UCI game policy. Benchmarks explicitly disable it so performance and
   move choice remain comparable.

## Configuration and output schema

| Interface | Key or option | Range | Default | Notes |
|---|---|---|---|---|
| Startup file | `[search] resign_cp` | `-100000..0` | `-600` | `0` disables |
| UCI `setoption` | `Resign` (spin) | `-100000..0` | `-600` | `0` disables; overrides the file |

The effective value is included in `ConfigurationSnapshot` and in the startup
`engine-settings.ini` written by self-play. Invalid values fail startup with a
source/key/value diagnostic; no partial settings are applied.

| Condition | Required output |
|---|---|
| Ordinary score below threshold | `info string resign score cp ...` then `bestmove 0000` |
| Negative mate score below threshold | `info string resign score mate ...` then `bestmove 0000` |
| No resignation | Existing per-depth `info` lines then `bestmove <legal move>` |
| Terminal root | Existing info then `bestmove 0000`, without a resign diagnostic |
| No completed iteration | `info depth 0 ...` then `bestmove <legal fallback>` |
| `go infinite` stopped by `stop` | Existing infos then `bestmove <legal move>` |

Each `go` produces exactly one `bestmove` line.

## In scope

- `include/chess/uci.hpp`: add the `resign_cp` option field and, if useful, a
  small pure `ShouldResign` helper for boundary tests.
- `src/core/configuration.cpp`: parse, validate and snapshot
  `[search] resign_cp`.
- `src/uci/uci.cpp`: advertise and parse `Resign`; apply the decision after
  search; emit the resignation diagnostic.
- `config/try4.ini`: add the commented default.
- `scripts/search_bench.py`: send `setoption name Resign value 0` and record it
  in the benchmark metadata.
- `scripts/benchmark_forcing.py`: disable resignation for all forcing/tactical
  measurement configurations.
- `scripts/uci_stability.py`: accept an optional `--config` path and pass it to
  both engine instances; treat `info string resign` plus `bestmove 0000` as a
  resignation termination, not an illegal move or hang, and record the result.
- Tests: `tests/configuration_tests.cpp`, `tests/uci_tests.cpp`,
  `tests/engine_process_tests.py`; update existing fixtures that intentionally
  search losing positions so they disable resignation.
- Documentation: `docs/UCI.md`, `docs/CONFIGURATION.md`, `CHANGELOG.md`.
- `tasks/README.md`: index and dependency note for this task.

## Out of scope

- Changing search scores, evaluation, transposition tables, time management or
  move selection.
- Resignation in the `try4` CLI, `try4_selfplay`, or the core `Searcher`.
- Automatic GUI adjudication, draw offers, or `bestmove 0000` semantics beyond
  this policy.
- A minimum resign depth or score-stability window. If the owner wants to avoid
  shallow-score noise, add a separate follow-up option.
- Pondering, `searchmoves`, `go mate`, Chess960 or hot reload.

## Implementation instructions

1. Read this file, `include/chess/uci.hpp`, `src/core/configuration.cpp`,
   `src/uci/uci.cpp`, and the current UCI process tests.
2. Add the option, parser, snapshot and startup example; verify invalid values
   fail without partial application.
3. Apply the decision only after `Searcher::Search` returns and only when the
   exclusions in the contract hold. Reuse the existing score formatting rather
   than duplicating mate-distance logic.
4. Emit the resignation diagnostic and exactly one `bestmove 0000`. Do not
   change the terminal no-legal-move path.
5. Update the three scripts and the tests. Add a deterministic strict-boundary
   test; do not hard-code a brittle search score that later search work can
   change.
6. Update the documentation, run the commands below, and return the completion
   report.

## Requirements

1. The default threshold is `-600` cp and is active without a configuration file.
2. `[search] resign_cp` accepts `-100000..0`; `0` disables; values outside the
   range, non-integers and malformed rows fail startup with a diagnostic.
3. The `Resign` UCI spin option advertises the effective startup default, accepts
   the same range, rejects invalid values without changing the current setting,
   and overrides the startup file. `ucinewgame` retains the override.
4. The decision uses the last completed iteration's `scoreCp`; it never uses an
   aborted iteration, the depth-0 fallback score or a book move.
5. Resign only when `info.depth > 0`, `info.bestMove` has a value, the search is
   not infinite, and the search was not cancelled by `stop`/`quit`.
6. The strict comparison is `scoreCp < resign_cp`. A score equal to the threshold
   does not resign.
7. Negative mate scores resign under the default; positive mate scores never do.
8. A terminal root produces `bestmove 0000` with no resign diagnostic. A
   non-terminal no-move-budget search produces a legal fallback move.
9. The resignation diagnostic matches the output schema and is followed by
   exactly one `bestmove 0000`.
10. The benchmark harnesses disable resignation through the UCI option and record
    that option in their metadata; a lost tactical position must still return a
    legal benchmark move.
11. The stability harness records a resignation as a loss for the side to move,
    with termination `resign`, and does not count it as an illegal move or hang.
12. Full Debug and Release builds are warning-clean and the existing suite passes.

## Acceptance matrix

| ID | Case | Setup | Expected |
|---|---|---|---|
| R1 | Default value | No config file | Effective `resign_cp == -600`; handshake advertises default `-600` |
| R2 | File override | `[search] resign_cp=-700` | Snapshot and handshake use `-700` |
| R3 | UCI override | File `-700`, `setoption name Resign value -900` | `-900` applies; `ucinewgame` retains it |
| R4 | Invalid values | `-100001`, `1`, `abc` | Startup fails; no partial settings |
| R5 | Default resignation | Losing FEN `4k3/8/8/8/8/8/8/3QK3 b - - 0 1`, `go depth 3` | `score` below `-600`; resign diagnostic; `bestmove 0000`; exactly one bestmove |
| R6 | Disabled | `resign_cp=0`, same position and command | No resign diagnostic; legal `bestmove` |
| R7 | Lower threshold | `resign_cp=-5000`, same position | No resign diagnostic; legal `bestmove` |
| R8 | Strict boundary | Threshold equal to the observed last-completed score, then one cp higher | Equal: legal move; one cp higher: resign |
| R9 | Mate against side | `7k/8/5K2/6Q1/8/8/8/8 b - - 0 1`, `go depth 2` | `score mate -1`; resign diagnostic with mate formatting; `bestmove 0000` |
| R10 | Balanced/positive | Start position; white queen-to-move position | No resign diagnostic; legal `bestmove` |
| R11 | Infinite analysis | Losing FEN, `go infinite`, then `stop` | No resign diagnostic; legal `bestmove` |
| R12 | No completed iteration | Losing FEN, `go nodes 0` | `info depth 0`; no resign; legal fallback; one bestmove |
| R13 | Terminal root | Checkmate FEN `7k/6Q1/6K1/8/8/8/8/8 b - - 0 1` | `bestmove 0000`; no resign diagnostic |
| R14 | Benchmark isolation | Short search-bench smoke | Metadata contains `setoption name Resign value 0`; every benchmark result has a legal move |
| R15 | Forcing isolation | Forcing/tactical run with a losing case | No `bestmove 0000`; cases are scored on moves, not resignation |
| R16 | Stability termination | `uci_stability.py --config` with `resign_cp=-1`, two short games | Resignation is recorded as a loss/`resign`; no illegal-move or hang failure |
| R17 | Existing suite | Full Debug and Release `ctest` | All tests pass; no new warnings |

## Validation commands

```powershell
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug `
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug -L "config|uci|search|release" --output-on-failure
ctest --test-dir build/cmake -C Debug --output-on-failure

cmake -S . -B build/performance -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/performance
ctest --test-dir build/performance -C Release --output-on-failure

python scripts/search_bench.py `
  --engine build/performance/try4_uci.exe `
  --cli build/performance/try4.exe `
  --only startpos --depths 1 --movetime-ms 100 --repeats 1 --warmup-ms 0 `
  --cpu 2 --output build/resign-bench

python scripts/uci_stability.py `
  --engine build/performance/try4_uci.exe `
  --cli build/performance/try4.exe `
  --config build/resign-test.ini `
  --games 2 --workers 1 --base-ms 100 --inc-ms 10 --max-plies 20 `
  --output build/resign-stability
```

For the stability command, create `build/resign-test.ini` with
`[search]\nresign_cp=-1` so at least one game can exercise the resignation path.
If no resignation occurs, use a deliberately lost opening and repeat; the
process tests R5-R13 remain the normative output checks.

Manual GUI check: play a short Cute Chess game from a lost position and confirm
the GUI records resignation/forfeit, not a hang or crash. Generic GUIs may report
`bestmove 0000` differently; record the observed behavior in the completion
report.

## Risks and failure modes

| Risk | Mitigation |
|---|---|
| UCI has no resign command; a GUI may treat `0000` as a nullmove or illegal move | Emit `info string resign` before `0000`; document the limitation and verify in Cute Chess |
| A shallow score is noisy and causes an early resignation | Default threshold is large (`-600`); no minimum depth is added in this task. Record a follow-up if needed |
| An aborted iteration's score is used | Use only `SearchInfo::scoreCp` when `info.depth > 0`; add R12 and the `stop`/infinite tests |
| Resignation changes benchmark or tactical results | Disable it in `search_bench.py` and `benchmark_forcing.py`, and record the option |
| Existing tests search a lost position and now receive `0000` | Disable resignation explicitly in those fixtures; keep dedicated resignation tests separate |
| Mate scores are compared as ordinary centipawns by mistake | Compare the raw `scoreCp`; use the existing mate formatter only for output |
| `stop` races with the decision and produces a resignation during shutdown | Read the stop flag after `Search` returns; suppress resignation when it is set |
| The stability harness treats `0000` as an illegal move | Update its result parser and add the forced-resignation smoke run |
| Config and UCI defaults diverge | Reuse the same field and range; test handshake, snapshot and precedence |

## Completion report

Record:

1. Status and commit(s).
2. Files changed.
3. Commands run with pass/fail counts.
4. R1-R17 results, including the exact resignation lines and the observed GUI
   behavior.
5. Any deviation from the score-source or output assumptions.
6. Follow-ups: `Resign Depth` or score-stability gating, if desired.
7. The smallest next action.
