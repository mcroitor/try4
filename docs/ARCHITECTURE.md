# try4 Chess Engine — Architecture Assessment and Target Design

**Status:** Proposed  
**Date:** 2026-10-23  
**Scope:** Current-state analysis, target architecture, design decisions, and migration strategy.

---

## 1. Purpose and assumptions

`try4` is a small C++ chess project that currently implements a board, piece representation, and a naive make/undo mechanism. It does not yet generate moves, enforce the rules of chess, parse FEN, search, evaluate, or speak a protocol.

This document assumes the target is:

- a **correct, playable chess engine**;
- exposed through **UCI** (the standard engine/GUI boundary);
- reusable as a **core library** for a future CLI or GUI;
- built and tested on **Windows/MinGW** and **Linux**;
- written in **C++17** for broad toolchain compatibility.

If the intended target is instead a GUI application, a library, or a learning-only exercise, the core stages remain valid; only the protocol and release stages change.

### Quality priorities

In conflict, decisions are resolved in this order:

1. **Correctness and safety** — no illegal moves, no crashes, no corrupted state.
2. **Maintainability and testability** — clear boundaries, deterministic tests.
3. **Performance and strength** — measured and improved only after correctness.
4. **Cost and time-to-market** — small, shippable increments.

---

## 2. Executive summary

The project is a **skeleton**, not yet an engine. The pieces that exist are reasonable for a prototype, but several core contracts are missing or inverted:

- `Engine::GenerateMoves()` returns an empty list, so the engine cannot choose a move.
- `Board::Move()` moves any piece to any square without checking turn, legality, occupancy, or check.
- `Move` has no flags for captures, en passant, castling, or promotion.
- `UndoMove()` cannot restore side to move, castling rights, en passant state, or clocks.
- FEN import/export is stubbed.
- There are no tests, no version control, and no CI.

The highest-risk work is **rules correctness** (move generation, make/unmake, special moves), not search strength. The recommended path is therefore:

**baseline and guardrails → domain model and FEN → legal move generation + perft → complete game state → search → evaluation → UCI → optional performance work → release.**

The central architectural change is to replace the current `Board`/`Engine` split with a single **`Position`** value that owns all game state, and a separate **`Searcher`** that operates on a copy of a position. The core must be I/O-free; UCI belongs at the edge.

---

## 3. Current state

### 3.1 Inventory

| File | Approx. size | Responsibility | Maturity |
|---|---:|---|---|
| `defines.h` | 117 lines | `color_t`, `piece_t`, `Field`, 64 global square constants | Prototype |
| `piece.h` / `piece.cpp` | 54 / 38 lines | `Piece`, `Move`, global piece constants | Prototype |
| `board.h` / `board.cpp` | 29 / 81 lines | 8×8 board, setup, naive move, ASCII rendering | Prototype |
| `engine.h` / `engine.cpp` | 25 / 23 lines | Board owner, empty move generation, partial make/undo | Skeleton |
| `main.cpp` | 20 lines | Demo: print board, e2–e4, undo, print | Demo only |
| `Makefile`, `nbproject/` | generated | NetBeans/Make build for Debug and Release | Works |
| `build/`, `dist/` | artifacts | Object files and `try4.exe` | Build output |

### 3.2 Toolchain and build status

- **Compiler:** `g++ 16.2.0` (MSYS2/MinGW).
- **Make:** GNU Make 4.4.1.
- **Build system:** NetBeans-generated makefiles (`nbproject/Makefile-Debug.mk`, `Makefile-Release.mk`).
- **C++ standard:** not specified. The build relies on the compiler default, which makes builds non-reproducible across toolchains.
- **Warning flags:** none. A manual build with `-Wall -Wextra -Wpedantic` succeeds but emits warnings.
- **Version control:** the directory is **not a git repository**.
- **Tests/CI/docs:** none.
- **Verified behavior:** the project compiles and runs. It prints the initial position, applies `e2–e4`, and undoes it correctly for a quiet, non-special move.

### 3.3 What works today

- Board initialization to the standard starting position.
- Piece color/type storage and ASCII rendering (uppercase white, lowercase black, space for empty).
- Direct get/set of pieces.
- Copyable value types (`Piece`, `Board`), which is a good foundation.
- A simple `Move` record with `from`, `to`, `piece`, and `taked` fields.
- Naive make/undo for quiet moves.

