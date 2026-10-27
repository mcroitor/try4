# Task 049 - Record the search baseline and add a repeatable benchmark harness

| Field | Value |
|---|---|
| **ID** | 049 |
| **Stage** | Search performance - baseline |
| **Depends on** | 048 |
| **Estimate** | 0.5 day |
| **Priority** | P1 |
| **Status** | Done |

## Objective

Record a Release-mode search baseline (nodes, NPS, time-to-depth, best-move
stability) and add a repeatable harness plus a documented procedure, so tasks
050-057 can prove speed and depth gains instead of asserting them.

## Background

`docs/PERFORMANCE.md` records perft only. There is no search NPS baseline, no
time-to-depth baseline and no recorded binary/time control for the TSCP match
observation ("try4 reaches depth 4, TSCP reaches 7-9 halfmoves").

A 2026-10-27 analysis session on Windows 11 / MSYS2 g++ 16.2.0 produced these
**Debug** (`build/win-local`, start position) numbers with the current engine:

| Limit | Depth | Nodes | Time | NPS |
|---|---:|---:|---:|---:|
| `go depth 4` | 4 | 2,462 | 102 ms | 24k |
| `go depth 5` | 5 | 23,165 | 1,082 ms | 21k |
| `go depth 6` | 6 | 54,876 | 4,347 ms | 13k |
| `go depth 7` | 7 | 395,799 | 61,510 ms | 6k |
| `try4_perft_bench` depth 4 | - | 197,281 | 173 ms | 1,140k |

The same build does raw legal move generation plus make/unmake at ~1.1M
nodes/s but searches at ~20k nodes/s. That ~50x gap is the target of tasks
050-051. Debug numbers must not be used as optimization targets or as release
evidence; they are recorded here only to justify the work order.

## In scope

- `tests/data/bench/search.tsv` (tab-separated `id`, `kind`, `fen`) with 6-8
  positions: start position, Kiwipete, one middlegame, one rook endgame, one
  tactical/forcing position, one drawn-ish position.
- `scripts/search_bench.py`: drive `try4_uci` through pipes and support
  `--engine`, `--cli`, `--positions`, `--depths`, `--movetime-ms`,
  `--repeats`, `--output`, `--timeout-ms`. Parse
  `info depth/score/nodes/nps/time` and `bestmove`; validate every returned
  move through the debugging CLI legal-move judge; write `bench.json`,
  `bench.csv` and `bench.md`.
- Median-of-three reporting plus build metadata: build type, compiler,
  relevant CMake cache values, CPU/OS, date, configured `Hash`.
- A "Search baseline" section in `docs/PERFORMANCE.md` with the Release table,
  the exact reproduce command, and the Debug-vs-Release ratio.
- Record the binary, build type, time control and exact `go` line used for the
  original TSCP observation, or mark them explicitly as unknown.

## Out of scope

- Any engine behavior, evaluation, search, TT or time-management change.
- CI performance gates (hosted runner noise makes them unreliable); the script
  stays a manual Release gate.
- Strength, SPRT or Elo claims.
- Bitboards or other representation work.

## Requirements

1. Fixed-depth runs are deterministic within one build: identical node counts
   and PVs across repeats. Any deviation is a defect and must be reported, not
   averaged away.
2. Fixed-time runs report median and spread (min/max NPS); never compare
   medians from different build types.
3. The runner must not leak processes: handle EOF, timeout (default 30 s per
   search) and `quit` cleanup on all paths, including Windows.
4. `bench.json` schema:
   `{"meta":{"engine","build_type","compiler","cpu","os","date","hash_mb","positions_file","movetime_ms"},"runs":[{"position","kind","depth","score","nodes","nps","ms","pv","bestmove","legal"}]}`.
5. The position set is versioned and additive; changing it invalidates direct
   comparison with earlier baselines, so record the file hash.
6. Benchmarks run with the opening book disabled and no undocumented
   `setoption` calls; record any option used.

## Deliverables

- `scripts/search_bench.py`, `tests/data/bench/search.tsv`.
- "Search baseline" section in `docs/PERFORMANCE.md`.
- Baseline artifacts under `build/bench-baseline/` (not committed) and the
  committed summary table.

