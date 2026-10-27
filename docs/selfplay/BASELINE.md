# Evaluation baseline — 2026-10-24

This is the first recorded self-play baseline. It compares the full evaluator
(task 027) with a material-only evaluator. No evaluation weights were tuned
against the tactical suite or these games.

## Conditions

| Field | Value |
|---|---|
| Candidate | Full evaluation (material, PSTs, positional terms, tempo) |
| Baseline | Material-only (`EvaluateMaterial`) |
| Games | 48 (24 openings, both colors) |
| Time control | None; 5,000 nodes per move |
| Search | Iterative deepening up to depth 64, ordering and quiescence enabled |
| Transposition table | 16 MB per game/engine |
| Draw adjudication | 160 plies |
| Opening set | [`../openings.txt`](../openings.txt) |
| CPU | Intel Core i5-13600HX |
| OS | Windows 11 Pro |
| Compiler | GNU 16.2.0 (MSYS2 UCRT64) |
| Build | Release, `TRY4_WARNINGS_AS_ERRORS=ON` |
| Seed | 1 (no random move choice or opening shuffle) |
| Runtime | 376.03 seconds |

The exact settings are recorded in
[`results/baseline-2026-10-24/config.txt`](results/baseline-2026-10-24/config.txt).

## Result

From the full evaluator's perspective:

| Games | Wins | Draws | Losses | Score | Elo difference | 95% margin |
|---:|---:|---:|---:|---:|---:|---:|
| 48 | 42 | 5 | 1 | 0.9271 (92.71%) | +441.72 | ±149.77 |

Color split:

| Full evaluator as | Wins | Draws | Losses |
|---|---:|---:|---:|
| White | 23 | 1 | 0 |
| Black | 19 | 4 | 1 |

Terminations: 43 checkmate, 4 max-plies draws, 1 threefold repetition.
Node totals: 9,727,870 for the full evaluator and 9,643,008 for the material
baseline. Both sides completed essentially the same node budget; the full
evaluator did not win by searching more nodes.

The approximate score-normal SPRT diagnostic gave LLR 2.278 against a
continuation band of `[-2.944, 2.944]` for `H1 = +20 Elo`. The fixed sample
therefore does not reach that diagnostic's stopping boundary. This is a
low-budget match against one opponent, not a tournament-strength claim.

## Interpretation

The full evaluator clearly outperformed material-only at 5,000 nodes per move
in this sample. The result is reported without excluding the one loss or the
four adjudicated draws. The color split is noticeably better for the full
evaluator as White, so a larger match is needed before treating the Elo value
as stable.

The material-only baseline removes PSTs as well as positional terms and tempo.
The measured difference therefore covers the complete task-026/027 evaluation
stack, not only the positional terms.

## Reproduction

```sh
cmake -S . -B build/release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DTRY4_BUILD_TESTS=OFF \
  -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/release --config Release
./build/release/try4_selfplay \
  --openings docs/selfplay/openings.txt \
  --output docs/selfplay/results/baseline-2026-10-24 \
  --nodes 5000 --max-plies 160 --tt-mb 16 --seed 1 \
  --label baseline-2026-10-24 \
  --hardware "Intel Core i5-13600HX, Windows 11 Pro, MSYS2 UCRT64, GCC 16.2.0"
```

A two-opening run repeated twice produced byte-identical `games.csv` and
`summary.json` files. The recorded baseline uses fixed nodes; `--movetime` is
supported for fixed-time experiments but is not bit-reproducible.

## Stored artifacts

- [`results/baseline-2026-10-24/summary.txt`](results/baseline-2026-10-24/summary.txt)
- [`results/baseline-2026-10-24/summary.json`](results/baseline-2026-10-24/summary.json)
- [`results/baseline-2026-10-24/games.csv`](results/baseline-2026-10-24/games.csv)
- [`results/baseline-2026-10-24/games.pgn`](results/baseline-2026-10-24/games.pgn)
- [`results/baseline-2026-10-24/config.txt`](results/baseline-2026-10-24/config.txt)

## Limitations and next action

This is a 48-game, 5,000-node baseline. It is large enough to demonstrate a
large difference from the material-only baseline, but not to estimate a stable
rating or justify weight tuning. Task 028 is complete; task 029 is the next
backlog item (UCI adapter skeleton).