### 3.4 Maturity scorecard

| Area | Status | Notes |
|---|---|---|
| Board representation | Partial | Fixed 8×8 array; no validation, no 0x88 sentinels |
| Piece model | Partial | Color/type present; helpers and compact encoding missing |
| Game state | Missing | Side-to-move exists as a dead flag; no rights, en passant, clocks |
| FEN | Missing | Both import and export are stubs |
| Move generation | Missing | Returns an empty list |
| Legality / check | Missing | No attack detection, pins, check, mate, stalemate |
| Special moves | Missing | No castling, en passant, promotion; no move flags |
| Draw rules | Missing | No 50-move, repetition, insufficient material |
| Search | Missing | No negamax, alpha-beta, quiescence, TT, time management |
| Evaluation | Missing | No material, PST, or positional terms |
| Protocol | Missing | No UCI or CLI |
| Testing | Missing | No unit, perft, property, or integration tests |
| Build/CI | Partial | Local NetBeans build only; no standard flag or automation |
| Documentation | Missing | No README, architecture, or usage docs |

### 3.5 Code-level findings

Severity: **P0** blocks play; **P1** correctness risk; **P2** maintainability; **P3** polish.

| ID | Sev. | Location | Finding | Impact |
|---|---|---|---|---|
| F-01 | P0 | `engine.cpp:12-14` | `GenerateMoves()` returns an empty list. | The engine has no move source and cannot play. |
| F-02 | P0 | `board.cpp:45-52` | `Board::Move()` does not validate turn, legality, destination occupancy, or check. It can capture a friendly piece and move while in check. | Produces illegal positions; corrupts all downstream logic. |
| F-03 | P0 | `board.cpp:54-60` | `SetBoard()` and `GetFen()` are empty stubs. | No reproducible positions, no test fixtures, no UCI `position fen`. |
| F-04 | P0 | `engine.cpp:16-23` | `MakeMove` ignores the move result; `UndoMove` restores only two squares. Side to move, rights, en passant, clocks, and hashes are not restored. | Undo is not symmetric; search cannot rely on it. |
| F-05 | P1 | `piece.h:27-32` | `Move` lacks flags and promotion data. `taked` is supplied by the caller. | Callers must know capture state; special moves are unrepresentable. |
| F-06 | P1 | `defines.h:32-42` | `Field` converts characters with unchecked arithmetic and has no validation. | Invalid input can wrap `byte` and index out of bounds. |
| F-07 | P1 | `board.h:16` | `is_white_move` is stored but never read or toggled. | There is no authoritative side-to-move state. |
| F-08 | P1 | `board.cpp:7-43` | `Init()` does not clear the board first. | Re-initialization can leave stale pieces. |
| F-09 | P2 | `piece.h:20-21`, `piece.cpp:12,16` | `const` on by-value return types is ignored and inhibits moves. | Misleading API; compiler warnings. |
| F-10 | P2 | `piece.cpp:4,8` | Constructor initializer order differs from member declaration order. | `-Wreorder` warnings; fragile. |
| F-11 | P2 | `piece.h:34-51` | Twelve global `Piece` constants in a header have internal linkage per translation unit. | Duplication and inconsistent identity assumptions; better as `inline constexpr` or factory functions. |
| F-12 | P2 | `defines.h:44-114` | 64 global square constants in a header. | Header bloat and per-TU copies; leaks a representation detail. |
| F-13 | P2 | `piece.h` | Uses `std::string` without including `<string>`; it compiles only through `defines.h`. | Fragile include hygiene. |
| F-14 | P2 | `piece.cpp:22-29` | `switch(_type)` does not handle `NONE`. | `-Wswitch` warning; silent default behavior. |
| F-15 | P2 | `main.cpp:10` | Unused `argc`/`argv`. | Warning noise; no CLI entry point. |
| F-16 | P2 | Project root | No git, tests, CI, README, or standard build flag. | No safety net for refactoring; non-reproducible builds. |
| F-17 | P3 | `piece.cpp:31` | `::tolower(c)` with a possibly signed `char`. | Technically undefined for negative values; use `unsigned char`. |
| F-18 | P3 | `engine.h`, `board.h` | `Engine` is a board owner, not a search engine; `Field` means square. | Naming will become confusing once search is added. |

