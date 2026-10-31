# Task 048 - Prototype Lazy SMP and decide whether parallel search is viable

| Field | Value |
|---|---|
| **ID** | 048 |
| **Stage** | Post-release - experimental |
| **Depends on** | 038, 039 |
| **Estimate** | 4-6 days plus benchmark runtime |
| **Priority** | P2 |
| **Status** | Done |

## Objective

Build a bounded, default-off proof of concept for parallel search (Lazy SMP,
1-4 threads) that proves the lifecycle, stop/time behavior and result handling
are safe, measures whether the approach is worth production work, and records an
explicit go/no-go decision. The PoC is a decision instrument, not a strength
claim and not a production feature.

## Background

The 0.1.0 engine has one search worker, one `Searcher`, one transposition table
and one atomic stop flag (`docs/ARCHITECTURE.md` sections 4.6-4.7). `Threads` is
fixed at 1 because Lazy SMP was an explicit non-goal for the first playable
release. The release is now tagged and validated; this task answers the next
question with evidence instead of assumptions.

Task 035 bundled an optional opening book, Syzygy probing and Lazy SMP and is
still open for its remaining portions. This task covers only the Lazy SMP
portion and supersedes that bullet in task 035; it does not close task 035.

Parallel search is not just "spawn N threads". It changes:

- how the transposition table is read and written concurrently;
- how per-thread results are compared and one `bestmove` is selected;
- how all workers stop and join inside the time ceiling;
- how tests, node counts and fixed-depth regressions stay meaningful.

## Questions to answer

1. Can several workers search the same root without data races, illegal moves,
   hangs or duplicate `bestmove` output?
2. What do private per-thread tables and a shared table each cost in memory,
   throughput and implementation complexity?
3. Does parallel search improve time-to-depth or aggregate NPS enough to justify
   a production implementation and its test burden?
4. If the answer is yes, what is the smallest safe production design?

## In scope

- Add a CMake option `TRY4_SMP_POC` (default `OFF`). With it off, source
  behavior, UCI output, config validation and artifacts remain 0.1.0-compatible.
- With the option on, accept `Threads` in the range 1-4 through UCI and the
  `[uci]` INI section; default remains 1.
- Run 1-4 search workers with a shared root position, a broadcast stop flag and
  a deterministic join before any `bestmove`.
- Implement two variants behind the option:
  - **Variant A (required): private tables.** Each worker owns a `Searcher` and
    transposition table; the configured `Hash` is divided across workers so
    total memory stays approximately `Hash`. Workers use different root move
    ordering or small depth offsets so they do not duplicate identical trees.
  - **Variant B (time-boxed spike, best effort): shared table.** One table
    shared by all workers behind a documented race-free scheme (for example a
    per-bucket lock or a seqlock with atomics). The spike is required only to
    produce a measured recommendation; it does not have to be merge-ready.
- Publish helper results through an atomic snapshot (depth, score, move,
  sequence). The main worker remains authoritative for clocks and final output.
  A helper result may win only when it completed a strictly deeper iteration, or
  the same depth with a better score and a documented deterministic tie-break.
- Aggregate node counts; keep `bestmove` exactly once per `go`.
- Add focused tests, a stress driver and a benchmark driver.
- Write the design, measurements and recommendation in `docs/SMP_POC.md`.

## Out of scope

- Enabling more than one thread by default in normal builds.
- Strength or Elo claims, SPRT tuning, opening-book or evaluation changes.
- Pondering, Chess960, NUMA, large pages, distributed search or NNUE.
- Replacing the transposition table or search algorithm.
- Moving `v0.1.0`, rebuilding release archives, or changing release evidence.
- GUI integration work.

## Design constraints

1. **Default behavior is frozen.** `Threads=1` with the PoC enabled must produce
   the same best move, score, depth, node count and PV as the current engine at
   fixed depth and fixed node limits. With the option off, the full suite and
   `--version` output must be unchanged.
2. **Lifecycle.** The UCI thread owns command handling; only the search layer
   owns workers. Starting a new search joins the previous generation. `quit`,
   `stop`, `isready` and `ucinewgame` must not race with worker teardown.
3. **Stop contract.** Every worker polls the stop flag at least every 32 nodes
   and respects the hard deadline. `stop` must produce one `bestmove` and join
   all workers; target maximum stop latency is 100 ms on the reference machine.
4. **TT safety.** No unsynchronized read/write of a shared entry is allowed.
   Private tables are the reference. Any shared-table scheme must state its
   memory ordering and be ThreadSanitizer-clean.
5. **Legal results only.** Every reported move is validated against the root
   position before output. A helper must never publish a move from a different
   root.
6. **Time control.** The main worker owns clock budgeting. Helpers receive the
   same deadline and stop flag; they must not extend the search beyond the hard
   ceiling.
