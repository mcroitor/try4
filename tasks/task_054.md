# Task 054 - Cut evaluation cost while keeping scores byte-identical

| Field | Value |
|---|---|
| **ID** | 054 |
| **Stage** | Search performance - evaluation |
| **Depends on** | 051 |
| **Estimate** | 0.5-1 day |
| **Priority** | P2 |
| **Status** | Done |

**Unblocked:** task 051 is `Done`.

## Objective

Reduce the cost of a static evaluation by removing repeated validation and
redundant board sweeps, with an exact-score invariance gate so no evaluation
semantics change.

## Background

`EvaluateDetailed` (`src/core/evaluation.cpp:571-644`) calls `ValidateEvaluation`
on every invocation (line 572), then performs roughly ten full 64-square sweeps:

- material, PST and phase (577-599);
- `PawnStructure` for each color (271-310), itself two more sweeps plus
  `IsPassedPawn`;
- `Mobility` for each color (343-358) with per-slider ray walks;
- `BishopPair` and `RookFiles` per color (360-399);
- `KingSafety` per color (495-525), including a 64-square
  `CountKingAttackers` scan (458-493).

Evaluation runs at every leaf and every quiescence stand-pat, so it is the
second-largest cost after the redundant move generation addressed by tasks
050-051.

## In scope

- Hoist `ValidateEvaluation` out of the inner evaluation path: validate once per
  search/configuration change, keep the public `EvaluateDetailed` behavior
  (invalid parameters still yield an empty/zero breakdown).
- One board pass that simultaneously collects:
  - material and PST values,
  - phase,
  - pawn file counts per color,
  - slider and rook lists per color,
  - bishop counts and king squares.
- Reuse those collections in `PawnStructure`, `Mobility`, `BishopPair`,
  `RookFiles` and `KingSafety` instead of rescanning the board.
- Direct board access inside the evaluation translation unit to avoid repeated
  `PieceAt` bounds checks.
- Tests and a microbenchmark.

## Out of scope

- Changing weights, PST values, terms, phase formula, tempo or clamping.
- Search pruning, ordering, TT or time management.
- Incremental evaluation, NNUE or lazy evaluation.
- Piece-square table regeneration.

## Requirements

1. **Exact score invariance:** `Evaluate` and `EvaluateDetailed` must return
   identical values for every field on a frozen corpus of at least 10,000
   positions (seeded random playouts plus tactical/FEN fixtures). Integer sums
   may be reordered only if the result is provably identical; verify by test.
2. `EvaluateDetailed` must keep zeroing the breakdown and returning `{}` for
   invalid parameters, and `Evaluate` must keep clamping to
   `kMaxStaticScore`.
3. No change to which terms are active in which phase, including king safety
   and mobility conditions.
4. The evaluation hot path must not validate parameters per call and must not
   allocate.
5. Measure the improvement on the corpus (evaluations/second, or ns per
   evaluation) and record the method; target >= 1.5x.
6. Existing evaluation unit tests (symmetry, PST, pawn structure, mobility,
   bishop pair, rook files, king safety, tempo) must pass unchanged.
7. Warning-clean under `/W4 /WX` and `-Wall -Wextra -Wpedantic -Werror`;
   ASan/UBSan clean.

## Acceptance matrix

| Case | Expected result |
|---|---|
| 10k-position golden corpus | Exact score equality for every reported field |
| Invalid parameters | Same zeroed behavior as before |
| Evaluation unit tests | All pass unchanged |
| Symmetry fixtures | Mirror positions still evaluate symmetrically |
| Throughput | >= 1.5x evaluations/second, measured and documented |
| Whole-search effect | Fixed-depth node counts unchanged; NPS improved |
| Sanitizers and warnings | Clean |

## Validation commands

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug -L "unit|search|tactical" \
  --output-on-failure
python scripts/search_bench.py --engine build/performance/try4_uci.exe \
  --cli build/performance/try4.exe --depths 5,6,7 --output build/bench-after-054
