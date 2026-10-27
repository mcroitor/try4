# Task 050 - Add allocation-free move generation and unchecked make/unmake to Position

| Field | Value |
|---|---|
| **ID** | 050 |
| **Stage** | Search performance - core primitives |
| **Depends on** | 049 |
| **Estimate** | 1 day |
| **Priority** | P1 |
| **Status** | Done |

## Objective

Provide the `Position` primitives the search needs to stop copying positions,
regenerating legal moves and allocating per node, without changing any existing
public behavior.

## Background

The current search cannot avoid the expensive API because the cheap variants do
not exist:

- `Position::LegalMoves() const` copies the whole `Position` including
  `history_`, clears it, then generates (`src/core/position.cpp:365-369`).
- `Position::Result()` calls `LegalMoves()`, `IsThreefoldRepetition()` and
  `HasInsufficientMaterial()` on every call (`position.cpp:371-387`).
- `Position::MakeMove()` calls `FindLegalMove()`, which calls `LegalMoves()`
  again (`position.cpp:404-411`).
- `IsThreefoldRepetition()` copies the entire `Position` for every history hash
  that matches the current hash (`position.cpp:221-244`).
- `MoveList` already exists with a fixed capacity of 256
  (`include/chess/movegen.hpp:17-42`) but neither `Position` nor the search uses
  it.

`src/core/search.cpp` calls the expensive variants at every node: `Result()`
(298), `LegalMoves()` (329), `MakeMove()` (344), and the same pattern in
`Quiescence` (202, 221, 257). Task 051 removes those calls; this task supplies
the replacements and proves they are equivalent.

## In scope

- `std::size_t Position::GenerateLegalMoves(MoveList* out)` (const): no
  `std::vector`, no `Position` copy, same ordering as `LegalMoves()`.
- `bool Position::MakeMoveAssumeLegal(const Move& move)`: documented
  precondition (the move came from `GenerateLegalMoves`/`LegalMoves` for this
  exact position), identical state transition to `MakeMove`, single history
  push.
- `bool Position::IsRepetition() const`: hash-history scan with no `Position`
  copies; three-occurrence semantics equal to the current
  `IsThreefoldRepetition`.
- `void Position::ReserveHistory(std::size_t capacity)`.
- Reimplement `LegalMoves()`, `LegalMoves() const`, `MakeMove()`, `Result()`
  and `IsThreefoldRepetition()` on top of the new primitives, preserving their
  observable behavior.
- Tests and documentation comments.

## Out of scope

- Search changes (task 051).
- Transposition table, evaluation, pruning or time management.
- Changing legal-move ordering, legality rules or the 0x88 representation.
- Bitboards.

## Requirements

1. `GenerateLegalMoves` must produce the same **sequence** as `LegalMoves()`,
   not merely the same set. Ordering is part of the contract because fixed-depth
   determinism and move ordering depend on it.
2. `MakeMoveAssumeLegal` must not repeat legality work. It must remain guarded
   by a debug assertion/precondition check, and a test must prove the guard
   fires for an illegal move in an assertion-enabled build.
3. `IsRepetition` must not construct `Position` copies and must agree with
   `Result() == GameResult::DrawRepetition` wherever fifty-move and
   insufficient-material draws do not take precedence. Document the precedence
   rule in the header.
4. Repetition detection may rely on the 64-bit Zobrist hash (which already
   covers side, castling and en-passant file). Record that collision policy
   explicitly instead of silently weakening the current board comparison.
5. `ReserveHistory` must not change `HistoryDepth()` semantics or FEN output.
6. `GenerateLegalMoves` and `MakeMoveAssumeLegal` must not allocate beyond
   `history_` growth. No `std::vector` in either path.
7. Warning-clean under `/W4 /WX` and `-Wall -Wextra -Wpedantic -Werror`;
   ASan/UBSan clean.

## Deliverables

- New `Position` API plus implementations.
- Equivalence tests and a repetition fixture.
- Header documentation of preconditions, precedence and collision policy.

## Acceptance matrix

| Case | Expected result |
|---|---|
| Perft depths 1-4, six reference positions | Identical counts |
| 10,000 seeded random legal positions | `GenerateLegalMoves` size and sequence equal `LegalMoves()` |
| 1,000 random positions, both colors | `MakeMoveAssumeLegal` yields the same board, FEN, hash, castling, en passant and clocks as `MakeMove` |
| Illegal move, assertion-enabled build | Debug guard fires; release behavior documented |
| Random playouts with repetitions | `IsRepetition()` agrees with `Result() == DrawRepetition` excluding higher-precedence draws |
| Castling, en passant, promotion | Generated sequences unchanged |
| Existing suites | `unit`, `fen`, `perft`, `property`, `search` pass |

## Validation commands

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug -L "unit|fen|perft|property|search" \
  --output-on-failure

cmake -S . -B build/performance -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/performance
ctest --test-dir build/performance -C Release --output-on-failure
```

Expected: all suites pass before the search starts using the new API.

## Risks and failure modes

- **Ordering drift.** Reimplementing generation can reorder moves; the
  sequence-equality test is mandatory, and any intentional ordering change must
  wait for task 052.
- **Unchecked-make misuse.** A wrong move silently corrupts the position; keep
  the debug guard, document the precondition at the call site and add the
  illegal-move test.
- **Hash-only repetition.** Collisions are theoretically possible; document the
  accepted 64-bit risk or keep a cheap secondary comparison if one can be added
  without copies.
- **Hidden copies.** `LegalMoves() const` must be rechecked with a profiler;
  equivalence tests do not catch allocations.

## Completion notes

- Added `Position::GenerateLegalMoves(MoveList*) const`,
  `MakeMoveAssumeLegal`, `IsRepetition` and `ReserveHistory`.
  `LegalMoves()`, `Result()` and `FindLegalMove()` now use the fixed-capacity
  generation path; `LegalMoves() const` no longer copies the position.
- `GenerateLegalMoves` preserves the pseudo-legal order and filters king safety
  with temporary make/unmake. It is logically const and not reentrant; the
  header documents the restoration and `ReserveHistory` contract.
- Repetition detection is now a hash-history scan. The header records the
  accepted 64-bit Zobrist collision policy and the existing `Result()` draw
  precedence.
- `MakeMoveAssumeLegal` performs its precondition check only in debug builds
  and throws `std::logic_error` for an illegal or inexact move. Release builds
  have no legality check.
- Added `tests/position_primitives_tests.cpp`: 10,000 generated sequences,
  1,000 make-transition comparisons, castling/en-passant/promotion fixtures,
  repetition and fifty-move precedence, history reservation, and the debug
  guard. CMake registers it as `try4_position_primitives` with the `property`
  label.
- Debug and Release full suites pass **24/24**. Release start-position depth 6
  still reports **79,677 nodes** with the same PV; the observed time was 383 ms
  on this host. ASan/UBSan were not run locally because MinGW does not provide
  first-class ASan support; the repository's Linux CI job remains the sanitizer
  gate.
- Smallest next action: start task 051, using the new generation and unchecked
  make path in `Negamax` and `Quiescence` without changing node counts.

## Completion report

Return:

- **Status:** done or blocked
- **Files changed:** paths and one-line descriptions
- **Commands and results:** builds, suites, equivalence counts, sanitizer result
- **Acceptance matrix:** each row pass or fail
- **Risks or follow-ups:** ordering, collision policy, remaining copies and the
  smallest next action