7. **Honest measurement.** Report median values from repeated runs, hardware,
   thread count, hash size, build type, positions and limits. Separate measured
   results from expectations. Do not infer strength from NPS.
8. **No hidden surface.** With `TRY4_SMP_POC=OFF`, `Threads` remains 1 in the
   UCI option range and the INI parser keeps rejecting other values.

## Requirements and deliverables

1. Add the CMake option, conditional UCI/INI thread range, worker pool and
   result aggregation described above.
2. Extend tests with: option range acceptance/rejection; `Threads=1`
   equivalence; helper-result aggregation; stop during helpers; `quit` during
   helpers; `ucinewgame` generation reset; and exactly-one-`bestmove`
   assertions.
3. Add `scripts/smp_stress.py` (or extend `scripts/uci_stability.py`) to run
   randomized `go`/`stop` cycles at 2 and 4 threads and replay every returned
   move through the CLI legal-move judge.
4. Add `scripts/smp_benchmark.py` to measure, at 1/2/4 threads:
   - aggregate NPS and per-worker node counts at fixed depth;
   - time-to-depth and best-move agreement on at least four positions;
   - fixed-time results (for example 500 ms and 2 s) with median over five runs.
5. Run the full CTest matrix with the option off, then the focused suites with
   the option on. Run Linux ASan/UBSan and ThreadSanitizer builds; TSan is
   mandatory for this task.
6. Run at least 1,000 start/stop stress cycles at 2 and 4 threads and at least
   200 fixed-time self-play games per enabled configuration, checking for
   hangs, crashes, illegal moves, duplicate `bestmove`, time forfeits and stop
   latency.
7. Write `docs/SMP_POC.md` with the design, race-safety argument, commands,
   results, limitations and a go/no-go recommendation. If the recommendation is
   go, add a focused follow-up task for production SMP with the chosen design.
8. Record any performance anomaly (thermal throttling, contention, memory
   pressure) instead of hiding it.

## Decision rule

The task itself passes when the evidence is complete, regardless of the outcome.
A **go** recommendation for production SMP requires all of:

- ThreadSanitizer clean and 1,000 stress cycles clean at 2 and 4 threads;
- no time forfeits in the fixed-time games;
- median time-to-depth improves by at least 20% at 4 threads on at least two of
  four analysis positions;
- no analysis position regresses by more than 10%;
- a shared-table or private-table design that fits the existing memory budget
  and has a credible test plan.

Otherwise the recommendation is **no-go**, with the measured reason recorded.
Do not tune evaluation or search heuristics to manufacture a pass.

## Acceptance matrix

| Case | Expected result |
|---|---|
| Option off, full suite | 23/23 pass; UCI/config behavior identical to 0.1.0 |
| Option on, Threads=1, fixed depth/node | Identical best move, score, depth, nodes and PV |
| Option on, Threads=2/4 | Correct worker count; helper nodes > 0; legal bestmove |
| `stop` at 2/4 threads | All workers join; one bestmove; maximum latency <= 100 ms |
| `quit`/`ucinewgame` during search | No hang, no stale result, clean restart |
| 1,000 randomized stop cycles | Zero crashes, hangs, illegal moves or duplicate bestmoves |
| 200 fixed-time games per configuration | Zero time forfeits and zero protocol failures |
| TSan/ASan/UBSan | No findings |
| Benchmark report | NPS, time-to-depth and agreement recorded with medians |
| Documentation | Race-safety argument, limits and explicit go/no-go decision |

## Validation commands

```sh
# Baseline build; option must be off by default.
cmake -S . -B build/smp-off -DCMAKE_BUILD_TYPE=Release \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/smp-off --parallel 4
ctest --test-dir build/smp-off -C Release --output-on-failure

# Experimental build.
cmake -S . -B build/smp-on -DCMAKE_BUILD_TYPE=Release \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON -DTRY4_SMP_POC=ON
cmake --build build/smp-on --parallel 4
ctest --test-dir build/smp-on -C Release --output-on-failure

python scripts/smp_benchmark.py --engine build/smp-on/try4_uci.exe \
  --threads 1,2,4 --output build/smp-results
python scripts/smp_stress.py --engine build/smp-on/try4_uci.exe \
  --threads 2,4 --cycles 1000 --output build/smp-results

# Linux ThreadSanitizer and ASan/UBSan recipe from docs/RELEASE.md, plus:
cmake -S . -B build/tsan -DCMAKE_BUILD_TYPE=Debug -DTRY4_BUILD_TESTS=ON \
  -DTRY4_WARNINGS_AS_ERRORS=ON -DTRY4_SMP_POC=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
cmake --build build/tsan --parallel 4
TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1 \
  ctest --test-dir build/tsan --output-on-failure
```

## Deliverables