### 3.6 Verified build warnings

A scratch C++17 build produced warnings for:

- ignored `const` qualifiers on `Piece::Color()` and `Piece::Type()`;
- member initialization order in both `Piece` constructors;
- unused `fen` parameter in `Board::SetBoard`;
- enumeration value `NONE` not handled in `Piece::ToString`;
- unused `argc`/`argv` in `main`.

No errors. The demo output is correct for the quiet move case.

---

## 4. Target architecture

### 4.1 Architectural goals

| Goal | Target |
|---|---|
| Rule correctness | All legal moves generated; perft matches reference values; no illegal moves in self-play. |
| State integrity | `MakeMove`/`UndoMove` are exact inverses, including rights, clocks, en passant, and hash. |
| Testability | Core library has no I/O; rules and search are unit-testable without a board or GUI. |
| Maintainability | One owner of game state; explicit types; small interfaces; representation hidden. |
| Performance | Correct first; measured NPS baseline; optional bitboard migration behind a stable API. |
| Portability | C++17; GCC/Clang/MSVC-compatible; Windows and Linux CI. |
| Operability | UCI `info` output, stop responsiveness, deterministic tests at fixed depth. |

### 4.2 Design principles

1. **Correctness before speed.** No search or UCI work until perft passes.
2. **One source of truth.** A `Position` owns board, side, rights, en passant, and clocks.
3. **Explicit moves.** Every move carries its flags and promotion piece; never infer them from the board after the fact.
4. **Symmetric make/unmake.** History records everything needed to restore the prior state exactly.
5. **No I/O in the core.** UCI, CLI, logging, and file access are adapters.
6. **Hide the representation.** Callers use `Square`/`Move`; 0x88 or bitboards remain private.
7. **Every stage has an automated gate.** A stage is not done because it compiles; it is done because its tests pass.

### 4.3 Layered design

```
┌─────────────────────────────────────────────────────────┐
│ Applications                                            │
│   uci_main            cli_main            (future GUI)  │
└───────────────┬───────────────────────┬─────────────────┘
                │                       │
┌───────────────▼───────────────────────▼─────────────────┐
│ Adapters                                                │
│   UciSession: parse/format, async search, options       │
└───────────────┬─────────────────────────────────────────┘
                │
┌───────────────▼─────────────────────────────────────────┐
│ Search and Evaluation                                   │
│   Searcher, TimeManager, TranspositionTable, Evaluator  │
└───────────────┬─────────────────────────────────────────┘
                │
┌───────────────▼─────────────────────────────────────────┐
│ Rules and Position                                      │
│   Position, MoveGenerator, FEN, Zobrist, draw rules,    │
│   legal-move filtering, make/unmake history             │
└───────────────┬─────────────────────────────────────────┘
                │
┌───────────────▼─────────────────────────────────────────┐
│ Core types                                              │
│   Square, Piece, Move, Color, PieceType, CastlingRights │
└─────────────────────────────────────────────────────────┘
```

Dependencies point downward only. The core never includes UCI or iostream headers.

### 4.4 Components and responsibilities

| Component | Responsibility | Must not do |
|---|---|---|
| `core/types` | `Color`, `PieceType`, `Piece`, `Square`, `Move`, `MoveFlag`, `CastlingRights`, parsing/formatting helpers. | Know about boards, search, or I/O. |
| `core/position` | Board occupancy, side to move, rights, en passant, halfmove/fullmove clocks; FEN; legal move list; check/result; make/unmake history. | Search, evaluate, print to `std::cout`. |
| `core/movegen` | Pseudo-legal generation per piece; attack queries; legality filtering; perft. | Own game state outside a `Position`. |
| `core/fen` | Strict FEN parse/serialize with validation and useful errors. | Silently accept malformed input. |
| `core/zobrist` | Deterministic position hash; incremental updates; repetition keys. | Be used before its incremental update is tested. |
| `search/searcher` | Iterative deepening, negamax alpha-beta, quiescence, PV, stop handling. | Perform I/O or mutate a caller's position. |
| `search/eval` | Material, PST, pawn structure, mobility, king safety, tapered score. | Decide legality. |
| `search/tt` | Transposition table keyed by Zobrist hash; replacement policy. | Store pointers into a mutable position. |
| `uci` | Parse UCI commands, format `info`/`bestmove`, manage worker thread, options. | Contain chess rules. |
| `apps` | Process entry points and wiring. | Duplicate core logic. |
| `tests` | Unit, perft, property, UCI integration, regression suites. | Depend on a GUI. |
| `bench` | Perft and search benchmarks. | Be part of the release binary. |

