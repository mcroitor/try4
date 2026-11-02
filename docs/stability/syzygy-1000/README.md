# 1,000-game Syzygy stability report

This directory preserves the task-035 protocol-level stability run for the
first Syzygy integration. It is a stability check between two identical engine
processes, not a strength measurement.

## Provenance

- Engine: `try4 0.1.0`, `build/performance/try4_uci.exe`
  (Windows Release, GCC 16.2.0, CMake 4.4.3, Ninja).
- Cli judge: `build/performance/try4.exe`.
- Harness: `scripts/uci_stability.py`.
- Tablebase: local `c:\games\see\Syzygy345` 3-4-5 Syzygy set with 145 `.rtbw`
  and 145 `.rtbz` files.
- Hardware: Intel Core i5-13600HX, Windows 11 build 26200.
- Date: 2026-10-31.

```text
python scripts/uci_stability.py --engine build/performance/try4_uci.exe \
  --cli build/performance/try4.exe --games 1000 --workers 4 \
  --base-ms 1000 --inc-ms 50 --moves-to-go 200 --max-plies 120 \
  --syzygy-path c:\games\see\Syzygy345 \
  --output docs/stability/syzygy-1000 \
  --label syzygy-1000 --seed 35 \
  --hardware "Windows, local Syzygy345 3-4-5 set"
```

Both engines set `OwnBook=false` and `Threads=1` and received
`SyzygyPath=c:\games\see\Syzygy345` through `setoption`.

## Result

```text
games: 1000/1000 in 1327.4s
results W/D/L (white perspective): 444/333/223
terminations: resignation 667, max-plies 290, threefold-repetition 42,
              insufficient-material 1
move wall ms: count 86313, min 1.244, p50 52.125, p95 103.466,
              p99 106.256, max 116.005, mean 58.305
stop latency ms: count 40, min 0.089, p50 0.156, p95 0.298,
                 p99 1.037, max 1.037, mean 0.198
ready latency ms: count 2040, min 0.113, p50 2.136, p95 3.33,
                  p99 4.142, max 7.389, mean 2.221
```

There were no illegal moves, no crashes, no hangs, no time forfeits and no
unsupported-command diagnostics. The 667 `resignation` terminations are the
task-058 resignation option firing from the normal search, not protocol
failures; they are preserved here for transparency.

## Limits of this evidence

- Identical engines self-play; results do not measure playing strength.
- The run was protocol-level on Windows; no GUI transcript is claimed.
- Syzygy probing affects endgame positions only, so the run is a stability
  and termination-behavior check, not a tactical-quality gate.
- The archived `stability.json` keeps the per-game summaries and failure
  record for audit purposes.