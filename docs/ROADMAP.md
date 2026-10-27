# try4 Chess Engine — Development Roadmap

**Status:** try4 0.1.0 tagged locally as `v0.1.0` on 2026-10-26; final local release gates passed. The original checklists below are planning criteria, not a claim that every optimization target was pursued. Current evidence is in [`RELEASE_VALIDATION.md`](RELEASE_VALIDATION.md); hosted CI and a separate physical clean machine remain unverified.
**Date:** 2026-10-23 (status updated 2026-10-26)  
**Companion document:** [`ARCHITECTURE.md`](ARCHITECTURE.md)

---

## 1. How to use this roadmap

The stages are ordered by dependency, not by novelty. Each stage produces a working, tested increment. Do not start a stage until the previous stage's exit criteria are met.

- **Estimate unit:** effort-days for one developer, including tests and documentation. They are planning ranges, not commitments.
- **Core release path:** Stages 0–6 (playable UCI engine).
- **Optional strength path:** Stage 7.
- **Release path:** Stage 8.
- **Rule:** a stage is done only when its exit criteria are verified in CI, not when the code compiles locally.

### Assumptions

- One developer working part-time or full-time.
- C++17, CMake, GCC/Clang, Windows + Linux.
- Correctness-first; performance work is deferred until perft and search are stable.
- UCI is the external interface; GUI is out of scope for v1.

### Timeline summary

| Stage | Name | Primary outcome | Effort (days) | Depends on |
|---|---|---|---:|---|
| 0 | Baseline and guardrails | Reproducible build, tests, CI, version control | 2–3 | — |
| 1 | Domain model and FEN | Valid `Square`/`Move`/`Position`, strict FEN | 3–5 | 0 |
| 2 | Legal move generation | Correct legal moves, make/unmake, perft | 8–12 | 1 |
| 3 | Complete game state | Draws, notation, invariants, CLI | 4–6 | 2 |
| 4 | Search foundation | Iterative deepening, alpha-beta, quiescence, time control | 6–8 | 3 |
| 5 | Evaluation | Material + positional eval, measurable strength | 6–8 | 4 |
| 6 | UCI and playable engine | GUI-playable engine, async stop, integration tests | 5–7 | 4–5 |
| 7 | Performance and strength (optional) | Bitboards, advanced search, TT improvements | 10–15 | 6 |
| 8 | Release engineering | Versioned binaries, docs, final regression | 2–3 | 6 (7 optional) |

**Core path (0–6):** roughly 34–49 effort-days. Full-time that is about 7–10 weeks; part-time (10–15 h/week) it is roughly 3–5 months.

**With optional performance and release (0–8):** roughly 46–67 effort-days.

---

## 2. Stage 0 — Baseline and guardrails

**Goal:** make the current prototype safe to change. No chess features yet.

### Tasks

- [ ] Initialize git; add `.gitignore` for `build/`, `dist/`, `nbproject/private/`, `*.o`, `*.exe`, editor folders.
- [ ] Add `README.md` with build, run, and test commands.
- [ ] Add `CMakeLists.txt`:
  - C++17, no compiler extensions;
  - Debug and Release configurations;
  - one core library target and one app target;
  - `-Wall -Wextra -Wpedantic` (and `-Werror` in CI);
  - tests optional via a CMake option.
- [ ] Add Catch2 v3 and CTest. Vendor it or use `FetchContent` with an offline fallback.
- [ ] Add characterization tests:
  - default `Piece` is empty and prints a space;
  - white/black piece rendering;
  - initial board ASCII output;
  - `SetPiece`/`GetPiece`;
  - quiet move and undo on the demo position.
- [ ] Fix low-risk warnings:
  - remove `const` from by-value return types;
  - reorder `Piece` initializer lists;
  - include `<string>` directly in `piece.h`;
  - handle `PieceType::None` in `ToString`;
  - remove or use `main` parameters;
  - mark `SetBoard`'s unused parameter or leave it for Stage 1.