### 4.5 Key interfaces

The exact syntax can evolve, but these contracts should remain stable.

```cpp
namespace chess {

enum class Color : std::uint8_t { White, Black };
enum class PieceType : std::uint8_t {
    None, Pawn, Knight, Bishop, Rook, Queen, King
};
enum class MoveFlag : std::uint8_t {
    Quiet, Capture, DoublePush, EnPassant,
    CastleKing, CastleQueen, Promotion
};

struct Square {
    std::uint8_t index; // 0..63, a1 = 0
    // Constructors, IsValid(), File(), Rank(), Parse(), ToString()
};

struct Move {
    Square from;
    Square to;
    MoveFlag flag;
    PieceType promotion; // None unless flag == Promotion
};

enum class GameResult {
    Ongoing, Checkmate, Stalemate, DrawFiftyMove,
    DrawRepetition, DrawInsufficientMaterial
};

class Position {
public:
    static std::optional<Position> FromFen(
        std::string_view fen,
        std::string* error = nullptr);

    std::string ToFen() const;
    Color SideToMove() const;
    bool InCheck(Color side) const;
    bool IsLegal(Move move) const;

    // Applies a legal move and pushes history. Returns false if illegal.
    bool MakeMove(Move move);
    void UndoMove();

    std::vector<Move> LegalMoves() const;
    GameResult Result() const;
};

struct SearchLimits {
    std::optional<int> depth;
    std::optional<std::int64_t> moveTimeMs;
    std::optional<std::int64_t> whiteTimeMs;
    std::optional<std::int64_t> blackTimeMs;
    std::optional<std::int64_t> whiteIncrementMs;
    std::optional<std::int64_t> blackIncrementMs;
    std::optional<int> movesToGo;
};

struct SearchInfo {
    int depth;
    int scoreCp;          // or mate score convention
    std::uint64_t nodes;
    std::vector<Move> pv;
    std::optional<Move> bestMove;
};

class Searcher {
public:
    explicit Searcher(std::size_t transpositionTableMegabytes = 16);
    void ClearTranspositionTable();
    void ResizeTranspositionTable(std::size_t megabytes);

    SearchInfo Search(const Position& position,
                      const SearchLimits& limits,
                      std::atomic_bool& stop);
};

struct TimeBudget {
    std::optional<std::int64_t> softMs;
    std::optional<std::int64_t> hardMs;
};

TimeBudget ComputeTimeBudget(const SearchLimits& limits, Color side);

class UciSession {
public:
    UciSession(std::istream& in, std::ostream& out);
    int Run();
};

} // namespace chess
```

**Important contract change:** `MakeMove` applies only legal moves and records everything needed for `UndoMove`. The search never needs to construct a reverse move or supply the captured piece. This removes the current caller-owned `taked` hazard.

### 4.6 Search state, table, and clocks

- Quiescence uses stand-pat outside check, extends captures, en passant, and promotions, and searches evasions when in check. Its recursion is capped at eight plies beyond the nominal depth; reaching the cap falls back to the static baseline.
- Move ordering is stable and deterministic: TT move, MVV-LVA captures, promotions, per-ply killers, then history. Killers and history update only on quiet beta cutoffs.
- The transposition table is direct-mapped and power-of-two sized. Each bucket stores the full Zobrist key, remaining depth, bound type, mate-adjusted score, and best move. A score is reused only when the stored depth is sufficient and the bound permits it; root nodes never take a TT cutoff so the reported PV always begins with a real move.
- A `Searcher` owns one table. Tables are not shared between concurrent searches; that restriction is revisited with the threading task.
- `movetime` takes precedence over clock fields. Clock budgeting derives a soft target (stop before the next iteration) and a hard ceiling (abort the current iteration), reserving a small safety margin. The hard ceiling is the value tested against the 50 ms tolerance.
- Search polls the atomic stop flag and hard deadline every 32 nodes. Completed iterations are retained; a stopped partial iteration is discarded, and the legal fallback remains available.