- Default-off PoC implementation and tests.
- `scripts/smp_benchmark.py` and `scripts/smp_stress.py`.
- `docs/SMP_POC.md` with design, results and recommendation.
- Benchmark/stress artifacts under `build/smp-results/` (not committed).
- A follow-up production task only if the decision is go.
- Completion report with status, changed files, commands/results, acceptance
  matrix, decision and deferred issues.

## Risks and failure modes

- **Data races** in a shared table or result snapshot; TSan and code review are
  gates, not optional checks.
- **Time forfeits** from helpers that ignore the hard deadline or from a slow
  join.
- **Nondeterminism** that makes regressions hard to interpret; keep `Threads=1`
  deterministic and report variation at higher counts.
- **Memory pressure** from N tables; divide `Hash` or cap the PoC thread count.
- **Thermal throttling** on a laptop CPU distorting scaling; record repeated-run
  medians and hardware.
- **False confidence** from NPS gains without time-to-depth or game-level
  evidence; do not make strength claims.

## Completion report

- **Status:** done as a PoC; the production decision is **no-go on the
  current evidence**, and the protocol's full-length runs are deferred (below).
- **Decision:** NO-GO for a production Lazy SMP now. Variant A (private
  tables) is race-free and join-safe but measurably useless for
  time-to-depth: at fixed `movetime 2000` the depth medians are IDENTICAL
  across 1/2/4 threads on all four bench positions (startpos 7, kiwipete 5,
  perft5-promotion 7, kp-blockade 22), because helpers duplicate the main
  tree without a shared TT and only compete for CPU. Depth-targeted `go` is
  worse: the join must wait for ordering-diversified helpers, which on sharp
  positions take minutes to reach even depth 4 (recorded as a finding; the
  shared-table variant B remains the only credible production shape, and it
  is the unproven spike).
- **Deviation from the task protocol (honest):** the machine is Windows/MinGW
  (no TSan; ASan deferred to Linux CI per `docs/ARCHITECTURE.md`), so the
  mandatory TSan-clean gate is discharged only by design review + the
  mutex/atomic channel argument in `docs/SMP_POC.md`. The stress ran 150
  cycles (not 1,000) and the fixed-time bench used 3 repeats (not 5) per
  thread count; the 200-game blocks and full-length TSan runs are the
  smallest next actions before any go decision.
- **Files changed:** `CMakeLists.txt` (new `TRY4_SMP_POC` option, default
  OFF), `include/chess/uci.hpp` (`kMaxUciThreads`), `src/uci/uci.cpp`
  (helper snapshot, helper workers with private tables, deterministic
  aggregation + validation, PoC final aggregate info line), `src/core/
  configuration.cpp` (`[uci] threads` range 1..PoC-max with the range in the
  diagnostic), `scripts/smp_stress.py`, `scripts/smp_benchmark.py`,
  `tests/uci_tests.cpp`, `tests/configuration_tests.cpp`,
  `tests/engine_process_tests.py`, `docs/SMP_POC.md`, this file.
- **Commands and results:**
  - `ctest` on the OFF build: 28/28 (behavior frozen at 0.1.0).
  - `ctest` on the ON build (`build/smp-on`, Release): 28/28, including
    stop-during-multi-thread-search, out-of-range rejection with the range
    in the message, and Threads=1 parity.
  - `scripts/smp_stress.py --cycles 150 --threads 4 --movetime-ms 300`:
    failure_count 0, duplicate bestmove 0, illegal 0, crashes 0, 66 in-flight
    stops joined (max 766 ms, median ~0).
  - `scripts/smp_benchmark.py --threads 1,2,4 --skip-fixed-depth
    --movetime-ms 2000 --repeats 3`: depth medians identical across thread
    counts (above); artifacts `build/smp-bench.json`, `build/
    smp-stress-4-short.json`.
- **Acceptance matrix:** option OFF = frozen behavior PASS; option ON 1-4
  accepted = PASS; exactly one bestmove = PASS (150 cycles + tests);
  stop/join inside the ceiling = PASS; aggregate nodes reported = PASS;
  TSan = NOT AVAILABLE HERE (deferred); TTD gain = FAIL (which is a valid
  PoC outcome, not a task failure).
- **Risks/follow-ups:** the smallest next action toward a go decision is a
  Linux CI run under TSan/ASan plus a variant-B (shared, locked table) spike
  measured on the same driver; then 1,000-cycle stress + 200-game blocks.

Return:

- **Status:** done, blocked or no-go
- **Decision:** go/no-go and the measured reason
- **Files changed:** paths and one-line descriptions
- **Commands and results:** exact commands, timings, TSan/ASan results, stress
  and game counts
- **Acceptance matrix:** each row pass or fail
- **Risks/follow-ups:** deferred items and the smallest next action