- [ ] Add a minimal CI workflow: build + tests on Ubuntu and Windows.
- [ ] Record the current demo output as a golden/characterization fixture.

### Deliverables

- `CMakeLists.txt`, test target, `.gitignore`, `README.md`, CI workflow.
- First test suite covering existing behavior.
- Warning-clean build with an explicit standard.

### Exit criteria

- [ ] A fresh checkout builds with one command.
- [ ] `ctest` (or equivalent) passes.
- [ ] No warnings with the chosen flags.
- [ ] CI is green on Ubuntu and Windows.
- [ ] The demo output matches the recorded baseline.

**Effort:** 2–3 days.  
**Risk:** over-engineering the build. Keep the CMake file minimal; add packaging later.

---

## 3. Stage 1 — Domain model and FEN

**Goal:** establish trustworthy chess types and a complete, serializable game state.

### Tasks

- [ ] Create the `chess` namespace.
- [ ] Replace `Field` with `Square`:
  - index 0..63 (`a1 = 0`);
  - constructors from file/rank and from algebraic notation;
  - `IsValid()`, `File()`, `Rank()`, `ToString()`, equality;
  - parsing returns `std::optional<Square>` or a documented error.
- [ ] Replace `piece_t`/`color_t` with scoped enums and helper predicates:
  - `IsSlider`, `IsPawn`, `IsKing`, `Opposite(Color)`, etc.
- [ ] Make `Piece` compact and self-contained; remove header-level global piece objects or make them `inline constexpr`.
- [ ] Define `Move` with `from`, `to`, `MoveFlag`, and `promotion`. Add encoding/decoding tests.
- [ ] Implement `Position` with:
  - board occupancy;
  - side to move;
  - castling rights;
  - en passant target;
  - halfmove clock and fullmove number;
  - `Clear()` and standard setup.
- [ ] Implement strict FEN parsing and serialization:
  - validate piece placement, side, rights, en passant, clocks;
  - reject malformed or impossible states with a useful error;
  - round-trip property: `ToFen(FromFen(s)) == s` for canonical inputs.
- [ ] Preserve or wrap the existing demo until the new path is ready.
- [ ] Add tests for all types, FEN edge cases, and standard positions.

### Deliverables

- New core types and `Position`.
- FEN import/export with validation.
- Unit tests and a small corpus of reference FENs.

### Exit criteria

- [ ] FEN round-trips exactly for the six perft positions and additional edge cases.
- [ ] Invalid FENs are rejected with an error, not accepted silently.
- [ ] `Square` and `Move` parsing/formatting are covered by tests.
- [ ] The old demo still builds or has been replaced by an equivalent new one.
- [ ] No global mutable state; `Position` is copyable.

**Effort:** 3–5 days.  
**Risk:** FEN edge cases (en passant target legality, clocks, castling rights). Test them explicitly.

---

## 4. Stage 2 — Legal move generation

**Goal:** generate every legal move correctly and prove it with perft. This is the hardest and most important stage.

### Tasks

- [ ] Implement the 0x88 board representation (or a carefully bounded 8×8 equivalent) behind the `Position` API.
- [ ] Generate pseudo-legal moves:
  - pawn single push, double push, captures, promotions;
  - knight and king offsets;
  - sliding pieces with blocking;
  - en passant;
  - castling (both sides), including rights and empty/attacked squares.
- [ ] Implement attack detection:
  - `IsSquareAttacked(square, byColor)`;
  - `InCheck(side)`.
- [ ] Filter pseudo-legal moves for legality:
  - king cannot remain in check;
  - pinned pieces;
  - en passant discovered-check edge case;
  - castling through/into check.
- [ ] Implement `MakeMove`/`UndoMove` with a history record:
  - captured piece;
  - castling rights before the move;
  - en passant target before the move;
  - halfmove/fullmove clocks;
  - Zobrist hash (or a placeholder until Stage 3);
  - side to move.
