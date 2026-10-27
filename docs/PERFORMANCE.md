# Performance Baseline

Perft is the Stage 2 correctness gate and the first repeatable performance
measurement. The numbers below are a baseline, not a target: they were captured
before any search or evaluation work.

## Environment

| Field | Value |
|---|---|
| Date | 2026-10-23 |
| CPU | 13th Gen Intel Core i5-13600HX (14 cores / 20 logical processors, 2.6 GHz base) |
| OS | Windows 11 Pro |
| Compiler | g++ (MSYS2) 16.2.0 |
| Build system | CMake 4.4.3, Ninja 1.13.2 |
| Build type | `Release` (`-O3 -DNDEBUG`) |
| Warnings | `-Wall -Wextra -Wpedantic -Werror` |

The measurements are single-threaded and use the `try4_perft_bench` executable.

## Results

| Position | Depth | Nodes | Time | Nodes/second |
|---|---:|---:|---:|---:|
| Start | 5 | 4,865,609 | 204 ms | 23,851,025 |
| Kiwipete | 4 | 4,085,603 | 299 ms | 13,664,224 |
| Kiwipete | 5 | 193,690,690 | 19,569 ms | 9,897,833 |

The start position has fewer legal moves and simpler branches, so its NPS is
higher than Kiwipete's. These numbers include full legal-move filtering and
make/unmake for every node.

## Reproduce

Build the optimized benchmark:

```sh
cmake -S . -B build/cmake-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DTRY4_BUILD_TESTS=OFF \
  -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/cmake-release --config Release
```

Run one position:

```sh
./build/cmake-release/try4_perft_bench "<FEN>" 5
```

Run the six reference positions at depth 5 (or 6) and compare every count:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/perft_deep.ps1 `
  -Exe build/cmake-release/try4_perft_bench.exe `
  -Depth 5
