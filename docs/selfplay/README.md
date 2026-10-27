# Self-play harness

`try4_selfplay` plays the production evaluator against a material-only baseline
under identical search conditions. It is a strength-measurement tool, not a
tactical test: the tactical gate remains `try4_tactical` and the perft gate
remains `try4_perft`.

## Protocol

- Candidate: full evaluation, including material, PSTs, positional terms, and
  tempo.
- Baseline: `EvaluateMaterial`, which uses middlegame material values only.
- Both sides use the same search configuration, fixed-depth iteration, move
  ordering, quiescence, transposition-table size, and node or time limit.
- Every opening is played twice, once with each color assignment.
- A separate searcher and transposition table are created for each game.
- Games are adjudicated as draws at `--max-plies` (160 in the recorded
  baseline). Checkmate, stalemate, fifty-move, threefold repetition, and
  insufficient material use the normal `Position::Result()` rules.
- Fixed-node games are deterministic. Fixed-time games are supported but are
  hardware- and load-dependent by nature.

## Opening set

[`openings.txt`](openings.txt) contains 24 varied opening sequences in UCI
notation. The harness applies them from the standard initial position and
validates every move before starting.

```sh
./build/release/try4_selfplay --validate-openings --openings docs/selfplay/openings.txt
```

## Usage

```sh
# Build a release harness without the test suite.
cmake -S . -B build/release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DTRY4_BUILD_TESTS=OFF \
  -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/release --config Release

# Record a fixed-node baseline.
./build/release/try4_selfplay \
  --openings docs/selfplay/openings.txt \
  --output docs/selfplay/results/baseline-2026-10-24 \
  --nodes 5000 --max-plies 160 --tt-mb 16 --seed 1 \
  --label baseline-2026-10-24 \
  --hardware "Intel Core i5-13600HX, Windows 11 Pro, MSYS2 UCRT64, GCC 16.2.0"

# Quick smoke run.
./build/release/try4_selfplay --limit-openings 2 --nodes 1000 \
  --output /tmp/try4-smoke --label smoke
```

Use `--movetime MS` instead of `--nodes N` for fixed-time games. The two
options are mutually exclusive. `--help` lists every option.

## Output

The output directory contains:

| File | Contents |
|---|---|
| `config.txt` | Harness settings, hardware, compiler, and build type |
| `games.csv` | One row per game with result, termination, plies, node counts, and UCI moves |
| `games.pgn` | PGN with SAN moves and result headers |
| `summary.txt` | Human-readable aggregate |
| `summary.json` | Machine-readable aggregate and approximate SPRT diagnostic |

The `games.csv` and `summary.json` files are byte-for-byte reproducible for a
fixed opening file, seed, and fixed-node setting. The harness has no random
move choice or opening shuffling; `--seed` is recorded for future opening
selection but does not currently alter play.

## Elo calculation

For games `N`, wins `W`, draws `D`, and losses `L`:

```text
score S = (W + 0.5 * D) / N
Elo difference = -400 * log10(1 / S - 1)
```

The reported `95% margin` is a normal approximation using the sample variance
of game scores and the derivative of the Elo conversion. It is not a
confidence interval for a universal rating difference; it describes this
sample. If `S` is exactly 0 or 1, the harness uses a one-half-game clamp and
marks `elo_clipped` in `summary.json`.

The harness also reports a score-normal SPRT-style diagnostic with `H0 = 0
Elo`, `H1 = +20 Elo`, `alpha = beta = 0.05`, and a variance of 0.25 per game.
It is printed for monitoring only; the baseline does not stop early. Draws
enter as 0.5 and the model is an approximation, so do not treat the LLR as a
substitute for a properly sized match.

## Reproducibility check

The recorded baseline was generated with the command above. A smaller
two-opening run repeated twice produced byte-identical `games.csv` and
`summary.json` files. To compare a rerun, keep the opening file, `--nodes`,
`--max-plies`, `--tt-mb`, and engine build identical; compare `games.csv` or
`summary.json` directly.

## Limitations

- Fixed-node self-play at 5,000 nodes is a low-budget baseline, not a claim
  about tournament strength.
- Only one opponent (the material-only evaluator) is measured. A stronger
  evaluation is not automatically a stronger engine against every opponent.
- The material baseline intentionally removes PSTs as well as positional terms
  and tempo.
- The opening set is small: 24 openings, 48 games. The report includes every
  game, including the single loss.
- Time-based games are supported, but the recorded baseline uses fixed nodes
  for reproducibility.
