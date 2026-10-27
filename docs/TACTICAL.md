# Tactical suite

The tactical suite is the search-correctness gate before UCI work. Its corpus
lives in [`tests/data/tactical/positions.tsv`](../tests/data/tactical/positions.tsv)
and is executed by `try4_tactical` under the `tactical` CTest label.

## Data format

Each non-comment line has seven tab-separated fields:

```text
id  phase  fen  depth  expectations  avoid  note
```

`phase` is `opening`, `middlegame`, or `endgame`. Expectations are
semicolon-separated:

- `mate:N` - exact score `kMateScore - N`; `N` counts **plies**.
- `best:UCI` - exact best move.
- `positive` - the score must be greater than zero.
- `score:N` - exact centipawn score.

`avoid` contains a move that must not be selected, or `-`.

The runner performs two fresh fixed-depth searches per position and compares
depth, score, node count, and best move. No clock fields are set.

## Coverage

| Case | Phase | Depth | Expectation |
|---|---|---:|---|
| `mate1-scholar` | middlegame | 1 | mate in 1 ply |
| `mate2-kqk` | endgame | 3 | mate in 2 moves (3 plies) |
| `mate3-krk` | endgame | 5 | mate in 3 moves (5 plies) |
| `capture-hanging-queen` | opening | 2 | win the queen |
| `stalemate-avoid-kqk` | endgame | 2 | avoid `g5g6` stalemate |
| `regression-kiwipete` | middlegame | 3 | `e2a6` at fixed depth |
| `regression-perft3` | endgame | 3 | `b4f4` at fixed depth |

The `stalemate-avoid-kqk` case checks both the winning `g5g7` and the trap
`g5g6`: after `Qg6` the position is a verified stalemate.

## Baseline

Latest Windows Debug run (GCC 16.2.0, `-DTRY4_WARNINGS_AS_ERRORS=ON`):

| Case | First-search time | Nodes | Best |
|---|---:|---:|---|
| `mate1-scholar` | 8 ms | 89 | `f3f7` |
| `mate2-kqk` | 55 ms | 1,965 | `e5f6` |
| `mate3-krk` | 401 ms | 17,551 | `e5f6` |
| `capture-hanging-queen` | 17 ms | 231 | `f3h4` |
| `stalemate-avoid-kqk` | 4 ms | 55 | `g5g7` |
| `regression-kiwipete` | 1,914 ms | 20,589 | `e2a6` |
| `regression-perft3` | 81 ms | 649 | `b4f4` |

- Mate-in-1: 1/1 pass. Mate-in-2: 1/1 pass. Mate-in-3: 1/1 pass.
- Overall corpus pass rate: **7/7**.
- Full test runtime: 9.17 s (two deterministic searches per position).

No mate-in-3 exceptions are needed at the documented depth. The mate-in-3 case
is the slowest non-regression position because the engine must resolve a
five-ply forced line.

Best-move expectations are search baselines, not claims about absolute chess
strength. Evaluation work in tasks 026-028 may intentionally change them; update
the data file with review rather than weakening the runner.

## Commands

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug -L tactical --output-on-failure
./build/cmake/try4_tactical
```