```

Use `-Depth 6` for the full deep run and `-Position kiwipete` to run a single
position. Depth 6 takes substantially longer and is intentionally excluded from
default CI.

The normal CI gate remains depths 1–4 for all six positions:

```sh
ctest --test-dir build/cmake -C Debug -L perft --output-on-failure
```

## Search baseline (2026-10-27)

Perft above measures move generation. This section records the first repeatable
**search** baseline (task 049) and the exact harness that produced it. The
numbers are a reference point for tasks 050–057, not an optimization target.

### Method

- Engine: `try4_uci` (try4 0.1.0), one search per `go`, one thread.
- Every search is preceded by `ucinewgame`, which clears the transposition
  table. The node counts below are therefore cold-TT counts.
- Options: `setoption name OwnBook value false`; `Hash` is the engine default
  (16 MB) and was not overridden. Move Overhead keeps its default 20 ms, so a
  `go movetime 2000` search actually runs for about 1980 ms.
- Every returned `bestmove` is validated by the `try4` CLI legal-move judge
  (`fen ...` followed by `move ...`).
- A fixed-time case fails if its wall time exceeds the requested movetime by
  more than 100 ms; the measured wall times were 1980–1988 ms for 2000 ms.
- Positions: `tests/data/bench/search.tsv` (SHA-256
  `809d2f2c156fefea5cb92057789d259e3ca9fed8e18aa2615dd2fb33052b8d2a`).
- The engine is pinned to one logical CPU (`--cpu 2`, a P-core on this host) and
  a discarded 30 s warm-up search runs first. Without both controls this hybrid
  CPU produced fixed-time NPS spreads of 30–200% from P/E-core placement and
  clock ramping.
- Three repeats per case; medians are reported. All fixed-depth node counts,
  PVs and best moves were identical across repeats, so the search is
  deterministic once the TT state is fixed.

| Field | Value |
|---|---|
| Date | 2026-10-27 |
| CPU | 13th Gen Intel Core i5-13600HX (14 cores / 20 logical processors) |
| OS | Windows 11 Pro 10.0.26200 |
| Compiler | g++ (MSYS2 UCRT64) 16.2.0 |
| Build type | `Release` (`-O3 -DNDEBUG`), warnings-as-errors |
| CMake / Ninja | CMake 4.4.3 / Ninja 1.13.2 |
| Position file | `809d2f2c…` (see above) |
| Engine affinity / warm-up | logical CPU 2 / 30 s |

The fixed-time spread is `(max - min) / median`. The worst case in the
committed run is 4.9% (the 27-ply `kp-blockade` shuffle); every other position
is below 4%.

### Time to depth — start position (Release)

| Depth | Nodes | Time | Nodes/second | Score | Best move |
|---:|---:|---:|---:|---|---|
| 1 | 41 | 1 ms | 41,000 | cp 43 | b1c3 |
| 2 | 180 | 1 ms | 180,000 | cp 10 | b1c3 |
| 3 | 1,175 | 7 ms | 167,857 | cp 43 | b1c3 |
| 4 | 3,556 | 21 ms | 169,333 | cp 10 | b1c3 |
| 5 | 26,465 | 168 ms | 157,529 | cp 12 | b1c3 |
| 6 | 79,677 | 565 ms | 141,021 | cp 10 | b1c3 |
| 7 | 432,137 | 3,404 ms | 126,949 | cp 31 | g1f3 |

Depth 7 is the first iteration that changes the best move. The 3.4 s cost is
also the reason the full-set run below stops at depth 6: a depth-7 sweep of all
positions takes many minutes and is not a useful quick gate.

### Fixed depth 6 — all positions (Release)

| Position | Kind | Nodes | Time | Nodes/second | Score | Best move |
|---|---|---|---:|---:|---|---|
| startpos | opening | 79,677 | 569 ms | 140,029 | cp 10 | b1c3 |
| kiwipete | tactical | 841,911 | 12,679 ms | 66,402 | cp 10 | e2a6 |
| perft3-rook-endgame | endgame | 16,974 | 107 ms | 158,635 | cp 46 | b4f4 |
| perft5-promotion | tactical | 370,774 | 4,445 ms | 83,413 | cp 499 | d7c8q |
| perft6-middlegame | middlegame | 2,269,034 | 32,779 ms | 69,222 | cp 69 | c3d5 |
| queen-endgame | endgame | 52,546 | 245 ms | 214,473 | cp 1013 | g1d1 |
| in-check | opening-check | 155,738 | 1,472 ms | 105,800 | cp 309 | c2c3 |
| kp-blockade | drawn-ish | 1,298 | 3 ms | 432,666 | cp 10 | e3d3 |

Terminal-heavy and quiet endgames reach the highest NPS; tactical middlegames
with wide trees and quiescence work are the slowest. A depth-6 search of
`perft6-middlegame` takes 33 s, so fixed-depth timing is only meaningful for
depth 5 and below on that position.

### Fixed time — 2000 ms per search (Release)

> **Reporting note (2026-10-27):** these rows were recorded before the UCI
> adapter streamed per-depth `info` lines. The last `info` before `bestmove` now
> reports the last completed iteration, whereas the original run used a single
> final line whose node count included the aborted next iteration. Re-run this
> table before comparing fixed-time NPS with later builds; the fixed-depth node
> counts and PVs remain the stable reference.

| Position | Kind | Depth | Nodes | NPS min | NPS median | NPS max | Spread | Best move |
|---|---|---:|---:|---:|---:|---:|---:|---|
| startpos | opening | 6 | 229,788 | 114,876 | 116,054 | 116,235 | 1.2% | b1c3 |
| kiwipete | tactical | 4 | 121,597 | 61,246 | 61,412 | 62,102 | 1.4% | e2a6 |
| perft3-rook-endgame | endgame | 8 | 338,041 | 164,776 | 170,727 | 171,075 | 3.7% | b4f4 |
| perft5-promotion | tactical | 5 | 174,135 | 87,848 | 87,946 | 89,024 | 1.3% | d7c8q |
| perft6-middlegame | middlegame | 4 | 155,643 | 78,150 | 78,607 | 80,331 | 2.8% | c3d5 |
| queen-endgame | endgame | 8 | 453,164 | 226,971 | 228,870 | 229,771 | 1.2% | e3e4 |
| in-check | opening-check | 6 | 202,018 | 100,985 | 102,029 | 102,217 | 1.2% | c2c3 |
| kp-blockade | drawn-ish | 27 | 681,536 | 333,656 | 344,210 | 350,432 | 4.9% | e3f3 |

At two seconds the Release build reaches depth 6 on the start position, depth 4
on Kiwipete and depth 4–8 on the rest. `kp-blockade` reaches depth 27 because it
is a dead-drawn shuffle with a tiny tree, so its NPS is high and it is not a
useful depth comparison.

### Debug vs Release (depth 4)

| Position | Release ms | Release NPS | Debug ms | Debug NPS | Ratio |
|---|---:|---:|---:|---:|---:|
| startpos | 20 | 177,800 | 247 | 14,396 | 12.3× |
| kiwipete | 829 | 64,196 | 7,600 | 7,002 | 9.2× |
| perft3-rook-endgame | 10 | 183,200 | 98 | 18,693 | 9.8× |
| perft5-promotion | 217 | 85,617 | 2,439 | 7,617 | 11.2× |
| perft6-middlegame | 760 | 70,935 | 6,903 | 7,809 | 9.1× |
| queen-endgame | 17 | 215,941 | 175 | 20,977 | 10.3× |
| in-check | 107 | 106,514 | 1,011 | 11,272 | 9.5× |
| kp-blockade | 1 | 353,000 | 6 | 58,833 | 6.0× |

Debug is about **9–10× slower** than Release (median 9.8× for searches of 10 ms
or longer); the sub-millisecond `kp-blockade` row is timing-noise-limited. The
"3–5×" estimate in the task plan was low. The earlier 2,462-node Debug
start-position measurement in the task plan did not reproduce here (3,556 nodes
at depth 4) because this harness clears the TT before every search; the earlier
figure appears to be a warm-TT count.

The same gap shows up as depth at 2 s: Debug reaches depth 5 on the start
position versus Release depth 6, depth 2 versus 4 on Kiwipete, and depth 3
versus 4 on `perft6-middlegame`. Debug fixed-time spreads were 0.8–12.8% (two
positions above 5%), so the ratio table above uses fixed-depth medians; the
Release fixed-time spread is the acceptance measurement.

### Original TSCP observation

The 2026-10-27 match observation ("try4 ~depth 4, TSCP 7–9 halfmoves") is
recorded here as anecdotal because the artifacts were not archived:

| Field | Value |
|---|---|
| Binary | **unknown** — not recorded |
| Build type | **unknown** — not recorded |
| Time control | 60 moves / 2 minutes per side (owner report; Cute Chess `tc=60/120`) |
| Exact `go` line | **unknown** — under Cute Chess the engine receives `go wtime … btime … winc … binc …`, not `go depth N` |
| PGN / match log | **unknown** — not in the repository |
| Result | try4 reached ~depth 4 while TSCP reached 7–9 plies; both engines report plies |

The baseline shows that the Release build already reaches depth 6 at 2 s on the
start position, while a Debug build reaches depth 5. A Debug build, a shorter
effective clock, or a slower host can therefore account for several plies of the
reported gap. This is a hypothesis, not a measurement: task 057 must record the
binary, build type, time control and `go` line in the match report.

### Noise and comparison rules

- Compare node counts and PVs first; they are deterministic for a fixed build
  and position, independent of host load.
- Compare NPS only within one host power state and one build type. On this host
  two clean runs of the same Release binary differed by about 35% in absolute
  NPS (start position fixed-time median 172k vs 116k) while node counts and PVs
  were identical. One intermediate run also hit a single 2 s host stall and
  reported a 38% spread; the committed run has no such stall. Re-run the
  baseline immediately before an optimization comparison.
- Do not compare a Debug median with a Release median. The report records
  `build_type` in the metadata and in every run record.

### Reproduce

Build the Release benchmark (tests are not needed for the harness; keep them on
for the full validation command):

```powershell
cmake -S . -B build/bench-release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DTRY4_BUILD_TESTS=ON `
  -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/bench-release
```