### 4.7 State ownership and threading

- `Position` is a **copyable value** with an internal history stack.
- `Searcher` owns long-lived state such as the transposition table and killer/history tables.
- `UciSession` owns the current `Position` and the `Searcher`.
- A search runs on a **copy** of the position on a worker thread. The UCI thread never reads or mutates that copy while the search is active.
- `std::atomic_bool stop` is the only cross-thread signal. Search polls it every N nodes.
- Before starting a new search, the UCI thread joins the previous worker.

This avoids data races and makes `stop`/`ponderhit` tractable.

### 4.8 Board representation decision

**Phase 1: 0x88 mailbox (128 entries with off-board sentinels).**

Why:

- Off-board detection is a cheap index test.
- Move generation is straightforward to write and review.
- It is fast enough for a learning/correctness-first engine.
- It avoids the complexity of magic bitboards before the rules are proven.

**Phase 2 (optional): bitboards behind the same `Position`/`MoveGenerator` API.**

Why:

- Only after perft is green and profiling shows move generation/search is the bottleneck.
- The representation is private, so callers and tests do not change.
- A full perft regression suite makes the migration safe.

Do **not** start with magic bitboards. The risk of subtle rule bugs is higher than the performance benefit at this stage.

### 4.9 Architecture decision records

| ADR | Decision | Rationale | Consequences |
|---|---|---|---|
| ADR-001 | Build the core as a static library; keep `uci_main` and `cli_main` as thin executables. | Testability, reuse, clean dependency direction. | One extra CMake target; trivial cost. |
| ADR-002 | `Position` owns all game state; retire the `Board`/`Engine` ownership split. | Eliminates dead flags and inconsistent state. | Requires a staged migration; old API temporarily wrapped. |
| ADR-003 | Use 0x88 first; defer bitboards. | Correctness first; simpler code; perft gates. | Optional later rewrite behind a stable API. |
| ADR-004 | UCI is the external contract; the core has no I/O. | Standard interoperability; testable core. | Adapter and thread management work in Stage 6. |
| ADR-005 | `MakeMove` accepts only legal moves and records history; `UndoMove` takes no arguments. | Symmetry and safety; fits search. | Callers must validate or use `LegalMoves()`. |
| ADR-006 | Move flags and promotion are explicit in `Move`. | Special moves cannot be inferred reliably after the fact. | Slightly larger move type; clearer logic. |
| ADR-007 | Zobrist hashing is introduced with draw rules and reused for the TT. | One hash implementation, incremental and testable. | Must test incremental updates against full recomputation. |
| ADR-008 | CMake is the canonical build; NetBeans may import it. Generated makefiles are not edited by hand. | CI portability and reproducible flags. | Keep the NetBeans project only if it remains useful; otherwise retire it. |
| ADR-009 | C++17 baseline; warnings-as-errors in CI. | Broad toolchain support; catches regressions early. | Avoid C++20-only features such as `std::stop_token`; use an atomic flag. |
| ADR-010 | Catch2 v3 + CTest (vendor or FetchContent fallback). | Mature assertions and test discovery; standard runner. | Network needed for FetchContent unless vendored. |
| ADR-011 | Fixed-depth tests are deterministic; time-based tests are separate. | Search timing must not make CI flaky. | Two test categories and appropriate tolerances. |
| ADR-012 | No global mutable state; each engine instance owns its search state. | Test isolation and thread safety. | Slightly more wiring in `main`. |

### 4.10 Non-functional targets

| Attribute | Target | Verification |
|---|---|---|
| Rule correctness | Exact perft for six standard positions, depths 1–4 in CI; 5–6 nightly. | Perft suite. |
| State integrity | 100,000 random legal games with make/unmake and FEN/hash invariants intact. | Property/fuzz test. |
| Stability | 1,000 self-play games without a crash, hang, or illegal move. | Self-play harness. |
| Stop latency | `stop` acknowledged and `bestmove` emitted within 100 ms. | UCI integration test. |
| Protocol | Passes a UCI conformance checklist; works in CuteChess/Arena. | Scripted UCI tests + manual GUI game. |
| Portability | Builds on Windows MinGW and Ubuntu GCC/Clang. | CI matrix. |
| Sanitizers | ASan/UBSan clean on Linux. | CI job. |
| Performance | Record baseline perft NPS in Stage 2; set a search NPS target after Stage 4. | Benchmarks. |
| Strength | Measure against a material-only baseline; set an Elo target after the baseline exists. | SPRT self-play. |