- [ ] Add `IsCheckmate()`/`IsStalemate()` and a provisional `Result()`.
- [ ] Add a `perft` command/bench target.
- [ ] Add the six standard perft positions and expected counts as test data.
- [ ] Add make/unmake invariant tests that compare full state before and after undo.
- [ ] Record perft NPS baseline (hardware, compiler, flags).

### Deliverables

- Legal move generator and attack queries.
- Symmetric make/unmake with history.
- Perft harness and regression suite.
- Performance baseline.

### Exit criteria

- [ ] Perft matches reference counts for all six positions at depths 1–4 in CI.
- [ ] Nightly/release run matches depths 5–6.
- [ ] 10,000 random legal games produce no illegal moves, crashes, or invariant failures.
- [ ] `UndoMove` restores exactly the pre-move FEN and state.
- [ ] Castling, en passant, and promotion each have dedicated tests.
- [ ] `Move` flags are consistent with the board transition.

**Effort:** 8–12 days.  
**Risk:** special moves and pinned-piece edge cases. Do not proceed to search with a single perft mismatch.  
**Gate A:** perft green. Search work is blocked until this gate passes.

---

## 5. Stage 3 — Complete game state and hardening

**Goal:** finish the rules of chess and make the core robust under randomized play.

### Tasks

- [ ] Add Zobrist hashing with incremental updates; test against full recomputation.
- [ ] Implement draw detection:
  - fifty-move rule;
  - threefold repetition (position keys, including side/rights/en passant);
  - insufficient material (K vs K, K+B vs K, K+N vs K, K+B vs K+B same color).
- [ ] Add SAN and UCI move notation:
  - parse/format at minimum for tests and debugging;
  - disambiguation for SAN.
- [ ] Add optional PGN import/export if needed for test corpora.
- [ ] Add a CLI:
  - load FEN, print board/FEN;
  - apply a move, list legal moves, undo;
  - print check/checkmate/stalemate/draw result.
- [ ] Add property/fuzz tests:
  - random legal playouts;
  - make/unmake invariants;
  - FEN round-trip after every move;
  - hash equals full recomputation.
- [ ] Run ASan/UBSan in Linux CI.
- [ ] Fix all findings from sanitizers and fuzzing.

### Deliverables

- Draw rules, notation, and a usable CLI.
- Property test harness with reproducible seeds.
- Sanitizer-clean core.

### Exit criteria

- [ ] 100,000 random legal games complete without a crash or invariant failure.
- [ ] Draw rules have unit tests, including repetition across undo.
- [ ] SAN/UCI notation round-trips for a representative corpus.
- [ ] ASan/UBSan jobs are clean.
- [ ] CLI can inspect and play a game from a FEN.

**Effort:** 4–6 days.  
**Risk:** repetition and hash correctness. Test incremental hash updates explicitly.

---

## 6. Stage 4 — Search foundation

**Goal:** choose strong moves within a time budget using a correct, testable search.

### Tasks

- [ ] Define search limits and results:
  - depth, nodes, move time, clock time, increments;
  - `SearchInfo` with depth, score, nodes, PV.
- [ ] Implement negamax alpha-beta with fail-soft scores.
- [ ] Add iterative deepening and principal variation extraction.
- [ ] Add quiescence search:
  - captures and promotions;
  - stand-pat;
  - optional delta pruning.
- [ ] Define mate-score convention and handle it consistently.
- [ ] Integrate draw scores for repetition and fifty-move rule.
- [ ] Add move ordering:
  - hash move (if TT exists);
  - MVV-LVA for captures;
  - killers and history heuristics.
- [ ] Add a transposition table (can be minimal here; improve in Stage 7):
  - Zobrist key, depth, score, bound, best move;
  - verify no cross-position corruption.
- [ ] Add time management:
  - allocate time from `movetime` or clock + increment;
  - atomic stop flag;
  - poll every N nodes;
  - never exceed the hard limit.
- [ ] Add a tactical test suite:
  - mate-in-1, mate-in-2, mate-in-3;
  - winning captures and simple tactics;
  - positions where a stalemate must be avoided.
- [ ] Add deterministic fixed-depth tests and separate time-based tests.

### Deliverables

