# 1,000-game UCI stability report

This directory preserves the protocol-level stability run used for the try4
0.1.0 release gate. It is a stability check between two identical engine
processes, not a strength measurement.

## Provenance

- Engine: `try4 0.1.0`, `build/feature-check/try4_uci.exe` (Windows Release,
  GCC 16.2.0, CMake 4.4.3, Ninja).
- Source: the v0.1.0 release commit; see
  [`../../RELEASE_VALIDATION.md`](../../RELEASE_VALIDATION.md) for the commit,
  archive checksums and final gate matrix.
- Harness: `scripts/uci_stability.py`.
- Hardware: Intel Core i5-13600HX, Windows 11 build 26200.
- Date: 2026-10-26.

```text
python scripts/uci_stability.py --engine build/feature-check/try4_uci.exe \
  --games 1000 --workers 4 \
  --output build/feature-validation/stability-1000 \
  --stop-probe-every 25 \
  --hardware "Intel i5-13600HX / Windows 11 26200" \
  --label v0.1.0-final
```

The harness played two real UCI processes with fixed clocks of 1000 ms plus
50 ms increment and `movestogo 200`. Every engine move was replayed through the
debug CLI, whose legal move generation is covered by the perft suite. The
opening file SHA-256 was
`f3b91de4be8f134468fe651fcb89c76bf416337ef38b76ca9e944aa03ea914ec`;
seed 1; at most 120 plies per game.

## Result

```text
games: 1000/1000 in 1290.4s (46.5 games/minute)
results W/D/L (white perspective): 248/488/264
terminations: checkmate 512, max-plies 399, threefold-repetition 85,
              insufficient-material 1, stalemate 3
total plies: 99293 (mean 99.29)
protocol failures: 0
move wall ms: count 92839, min 1.107, p50 58.831, p95 60.858,
              p99 61.931, max 67.745, mean 51.767
stop latency ms: count 40, min 0.145, p50 0.212, p95 0.424,
                 p99 0.516, max 0.516, mean 0.229
ready latency ms: count 2040, min 0.007, p50 1.537, p95 1.982,
                  p99 2.815, max 4.776, mean 1.549
```

No crashes, hangs, illegal moves, time forfeits, unsupported-command
diagnostics, duplicate `bestmove` lines or per-move deadline violations were
recorded. The 67.745 ms maximum move time is below the 1,000 ms per-move
violation threshold and the clocks retained a wide margin.

## Files

| File | SHA-256 |
|---|---|
| `games.csv` | `6a0a788693414126f8f3abd22ab0bcd6e3b9b85c558f3da7c68a971ff66ea935` |
| `games.pgn` | `12acd9f0af34d045afc3888e58b7737fec5788f728f1e92b800be4bdcbdfe049` |
| `stability.json` | `cafd4479a8ee44213042dd2fc28176369f1d94bbf8de16d0b996105000f8b649` |
| `summary.txt` | `66de627d34c687863a3e10b3e42d0e4acd0884f89c2ea4d8f4948da2210b2d45` |

`games.csv` contains the validation-ready move strings. `games.pgn` is provided
for inspection. `stability.json` retains settings, latencies, terminations and
per-game summaries.

## Limits of this evidence

- Identical engines self-play; results do not measure playing strength.
- The run used the protocol harness, not a GUI. The owner separately reported
  stable CuteChess play; no GUI transcript was archived.
- Only Windows was exercised for this 1,000-game run. Linux correctness was
  covered by the Release and sanitizer suites.
- The harness records wall-clock wait for `bestmove`; it does not inspect
  kernel scheduling or GUI-side latency.
