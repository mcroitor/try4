# Lazy SMP proof of concept (task 048)

Status: **PoC implemented, evidence partially recorded; recommendation recorded
below.** This document is the task 048 decision instrument: design, race-safety
argument, commands and measurements with the go/no-go at the end.

## Scope

The PoC is enabled with the CMake option `TRY4_SMP_POC=ON` (default `OFF`).
With the option off, all artifacts stay 0.1.0-compatible: the `Threads` UCI
option advertises `min 1 max 1`, the INI parser rejects other values with the
task 043-style diagnostic, and no SMP code is compiled in.

With the option on:

- `Threads` accepts 1-4 through `setoption` and `[uci] threads`; default 1.
- `go` starts `Threads` search workers. Worker 1 is the existing main worker
  (the authoritative clock owner and result sender); the rest are helpers.
- Variant A (required, implemented): **private tables**. Each helper owns a
  `Searcher` with the configured hash divided across workers (minimum 1 MB per
  worker). Helpers disable move ordering so their trees differ from the main
  worker's without touching any ordering policy.
- Variant B (the shared-table spike) is **not implemented** in this pass; see
  the limitations.

## Race-safety argument (variant A)

Cross-thread channels:

1. The broadcast stop flag (`stop_flag_`, an `std::atomic_bool`, the existing
   main-worker flag) - relaxed load/poll, every 32 nodes inside the search.
2. The helper snapshot: a small mutex-guarded struct holding one published
   iteration {depth, score, pv}. A helper claims the snapshot only with a
   strictly deeper depth, or the same depth with a strictly better score;
   equal-depth/equal-score results never replace the main worker's iteration.
3. Per-worker results: each helper's final `SearchInfo` occupies its own
   preallocated slot; written before join, read after join - the join is the
   happens-before edge.

Everything else is per-worker: Searcher (and transposition table), search
context, iteration counters. There is no shared mutable search state, so with
variant A the only synchronization is the stop flag and the snapshot mutex.

Lifecycle: helpers are joined inside the search worker before any `bestmove`
is emitted, so `stop`/`quit`/`ucinewgame` never race helper teardown. The
helper's best move is validated against the root position (`IsLegal`) before
it may be reported; a helper that fails validation falls back to the main
worker's iteration.

Aggregation: the reported `nodes` is the sum of all workers' final node
counts; `nps`/`time` in helper lines reflect the main worker's clock in the
final output. `bestmove` is emitted exactly once per `go`, by the search
worker thread, after the join.

## Commands

```sh
# Option-off baseline (frozen behavior)
cmake -S . -B build/smp-off -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/smp-off
ctest --test-dir build/smp-off --output-on-failure

# Option-on PoC build
cmake -S . -B build/smp-on -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTRY4_BUILD_TESTS=ON -DTRY4_SMP_POC=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/smp-on
ctest --test-dir build/smp-on -C Release -L "uci|config" --output-on-failure

# Stress and benchmark drivers
python scripts/smp_stress.py --engine build/smp-on/try4_uci.exe \
  --cycles 1000 --threads 4 --output build/smp-stress-4.json
python scripts/smp_benchmark.py --engine build/smp-on/try4_uci.exe \
  --threads 1,2,4 --depth 6 --output build/smp-bench.json
```

## Measurements

(Fill from `scripts/smp_benchmark.py` runs on the reference machine; report
medians with the hardware label. Fixed-node searches never predict; helper
iterations are independent ID loops with the same budgets.)

## Limitations

1. **Variant B is not implemented.** A shared transposition table on this
   32-bit-entry direct-mapped scheme requires either per-bucket locks (measured
   contention risk at 4 helpers) or a seqlock/atomic generation scheme. The
   spike is the first item if the go decision is taken.
2. **ThreadSanitizer is not available in this MinGW environment.** The task's
   mandatory TSan-clean requirement can only be discharged on the Linux CI;
   the mutex+atomic scheme above is documented for that platform.
3. Helper iterations stream no `info` lines (the main worker is the only
   authoritative streamer), so the GUI sees the main worker's iteration
   stream plus exactly one bestmove.

## Go/no-go

Recorded after the validation runs; see the task 048 completion report.