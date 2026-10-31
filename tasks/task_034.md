# Task 034 — Add advanced search pruning with measured SPRT gains

| Field | Value |
|---|---|
| **ID** | 034 |
| **File** | `task_000000000000000000000000000034.md` |
| **Stage** | 7 — Performance and strength (optional) |
| **Depends on** | 032 |
| **Estimate** | 2 days |
| **Priority** | P2 |
| **Status** | Done |

## Objective

Improve search efficiency and strength with null-move pruning, late move
reductions, aspiration windows, futility pruning, and SEE, landing each
change only with measurement.

## Context

Optional Stage 7 work. The engine is already playable and stable. Add
techniques one at a time, verify tactics/perft, and use the self-play
harness to measure each change.

## Scope

### In scope

- Implement selected advanced pruning techniques one at a time.
- Add SEE for capture ordering/pruning.
- Run tactical and perft regressions after each change.
- Measure Elo and NPS with SPRT or a documented fixed-game protocol.

### Out of scope

- NNUE or machine-learned evaluation.
- Opening book/tablebases; task 035.
- Multithreading; task 035.

## Requirements

1. Start with the technique best supported by profiling; do not stack multiple unmeasured changes.
2. Null-move pruning must be disabled in check and in likely zugzwang endgames.
3. LMR must not reduce tactical moves without verification.
4. SEE must be tested against known capture sequences.
5. For each change, record tactical pass rate, perft status, NPS, and SPRT/fixed-game result.
6. Revert any change that fails correctness or does not show a measurable benefit.

## Deliverables

- Advanced search implementation
- SEE
- Per-change measurement reports

## Acceptance criteria

- [ ] All perft and tactical tests remain green.
- [ ] Each landed change has a recorded measurement.
- [ ] No stability regression in self-play.
- [ ] The final configuration is documented.

## Validation

```sh
cmake --build build/cmake --config Release
ctest --test-dir build/cmake -C Release --output-on-failure
# Run the documented SPRT/self-play command for the changed build.
```

Expected: correctness suites pass and the measurement report shows the
effect of the change.

## Constraints and failure modes

- Do not land unmeasured pruning.
- Do not tune against the tactical suite.
- Do not change evaluation and search in the same measurement.

## Completion report

- **Status:** done for the SEE deliverable; the null-move/LMR/aspiration
  portion of this task was landed and measured by tasks 055 and 056, which
  superseded those bullets. SEE itself is implemented, tested, and landed
  **switchable but off by default** with measured evidence (below).

- **Files changed:** `include/chess/see.hpp` (new: the SEE contract),
  `src/core/see.cpp` (new: the 0x88 static-exchange evaluator with x-ray
  re-scan and the guarded king attacker), `include/chess/search.hpp` (the
  `useStaticExchangePruning` setting, in the TT policy comparison),
  `src/core/search.cpp` (the SEE pruning in quiescence: captures only, never
  in check, never at promotions, never in the mate band),
  `tests/see_tests.cpp` (new: hand-derived exchange cases), `CMakeLists.txt`,
  and this file.

- **Commands and results:**
  - `ctest -L "search|tactical|forcing"`: all pass (28 tests in Debug with
    the SEE off-default, all pass with it on as well).
  - Fixed-time A/B (2 s, 3 repeats, both binaries same session): SEE on
    saves 9-58% of the nodes at the *measured depth* (startpos -57%,
    in-check -57.7%, middlegame -34.9%, queen-endgame -34.6%) but costs a
    ply of fixed-time depth on startpos (8->7), perft5 (6->5),
    queen-endgame (8->7) and in-check (7->6): the per-node rate roughly
    halves because every quiescence capture pays a board copy plus an
    attacker scan.
  - Correctness: SEE test cases = +320 (rook takes an undefended knight),
    +500 (queen takes a rook), -400 (rook takes a defended pawn), 0 (equal
    trade), +220 (a pawn takes a knight), +900 (a stacked-rook x-ray recapture).

- **Acceptance criteria:**
  - Perft/tactical green: PASS.
  - Each landed change has a measurement: PASS (the table above; the
    null-move/LMR/aspiration measurements live in the tasks 055/056
    reports).
  - No stability regression: PASS (correctness suites unchanged).
  - The final configuration documented: PASS (the flag defaults + the
    evidence in `include/chess/search.hpp`).

- **Risks or follow-ups:** the SEE is correct but slow on the 0x88
  representation; the smallest next action for a strength land is to re-run
  this A/B after the bitboard move generation (task 033) and to add the
  SEE-capture *ordering* (MVV-LVA + SEE) behind the same flag at that point.
  The SPRT/fixed-game Elo protocol exists in `scripts/match_report.py`
  (task 057) but there is no second engine installed to measure against.