Avoid committing to a specific Elo number before a baseline exists. Strength claims without a measurement protocol are not actionable.

---

## 5. Testing strategy

### 5.1 Test pyramid

| Layer | What it covers | Examples | When |
|---|---|---|---|
| Unit | Core types, FEN, move encoding, clocks, rights | Square round-trip, FEN invalid inputs, promotion encoding | Stages 0–3 |
| Perft | Move generation and make/unmake correctness | Six standard positions, depths 1–6 | Stage 2 onward |
| Property/fuzz | Invariants under random play | Board/hash/FEN restored after undo; no illegal moves | Stage 3 onward |
| Tactical | Search correctness | Mate-in-1/2/3, winning captures, avoid stalemate | Stage 4 onward |
| UCI integration | Protocol and threading | Scripted `uci`/`isready`/`position`/`go`/`stop` sessions | Stage 6 onward |
| Self-play/SPRT | Strength regression | New vs. baseline at fixed time control | Stage 5 onward |
| Performance | NPS and latency | Perft NPS, search NPS, stop latency | Stages 2, 4, 7 |

### 5.2 Perft gates

Perft is the primary correctness gate. A single mismatch means the rules are wrong; search work must stop until it is fixed.

| Position | FEN | d1 | d2 | d3 | d4 | d5 |
|---|---|---:|---:|---:|---:|---:|
| Start | `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1` | 20 | 400 | 8,902 | 197,281 | 4,865,609 |
| Kiwipete | `r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1` | 48 | 2,039 | 97,862 | 4,085,603 | 193,690,690 |
| Position 3 | `8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1` | 14 | 191 | 2,812 | 43,238 | 674,624 |
| Position 4 | `r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1` | 6 | 264 | 9,467 | 422,333 | 15,833,292 |
| Position 5 | `rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8` | 44 | 1,486 | 62,379 | 2,103,487 | 89,941,194 |
| Position 6 | `r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10` | 46 | 2,079 | 89,890 | 3,894,594 | 164,075,551 |

CI runs depths 1–4 for all six; nightly/release runs depths 5–6. This keeps pull-request feedback fast while preserving deep coverage.

### 5.3 CI pipeline

1. Configure with CMake (Debug + Release).
2. Build with warnings-as-errors.
3. Run unit and FEN tests.
4. Run shallow perft.
5. Run scripted UCI smoke tests (once Stage 6 exists).
6. On Linux: ASan/UBSan job.
7. Nightly: deep perft, tactical suite, long self-play.

MinGW does not provide first-class ASan support. Use Ubuntu CI for sanitizer coverage and keep Windows for build/run verification.

### 5.4 Test data and reproducibility

- Store reference FENs and expected perft counts in source-controlled data files.
- Do not depend on wall-clock time in correctness tests.
- Seed randomized tests and print the seed on failure.
- Record benchmark hardware and compiler flags with results.
- Keep a `bench` target that is excluded from release binaries.

---

## 6. Migration strategy

Do not rewrite everything at once. The current behavior is small enough to characterize and replace incrementally.

### Step 1 — Freeze current behavior

Add tests around:

- default `Piece` and `Piece::ToString()`;
- initial board ASCII output;
- `Board::SetPiece`/`GetPiece`;
- quiet move and undo on the demo position.

These are characterization tests, not endorsements of the current API.

### Step 2 — Introduce the new core alongside the old code

- Add a `chess` namespace and the new types/`Position` in new files.
- Keep the old files compiling and the demo working.
- Add a CMake target for the new core library and its tests.
- Port FEN and board rendering first; then move generation.

### Step 3 — Switch and delete

- Point `main`/`uci_main` at the new core.
- Run the full test suite and perft.
- Delete the old `Board`/`Engine` implementation only after parity is proven.
- Rename `Field` to `Square` and `Engine` to `Searcher`/`Game` as part of the port.

### Step 4 — Retire generated build files