- Searcher with iterative deepening, alpha-beta, quiescence, ordering, TT, and time control.
- Tactical suite and search unit tests.
- Search NPS baseline.

### Exit criteria

- [ ] Solves 100% of the curated mate-in-1/2 suite at the specified depth.
- [ ] Solves the mate-in-3 suite at the specified depth (or documents exceptions).
- [ ] Fixed-depth node counts are deterministic.
- [ ] No time overrun greater than 50 ms in stress tests.
- [ ] `stop` returns a legal `bestmove` promptly.
- [ ] Draw scores and mate scores are unit-tested.

**Effort:** 6–8 days.  
**Risk:** mate-score and TT bugs. Prefer simple, verified logic over aggressive pruning.  
**Gate B:** tactical suite green. UCI integration may start, but a release is blocked.

---

## 7. Stage 5 — Evaluation

**Goal:** make the engine play meaningfully, with measurable improvement.

### Tasks

- [ ] Implement material evaluation with a clear centipawn scale.
- [ ] Add piece-square tables for the middlegame and endgame.
- [ ] Add tapered evaluation between phases.
- [ ] Add positional terms:
  - doubled, isolated, and passed pawns;
  - mobility;
  - bishop pair;
  - rook on open/semi-open file;
  - king pawn shield and attacker count;
  - tempo.
- [ ] Add evaluation tests:
  - symmetric positions evaluate to zero from the side-to-move perspective;
  - known material imbalances;
  - PST symmetry and color mirroring.
- [ ] Build a self-play harness with fixed nodes or fixed time.
- [ ] Establish a material-only baseline and run SPRT against it.
- [ ] Record results with time control, hardware, openings, and error margins.
- [ ] Tune weights only after the harness is trustworthy (optional Texel tuning later).

### Deliverables

- Evaluator with material and positional terms.
- Evaluation unit tests.
- Self-play/SPRT harness and baseline results.

### Exit criteria

- [ ] Evaluation is symmetric and free of side-to-move bugs.
- [ ] New evaluation beats the material-only baseline by a statistically meaningful margin (target: +100 Elo, subject to measurement).
- [ ] No regression in the tactical suite.
- [ ] Results are reproducible with recorded seeds and settings.

**Effort:** 6–8 days.  
**Risk:** overfitting to a tiny test set. Use a varied opening set and enough games.

---

## 8. Stage 6 — UCI and playable engine

**Goal:** ship an engine that a standard GUI can run and play complete games with.

### Tasks

- [ ] Implement the UCI adapter:
  - `uci`, `isready`, `setoption`, `ucinewgame`;
  - `position startpos [moves ...]`;
  - `position fen <fen> [moves ...]`;
  - `go depth|movetime|wtime|btime|winc|binc|movestogo|infinite`;
  - `stop`, `ponderhit` (optional), `quit`.
- [ ] Emit correct `id`, `option`, `info`, and `bestmove` lines.
- [ ] Add options:
  - `Hash`;
  - `Threads` (fixed at 1 for v1, or omit);
  - `Move Overhead`;
  - `UCI_Chess960` only if variant support is added (not in v1).
- [ ] Run search on a worker thread:
  - search a copy of the position;
  - atomic stop;
  - join before the next search;
  - no data races.
- [ ] Handle malformed/unknown commands without crashing.
- [ ] Add scripted UCI integration tests:
  - handshake;
  - startpos + moves;
  - FEN position;
  - depth and movetime limits;
  - stop during search;
  - quit during search.
- [ ] Test manually in CuteChess or Arena.
- [ ] Run at least 1,000 self-play games at a short time control.
- [ ] Verify no illegal moves, hangs, or time forfeits.

### Deliverables

- `try4-uci` executable.
- UCI integration test suite.
- Self-play stability report.
- Usage documentation.

### Exit criteria

- [ ] A GUI can start the engine, configure it, and play a complete game.
- [ ] Scripted UCI tests pass.
- [ ] 1,000 self-play games complete without crashes, hangs, or illegal moves.
- [ ] `stop` produces `bestmove` within 100 ms in stress tests.
- [ ] No time forfeits in the test time control.
- [ ] Engine responds correctly to `isready` while idle and after `ucinewgame`.