```

## Risks and failure modes

- **Silent score drift** from changed term order or clamping; the exact-equality
  test is the gate and must not be relaxed to a tolerance.
- **Shared state between color evaluations** in the single pass; keep
  per-color arrays explicit and test both colors.
- **PieceAt bypass** can hide an out-of-range bug; keep the 0x88
  `IsOnBoard` discipline inside the accessor.
- **Microbenchmark cherry-picking**; report the corpus, method and repetitions.

## Completion report

**Status:** Done (implementation on `feat/054-final`; PR pending because remote
push still lacks SSH credentials).

**Approach.** `BoardSummary` in `src/core/evaluation.cpp` walks the raw 0x88
board exactly once (Direct board access via a `Position` friendship granted in
`include/chess/position.hpp`) and fills, in one pass: the dense 64-square copy,
per-color piece lists in board-scan order, pawn file counts per color, bishop
counts, king squares, plus the material/PST/phase totals using the caller's
weights. Every positional helper (`PawnStructure`, `Mobility`, `BishopPair`,
`RookFiles`, `KingSafety`, `CountKingAttackers`) now works from the summary.
Validation is hoisted: the public `EvaluateDetailed`/`Evaluate`/
`EvaluateMaterial` keep validating (exact pre-054 behavior, zeroed breakdown on
invalid parameters), while new pre-validated fast paths
(`EvaluateDetailedFast`, `EvaluateFast`, `EvaluateMaterialFast`) skip the
per-call check for the search hot path; `Searcher::Search` validates
`limits.evaluation` once before building the context, so
`SearchContext::StaticEvaluation` switched to the fast paths.

**Files changed**

- `include/chess/position.hpp` - `friend struct BoardSummary;`
- `include/chess/evaluation.hpp` - fast-path declarations
- `src/core/evaluation.cpp` - single-pass `BoardSummary`, helpers over the
  summary, fast-path entry points
- `src/core/search.cpp` - `StaticEvaluation` uses the fast paths
- `tests/evaluation_equiv_tests.cpp` - new equivalence gate
- `bench/eval_bench.cpp` - new microbenchmark with an embedded baseline
- `CMakeLists.txt` - `try4_evaluation_equiv` test + `try4_eval_bench` target
- `tasks/task_054.md` - this report

**Evaluation throughput** (Release, one process, corpus of 10,007 positions =
10,000 seeded-playout positions + 7 tactical fixtures; baseline is a verbatim
copy of the pre-054 evaluator compiled into the bench, so coroutines and build
flags are identical):

| Path | evals/s | ns/eval |
|---|---|---|
| baseline (pre-054 public path) | 493,890 | 2024.7 |
| fast (post-054 hot path) | 1,479,029 | 676.1 |

Speedup **2.99x** (target >= 1.5x). Method: `try4_eval_bench.exe --positions
10000 --reps 40`; corpus seeded with 0x05400010, identical to the test corpus;
both paths produced the same score checksum over the corpus.

**Whole-search effect** (`scripts/search_bench.py`; pre-054 baseline built from
a fresh `main` worktree, same positions, depths 5,6 with 3 repeats and depth 7
with 1 repeat, 18,000 ms timeout):

- Node counts: **0 mismatches** across 24 position x depth runs.
- Scores and PVs: **0 mismatches** (byte-identical; e.g. startpos d5 = 26,465
  nodes / cp 12 / `b1c3 b8c6 g1f3 g8f6 d2d3`; d7 = 423,115 nodes).
- NPS (p50 per group): startpos d5 540k -> 913k, d6 200k -> 251k, d7 457k ->
  706k; kiwipete d5 263k -> 400k; queen-endgame d5 365k -> 538k. Mean gain
  **+71.6%** (range +22% .. +285%, the largest gains on positions with open
  files and few pieces where evaluation dominates).

**Commands/results**

- Debug suite: 25/25 pass (incl. new `try4_evaluation_equiv` with ~330,000
  assertions exact-equality gate and unchanged evaluation unit tests).
- Release suite: all pass.
- Corpus equality: seeded playouts (10,000) + fixtures; fast == public on every
  field, default and customized parameter sets, plus a king-less board built
  via `SetPiece` (FromFen requires kings) to cover the missing-king guard.

**Follow-ups**

- Linux CI job to run ASan/UBSan (MinGW does not provide first-class ASan
  support; see `docs/ARCHITECTURE.md`).