- Make `CMakeLists.txt` the canonical build.
- NetBeans can import the CMake project; do not hand-edit `nbproject/Makefile-*.mk`.
- Add `.gitignore` entries for `build/`, `dist/`, `nbproject/private/`, object files, and executables.

### Suggested target tree

```
CMakeLists.txt
README.md
docs/
  ARCHITECTURE.md
  ROADMAP.md
include/chess/
  types.hpp
  position.hpp
  movegen.hpp
  fen.hpp
  zobrist.hpp
  search.hpp
  eval.hpp
src/core/
  position.cpp
  movegen.cpp
  fen.cpp
  zobrist.cpp
src/search/
  search.cpp
  eval.cpp
  tt.cpp
src/uci/
  uci_session.cpp
apps/
  uci_main.cpp
  cli_main.cpp
tests/
  unit/
  perft/
  property/
  uci/
bench/
  perft_bench.cpp
  search_bench.cpp
```

For a project this size, headers may live beside sources under `src/` instead of a separate `include/` tree. Choose one convention and keep it consistent.

---

## 7. Risks and mitigations

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Special-move bugs (castling, en passant, promotion) | High | High | Perft suite, targeted unit tests, make/unmake invariants. |
| Illegal moves reach self-play/UCI | Medium | High | `MakeMove` validates; `LegalMoves()` is the only move source; assert in debug builds. |
| Undo state corruption | Medium | High | History object records rights, en passant, clocks, captured piece, hash; property test compares full state. |
| Search bugs (mate scores, TT collisions) | Medium | High | Tactical suite, deterministic depth tests, hash verification, conservative TT. |
| Time-management overruns / hangs | Medium | High | Atomic stop, poll every N nodes, stress tests, GUI self-play. |
| Representation rewrite destabilizes rules | Medium | Medium | Hide representation; perft regression before/after bitboards. |
| Build drift between NetBeans and CI | High | Medium | CMake canonical; CI builds from a clean checkout; do not edit generated makefiles. |
| Windows-only testing hides sanitizer issues | High | Medium | Ubuntu CI with ASan/UBSan. |
| Scope creep (GUI, NNUE, cloud, opening book) | High | Medium | Explicit non-goals for v1; stage gates. |
| No version control / lost work | Current | High | Initialize git in Stage 0 before any refactor. |
| Strength work without measurement | Medium | Medium | Baseline first; SPRT; record time control and hardware. |
| Premature optimization | Medium | Medium | Profile; only optimize after perft and tactical correctness. |

---

## 8. Non-goals for the first playable release

- Graphical user interface.
- Neural-network evaluation (NNUE).
- Cloud or distributed analysis.
- Opening book and endgame tablebases.
- Multithreaded search (Lazy SMP).
- Ponder mode.
- PGN database features beyond basic import/export if needed for testing.

These are reasonable post-1.0 extensions, but each adds surface area that would delay a correct, playable engine.

---

## 9. Open decisions

| Question | Default assumption | Impact if different |
|---|---|---|
| Is the target UCI, GUI, or a library? | UCI engine + reusable core. | Stages 0–5 unchanged; Stage 6 adapter changes. |
| Keep NetBeans as the primary IDE/build? | CMake canonical; NetBeans optional. | If NetBeans must remain primary, add a test target and replicate flags carefully. |
| C++17 or C++20? | C++17. | C++20 simplifies stop tokens/`span` but narrows toolchain support. |
| Target strength/time budget? | Set a baseline in Stage 4, then choose. | Determines whether Stage 7 (bitboards, advanced search) is in scope. |
| Is PGN/SAN required? | SAN for debugging/tests; PGN optional. | Adds parsing/formatting work in Stage 3. |
| Is a GUI required? | No; use CuteChess/Arena for manual testing. | Adds a separate UI architecture and delays v1. |

---

## 10. Recommended immediate actions

1. Initialize git and add a `.gitignore`.
2. Add a CMake build with explicit C++17 and warning flags.
3. Add Catch2/CTest and characterization tests for the current behavior.
4. Fix the low-risk warning and hygiene issues (F-09 to F-15).
5. Start Stage 1 with `Square`, `Move` flags, and `Position` + FEN.

The detailed sequence, estimates, and exit criteria are in [`ROADMAP.md`](ROADMAP.md).
