# Task 054 - Cut evaluation cost while keeping scores byte-identical

| Field | Value |
|---|---|
| **ID** | 054 |
| **Stage** | Search performance - evaluation |
| **Depends on** | 051 |
| **Estimate** | 0.5-1 day |
| **Priority** | P2 |
| **Status** | Blocked |

**Blocker:** waiting for task 051. Flip to `Ready` when 051 is `Done`.

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

Return status, files changed, exact-score diff result, throughput delta, node
and NPS deltas, commands/results, acceptance matrix and follow-ups.