Full baseline (fixed time first, then depths 1–6):

```powershell
python scripts/search_bench.py `
  --engine build/bench-release/try4_uci.exe `
  --cli build/bench-release/try4.exe `
  --depths 1,2,3,4,5,6 --movetime-ms 2000 --repeats 3 `
  --warmup-ms 30000 --cpu 2 --timeout-ms 180000 `
  --output build/bench-baseline
```

Start-position time-to-depth, including depth 7:

```powershell
python scripts/search_bench.py `
  --engine build/bench-release/try4_uci.exe `
  --cli build/bench-release/try4.exe `
  --only startpos --depths 1,2,3,4,5,6,7 --movetime-ms 2000 --repeats 3 `
  --warmup-ms 30000 --cpu 2 --timeout-ms 300000 `
  --output build/bench-startpos
```

Debug ratio run:

```powershell
cmake -S . -B build/cmake-debug -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DTRY4_BUILD_TESTS=OFF `
  -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/cmake-debug
python scripts/search_bench.py `
  --engine build/cmake-debug/try4_uci.exe `
  --cli build/cmake-debug/try4.exe `
  --depths 1,2,3,4 --movetime-ms 2000 --repeats 3 `
  --warmup-ms 30000 --cpu 2 --timeout-ms 300000 `
  --output build/bench-debug
```

`--cpu N` pins the engine with `SetProcessAffinityMask` on Windows and
`sched_setaffinity` on Linux; choose an otherwise idle P-core and record it.
The harness writes `bench.json`, `bench.csv` and `bench.md` under `--output`.
The artifacts above are not committed; only the tables and the position file
hash are tracked.