**Effort:** 5–7 days.  
**Risk:** threading and time management. Keep one search thread and one UCI thread; join deterministically.  
**Gate C:** 1,000-game stability. Release is blocked until this passes.

---

## 9. Stage 7 — Performance and strength (optional)

**Goal:** improve speed and playing strength without breaking correctness.

Start only if profiling shows a real bottleneck or a strength target requires it.

### Tasks

- [ ] Profile move generation, evaluation, and search.
- [ ] Replace 0x88 with bitboards behind the existing API:
  - occupancy bitboards;
  - knight/king attack tables;
  - magic or PEXT bitboards for sliders (with a fallback).
- [ ] Re-run the full perft suite after the representation change.
- [ ] Improve the transposition table (size, replacement, aging).
- [ ] Add advanced search techniques incrementally:
  - null-move pruning;
  - late move reductions;
  - aspiration windows;
  - futility pruning;
  - static exchange evaluation (SEE).
- [ ] Add opening book support (Polyglot) if desired.
- [ ] Add Syzygy tablebase probing if desired.
- [ ] Consider Lazy SMP multithreading only after single-thread stability.
- [ ] Maintain SPRT regression testing for every strength change.
- [ ] Record NPS and Elo deltas for each change.

### Deliverables

- Faster move generation/search.
- Measured strength gains.
- Updated benchmarks and regression results.

### Exit criteria

- [ ] Perft and tactical suites remain green.
- [ ] NPS target met (set after the Stage 2/4 baseline; a 5–10× movegen/search improvement is a reasonable bitboard goal).
- [ ] Each strength change has an SPRT result with confidence bounds.
- [ ] No stability regression in 1,000-game self-play.
- [ ] Documentation reflects any new options or limitations.

**Effort:** 10–15 days.  
**Risk:** a representation rewrite can introduce subtle rule bugs. Keep the old implementation until perft passes, and land the change behind a branch/flag.  
**Gate D:** profile-driven. If there is no measured bottleneck, skip this stage and release.

---

## 10. Stage 8 — Release engineering

**Goal:** produce a trustworthy, documented first release. The first release is
0.1.0, reflecting an honest pre-1.0 API/strength status.

### Tasks

- [x] Choose a version scheme and add `--version`/UCI `id` version.
- [x] Add `CHANGELOG.md`, MIT license, notices, and contribution notes.
- [x] Finalize README: build, UCI usage, options, limitations, tested platforms.
- [x] Add CMake install/package rules and Windows/Linux ZIPs with checksums.
- [x] Run the full regression matrix: unit, FEN, perft, property, tactical, UCI,
      books, configuration, forcing, 1,000-game protocol stability, ASan/UBSan.
- [x] Tag `v0.1.0` locally; archive artifacts and test reports.
- [x] Write a post-release roadmap and defer hosted CI/physical clean-machine
      verification to a follow-up task.

### Exit criteria

- [x] Release builds and tests pass from a fresh local clone.
- [x] All regression suites pass; deep perft depth 5–6 passes.
- [x] Release notes list known limitations and non-goals.
- [x] A new user can build and run the engine from the README alone.
- [x] No known P0/P1 defect is open for 0.1.0.
- [ ] Hosted CI, MSVC and a separate physical clean machine (deferred; no remote
      or environment available). The tag remains local.

**Effort:** 2–3 days.  
**Risk:** packaging surprises. Do a clean-machine build before tagging.

---

## 11. Milestones and decision gates

| Milestone | Stages | Definition | Gate |
|---|---|---|---|
| M1 — Foundation | 0–1 | Reproducible build, tested types, strict FEN | FEN round-trip and CI green |
| M2 — Rules complete | 2–3 | Legal move generation, draw rules, CLI | Perft exact; 100k random games clean |
| M3 — Thinking engine | 4–5 | Search + evaluation, tactical suite | Mate suite green; SPRT improvement |
| M4 — Playable | 6 | UCI, GUI games, async stop | 1,000 games clean; stop <100 ms |
| M5 — Strong and released | 7–8 (optional 7) | Performance work and v1.0 | Perft/tactical green; release checklist |