## Acceptance matrix

| Case | Expected result |
|---|---|
| Release, `go depth 1..7`, start position | Table recorded; node counts identical across 3 repeats |
| Release, `go movetime 2000`, all positions | Depth and NPS median recorded; NPS spread <= 5% |
| Debug vs Release | Ratio recorded; build type present in every record |
| Illegal bestmove, hang or missing `bestmove` | Script exits nonzero and names the failing case |
| Windows and Linux | Same schema and results format; no path assumptions |
| Original TSCP observation | Binary, build type, time control and `go` line documented or marked unknown |

## Validation commands

```sh
cmake -S . -B build/bench-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/bench-release
python scripts/search_bench.py --engine build/bench-release/try4_uci.exe \
  --cli build/bench-release/try4.exe --depths 1,2,3,4,5,6 \
  --movetime-ms 2000 --repeats 3 --warmup-ms 30000 --cpu 2 \
  --timeout-ms 180000 --output build/bench-baseline
python scripts/search_bench.py --engine build/bench-release/try4_uci.exe \
  --cli build/bench-release/try4.exe --only startpos --depths 1,2,3,4,5,6,7 \
  --movetime-ms 2000 --repeats 3 --warmup-ms 30000 --cpu 2 \
  --timeout-ms 300000 --output build/bench-startpos
ctest --test-dir build/bench-release -C Release --output-on-failure
```

Expected: baseline files are written, every bestmove is legal, and the full
suite still passes. `--cpu` and `--warmup-ms` are environment controls, not
measurement changes; without them the hybrid CPU produced NPS spreads of
30-200% on this host. The separate start-position run exists because depth 7
on the full tactical set takes several minutes.

## Risks and failure modes

- **Comparing across build types.** Debug measured ~9-10x slower than Release at
  depth 4 (not the 3-5x originally estimated); every record carries the build
  type and the report refuses to merge them.
- **NPS noise** from background load, clock ramping or thermal throttling. Pin
  the engine to one P-core, discard a warm-up search, use medians, record the
  CPU, and compare only within one host state.
- **Shallow or unrepresentative positions** hide regressions. Include a
  tactical and a middlegame position, not only the start position.
- **Parser drift.** `info` format changes are script defects; extend the parser
  rather than weakening the assertions.

## Completion notes

- `scripts/search_bench.py` drives `try4_uci` over pipes, sends `ucinewgame`
  (clears the TT) before every search, validates each `bestmove` through the
  `try4` CLI judge, and writes `bench.json`, `bench.csv` and `bench.md`.
- `tests/data/bench/search.tsv` contains 8 positions: start, Kiwipete, rook
  endgame, promotion tactics, quiet middlegame, queen endgame, an opening check
  and a dead-drawn KP blockade. Reports record its SHA-256
  (`809d2f2c...`).
- Environment controls were added because the hybrid laptop CPU otherwise
  produced 30-200% NPS spread: `--cpu` affinity, optional `--priority`, and
  `--warmup-ms`. The baseline uses logical CPU 2, normal priority, 30 s
  warm-up, 3 repeats.
- Release baseline at 2 s: start position depth 6, 229,788 nodes, 116,054 NPS
  (1.2% spread); Kiwipete depth 4, 61,412 NPS (1.4%); worst spread 4.9%
  (`kp-blockade`). All fixed-depth node counts, PVs and best moves were
  identical across repeats.
- Debug/Release depth-4 ratio: median 9.8x (7-12x across positions). At 2 s,
  Debug reaches 1-3 plies less than Release.
- The 2,462-node Debug start-position figure in the task background did not
  reproduce under the harness (3,556 nodes at depth 4). It appears to be a
  warm-TT count; the harness clears the TT before every search.
- Full tables and exact reproduce commands: `docs/PERFORMANCE.md`, section
  "Search baseline (2026-10-27)".

## Completion report

Return:

- **Status:** done or blocked
- **Files changed:** paths and one-line descriptions
- **Commands and results:** exact commands, baseline table, spread
- **Acceptance matrix:** each row pass or fail
- **TSCP observation:** binary/build type/time control/`go` line, or unknown
- **Risks or follow-ups:** deferred items and the smallest next action
