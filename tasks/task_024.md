# Task 024 — Implement the transposition table and time management

| Field | Value |
|---|---|
| **ID** | 024 |
| **File** | `task_000000000000000000000000000024.md` |
| **Stage** | 4 — Search foundation |
| **Depends on** | 022, 023 |
| **Estimate** | 2 days |
| **Priority** | P1 |
| **Status** | Done |

## Objective

Reuse search results safely and make the engine respect clock-based limits
with a responsive stop mechanism.

## Context

The search currently supports depth/node limits only. UCI needs clock
handling and a stop flag. The transposition table uses the Zobrist hash
from task 019.

## Scope

### In scope

- Implement a transposition table with depth, score, bound, and best move.
- Implement clock-based time allocation and a soft/hard limit.
- Add an atomic stop flag and node polling.
- Add tests for TT behavior and time-limit compliance.

### Out of scope

- UCI command handling; tasks 029–031.
- Multithreaded search; task 035.
- Advanced replacement/aging policies; task 034 if needed.

## Requirements

1. TT entries store key, depth, score, bound type, and best move; handle mate-score adjustment on store/probe.
2. Bound the TT memory and support clearing/resizing.
3. Do not treat a TT score as exact unless the bound and depth allow it.
4. Time manager handles `movetime`, `wtime`/`btime`, increments, and `movestogo`; compute a soft target and a hard ceiling.
5. Poll the atomic stop flag every N nodes; stop must produce a legal best move promptly.
6. Never exceed the hard limit by more than 50 ms in tests.
7. Add tests: TT probe/store round-trip, mate-score adjustment, fixed-depth result unchanged by TT, movetime compliance, and stop responsiveness.

## Deliverables

- Transposition table
- Time manager and stop flag
- TT/time tests

## Acceptance criteria

- [ ] TT hits do not change the correct fixed-depth result.
- [ ] Mate scores survive TT storage/probe correctly.
- [ ] Search respects `movetime` and hard clock limits within the tolerance.
- [ ] `stop` returns a legal move within 100 ms in stress tests.
- [ ] All existing search/tactical tests pass.

## Validation

```sh
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug -L search --output-on-failure
```

Expected: TT and time-management tests pass.

## Constraints and failure modes

- Do not add UCI parsing in this task.
- Do not use wall-clock assertions with tight tolerances in correctness tests; separate timing tests.
- Do not share the TT across concurrent searches until threading is designed.

## Completion notes

- Added `chess/transposition_table.hpp` and `src/core/transposition_table.cpp`:
  a bounded, direct-mapped table with full-key verification, depth, bound,
  mate-adjusted score, and best-move storage. It supports `Clear()` and
  `Resize()` and deliberately uses simple replacement until task 034.
- Added `ScoreToTT`/`ScoreFromTT` and unit tests for positive/negative mate
  distances. Search stores only bounds proven by its fail-soft result and never
  takes a root cutoff, keeping the reported PV rooted in a real move.
- Added `chess/time_manager.hpp` and `src/core/time_manager.cpp` with soft and
  hard budgets. `movetime` takes precedence over clock fields; clocks use
  `movesToGo` (default 30) and increments, reserving a small safety margin.
- Search polls the atomic stop flag and hard deadline every 32 nodes, checks
  the soft target only between iterations, and uses a 64-iteration cap when a
  time budget is present. The last completed iteration is always retained.
- Added seven TT/time tests: round-trip, collision rejection, resize/clear,
  mate adjustment, fixed-depth TT invariance, movetime/clock compliance, and
  threaded stop latency. Total suite is now 15 executables and 119 cases.
- Full Debug and full ASan/UBSan suites pass; stop latency was 30 ms and
  100 ms movetime completed within the 50 ms tolerance in local tests.

## Completion report

**Status:** done

**Files changed:**
- `include/chess/transposition_table.hpp`, `src/core/transposition_table.cpp` — bounded TT, full-key probe, mate-score conversion.
- `include/chess/time_manager.hpp`, `src/core/time_manager.cpp` — soft/hard clock budgeting.
- `include/chess/search.hpp`, `src/core/search.cpp` — TT cutoffs/ordering, time deadlines, polling.
- `tests/search_time_tests.cpp`, `CMakeLists.txt` — new test target and Threads linkage.
- `tests/README.md`, `README.md`, `docs/ARCHITECTURE.md`, `docs/HARDENING.md` — inventory, status, design, and sanitizer records.

**Commands and results:**
- `cmake -S . -B build/cmake ... -DTRY4_WARNINGS_AS_ERRORS=ON && cmake --build build/cmake --config Debug` — pass.
- `ctest --test-dir build/cmake -C Debug -L search --output-on-failure` — 2/2 pass (1.84 s).
- `ctest --test-dir build/cmake -C Debug --output-on-failure` — 15/15 pass (50.68 s).
- WSL2 ASan/UBSan full suite — 15/15 pass (145.05 s), no reports.

**Acceptance criteria:**
- TT fixed-depth invariance — pass (`try4_search_time`).
- Mate scores across TT — pass (direct conversion and stored-entry tests).
- Movetime/hard clock tolerance — pass (100 ms and 200 ms budgets tested under 50 ms tolerance).
- Legal move after stop within 100 ms — pass (threaded stop test, 30 ms observed).
- Existing search/tactical tests — pass (all existing search tests; tactical suite is task 025).

**Risks or follow-ups:** TT replacement is intentionally direct-mapped; aging is deferred to task 034. The TT key mixes in the halfmove clock but does not encode repetition history, so path-dependent draw reuse should be revisited with the tactical/TT regression work. Next action: task 025.

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