### Decision points

1. **After Stage 1:** confirm the external target (UCI vs. GUI vs. library). The core design does not change; the adapter does.
2. **After Stage 2:** review the perft NPS baseline. If acceptable, continue; if not, decide whether to optimize now or after Stage 6.
3. **After Stage 4:** set a realistic strength target based on the tactical suite and self-play baseline. Decide whether Stage 7 is in scope.
4. **After Stage 6:** if the engine is stable and strong enough for the intended use, release; otherwise enter Stage 7.

---

## 12. First sprint backlog (next 10 tasks)

These are the concrete next actions, in order:

1. Initialize git and add `.gitignore`.
2. Add `CMakeLists.txt` with C++17 and warning flags.
3. Add Catch2/CTest and a `tests` target.
4. Add characterization tests for the current board, piece, and quiet move/undo behavior.
5. Add `README.md` with build/run/test commands.
6. Fix the compiler warnings listed in `ARCHITECTURE.md` (F-09 to F-15).
7. Introduce `Square` with parse/format and validity tests.
8. Introduce `Move` with flags and promotion encoding tests.
9. Implement `Position` state (board, side, rights, en passant, clocks).
10. Implement FEN parsing/serialization and the round-trip test corpus.

Do not start move generation until tasks 1–10 are complete.

---

## 13. Definition of done (every stage)

A stage is complete only when all of the following are true:

- [ ] The code builds from a clean checkout with the canonical build.
- [ ] New behavior has automated tests.
- [ ] The full existing test suite passes.
- [ ] No new compiler warnings.
- [ ] CI is green.
- [ ] Relevant documentation is updated.
- [ ] Performance/correctness baselines are recorded where applicable.
- [ ] No P0/P1 defect is knowingly deferred without a written note.

---

## 14. Progress tracking template

Copy this table into the project README or issue tracker and update it as stages complete.

| Stage | Status | Start date | End date | Notes |
|---|---|---|---|---|
| 0 — Baseline | Done | 2026-10-23 | 2026-10-23 | CMake, Catch2/CTest, docs, CI workflow |
| 1 — Domain/FEN | Done | 2026-10-23 | 2026-10-23 | Types, Position, strict FEN |
| 2 — Movegen/perft | Done | 2026-10-23 | 2026-10-23 | 0x88 movegen; depth 1–6 references pass |
| 3 — Game state | Done | 2026-10-23 | 2026-10-23 | Draws, notation, CLI, hashing |
| 4 — Search | Done | 2026-10-23 | 2026-10-24 | Alpha-beta, TT, quiescence, clock |
| 5 — Evaluation | Done | 2026-10-24 | 2026-10-25 | Tapered evaluation + self-play baseline |
| 6 — UCI | Done | 2026-10-25 | 2026-10-26 | Async UCI, books, conformance fixes; 1,000-game report |
| 7 — Performance | Optional, not pursued | | | Profile-driven bitboards/advanced search deferred |
| 8 — Release | Done locally | 2026-10-25 | 2026-10-26 | v0.1.0 local tag; hosted CI/clean physical machine deferred |

### Metrics to record

- Perft NPS and exact counts per position.
- Search NPS at fixed depth.
- Tactical suite pass rate.
- Self-play Elo difference vs. baseline, with error margins.
- Stop latency and time-forfeit count.
- Crash-free games out of 1,000.
- Test count and coverage of core rules (qualitative is acceptable if coverage tooling is unavailable).
- Build time and release artifact size.

---

## 15. Recommended next action

Start with **Stage 0, tasks 1–5**. The current code is small enough that setting up version control, CMake, and tests will take less than a day and will pay for itself during the first refactor. Then proceed to Stage 1 and freeze the domain model before writing any move-generation code.
