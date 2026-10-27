# try4 Task Backlog

This backlog decomposes [`docs/ARCHITECTURE.md`](../docs/ARCHITECTURE.md) and
[`docs/ROADMAP.md`](../docs/ROADMAP.md) into executable tasks. Each task is a
self-contained contract with scope, requirements, acceptance criteria, and
validation commands.

## Numbering convention

Task files are named `task_<NNN>.md`, where `<NNN>` is a **3-digit
zero-padded** sequence number:

```text
task_001.md
task_002.md
...
task_038.md
```

Do not renumber tasks after work has started. Add new tasks at the end of the
sequence and record their dependencies in the new file and in the index below.

## Execution rules

1. Read the task file fully before changing code.
2. Verify every task listed under **Depends on** is complete.
3. Work only within the task's **In scope** section.
4. Run the **Validation** commands and check every acceptance criterion.
5. Update the task's **Status** field: `Ready`, `In progress`, `Blocked`, or `Done`.
6. If a task raises an unresolved product decision, stop and ask the user; do
   not guess.
7. Do not modify generated NetBeans makefiles by hand.
8. Return the completion report described at the end of the task file.

## Common definition of done

Every task must also satisfy the project-wide definition of done:

- The project builds from a clean checkout with the canonical CMake build.
- New behavior has automated tests.
- The full existing test suite passes.
- No new compiler warnings.
- CI is green.
- Relevant documentation is updated.
- Baselines are recorded where the task changes correctness or performance.
- No P0/P1 defect is knowingly deferred without a written note.

## Assumptions

- The target interface is UCI, with a reusable core library.
- C++17 is the baseline; CMake is the canonical build.
- `build/cmake` is the default build directory.
- Stage 7 tasks (033–035) are optional and should start only if profiling or a
  strength target justifies them.
- The license choice for task 036 requires explicit user approval.

## Effort

The task estimates total roughly **57 effort-days**:

- Stages 0–6 (tasks 001–032, core playable engine): ~47.5 effort-days.
- Stage 7 (tasks 033–035, optional): ~7 effort-days.
- Stage 8 (tasks 036–038, release): ~2.5 effort-days.

These are planning ranges for one developer, including tests and documentation.
They are not calendar commitments.

## Index

|   ID | Title                                                                      | Stage | Depends on    | Estimate |
| ---: | -------------------------------------------------------------------------- | ----: | ------------- | -------- |
|  001 | Initialize git and establish repository hygiene                            |     0 | —             | 0.5 day  |
|  002 | Add a canonical CMake build with C++17 and warning flags                   |     0 | 001           | 1 day    |
|  003 | Add Catch2/CTest and characterization tests, then wire CI                  |     0 | 002           | 1.5 days |
|  004 | Fix compiler warnings and header hygiene                                   |     0 | 002, 003      | 0.5 day  |
|  005 | Write the README and developer documentation                               |     0 | 002, 003      | 0.5 day  |
|  006 | Introduce the chess namespace, core enums, Piece, and Square               |     1 | 003, 004      | 1.5 days |
|  007 | Implement the Move type with explicit flags and promotion                  |     1 | 006           | 1 day    |
|  008 | Implement Position state and standard setup                                |     1 | 006, 007      | 1.5 days |
|  009 | Implement strict FEN parsing, serialization, and validation                |     1 | 008           | 1.5 days |
|  010 | Harden the core and FEN regression suite                                   |     1 | 009           | 1 day    |
|  011 | Migrate to a 0x88 board and implement attack detection                     |     2 | 008           | 2 days   |
|  012 | Generate pseudo-legal pawn, knight, and king moves                         |     2 | 011           | 2 days   |
|  013 | Generate pseudo-legal sliding-piece moves                                  |     2 | 011           | 1.5 days |
|  014 | Add castling, en passant, and special-move integration                     |     2 | 012, 013      | 2 days   |
|  015 | Implement make/unmake history and legal move filtering                     |     2 | 014           | 2.5 days |
|  016 | Implement checkmate, stalemate, and game result detection                  |     2 | 015           | 1 day    |
|  017 | Add the perft harness and reference regression suite                       |     2 | 015, 016      | 1.5 days |
|  018 | Add make/unmake invariant and random-playout tests                         |     2 | 017           | 1 day    |
|  019 | Implement Zobrist hashing and draw detection                               |     3 | 015, 016      | 2 days   |
|  020 | Implement SAN/UCI notation and an interactive CLI; retire the legacy core  |     3 | 016, 019      | 2 days   |
|  021 | Expand fuzzing and add sanitizer CI                                        |     3 | 018, 020      | 1.5 days |
|  022 | Implement the search contract, negamax alpha-beta, and iterative deepening |     4 | 016, 019      | 2 days   |
|  023 | Add quiescence search and move ordering                                    |     4 | 022           | 1.5 days |
|  024 | Implement the transposition table and time management                      |     4 | 022, 023      | 2 days   |
|  025 | Create the tactical suite and deterministic search regression tests        |     4 | 024           | 1.5 days |
|  026 | Implement material, piece-square, and tapered evaluation                   |     5 | 022           | 1.5 days |
|  027 | Add positional evaluation terms                                            |     5 | 026           | 1.5 days |
|  028 | Build the self-play/SPRT harness and establish an evaluation baseline      |     5 | 023, 024, 027 | 2 days   |
|  029 | Implement the UCI adapter skeleton and handshake                           |     6 | 024, 025      | 1 day    |
|  030 | Implement UCI position/go/stop/quit with asynchronous search               |     6 | 029           | 2 days   |
|  031 | Add UCI options, detailed info output, and conformance tests               |     6 | 030           | 1.5 days |
|  032 | Validate GUI play and 1,000-game stability; write UCI usage docs           |     6 | 031, 028      | 1.5 days |
|  033 | Migrate move generation to bitboards behind the public API                 |     7 | 032           | 3 days   |
|  034 | Add advanced search pruning with measured SPRT gains                       |     7 | 032           | 2 days   |
|  035 | Add optional opening book, Syzygy tablebases, and Lazy SMP                 |     7 | 034           | 2 days   |
|  036 | Add versioning, changelog, and license metadata                            |     8 | 032           | 0.5 day  |
|  037 | Add packaging and release binaries                                         |     8 | 036           | 1 day    |
|  038 | Run the final regression matrix, tag the release, and write release notes  |     8 | 037           | 1 day    |
|  039 | Close UCI conformance gaps before release                                   |     8 | 031           | TBD      |
|  040 | Agree configuration and forcing-search behavior                            | Enhancement | Existing engine | 0.5 day |
|  041 | Parameterize evaluation with immutable settings                            | Enhancement | 040           | 1.5–2 days |
|  042 | Implement configuration schema and strict file parser                      | Enhancement | 040, 041      | 1–1.5 days |
|  043 | Apply startup configuration and UCI overrides                               | Enhancement | 039, 041, 042 | 1–1.5 days |
|  044 | Search bounded forcing continuations including quiet checks                | Enhancement | 040           | 2–3 days |
|  045 | Connect forcing controls to configuration and UCI search                    | Enhancement | 043, 044      | 0.5–1 day |
|  046 | Validate configurable evaluation and forcing search                         | Enhancement | 041–045       | 1–2 days + matches |
|  047 | Complete independent release verification                                  | Post-release | 037, 038      | 0.5–1 day + CI |
|  048 | Prototype Lazy SMP and decide whether parallel search is viable             | Post-release | 038, 039      | 4–6 days + runtime |

### Configuration and forcing-search increment (2026-10-26)

The owner approved task 040 on 2026-10-26: INI with optional executable-adjacent
discovery, values/PST scales, and bounded tactical-horizon search. Tasks
039–046 are Done, and task 038's final local gates passed with a local `v0.1.0`
tag. Task 047 tracks the unavailable independent verification (hosted CI, MSVC,
separate physical clean machine, GUI transcript). The tag is unpublished because
no git remote is configured.

### Parallel-search experiment (defined 2026-10-26)

Task 048 defines a default-off Lazy SMP proof of concept with 1-4 workers. It
compares private and shared transposition-table variants, exercises stop/join
and time-control behavior, and requires ThreadSanitizer plus stress and
fixed-time game runs. The deliverable is a measured go/no-go decision, not a
default behavior change or a strength claim.

Dependency order:

```text
040 → 041 → 042 → 043 → 045 → 046
  └────────→ 044 ────────┘
039 ─────────────→ 043
```

Task 042 can define search keys from the approved task 044 contract before its
implementation; task 045 connects them to the finished algorithm. Task 043 does
not depend on 044's implementation. If 040 approves full PST cells or extensions
inside the main search, update the affected contracts before starting them.
These estimates are additional to the original 038-task baseline. Current
implementation details and observations are recorded in the per-task reports.

## Critical path

```text
001 → 002 → 003 → 004 → 006 → 007 → 008 → 009 → 011
    → 012/013 → 014 → 015 → 017 → 019 → 022
    → 024 → 025 → 029 → 030 → 031 → 032
    → 036 → 037 → 038
```

Parallelizable work appears after the core contracts are stable:

- 004 and 005 can run in parallel with each other once 002–003 are complete;
  005 can also overlap with early Stage 1 work.
- 010 can run alongside 011 once FEN is stable.
- 018 and 019 can overlap after make/unmake is stable.
- 026/027 can overlap with UCI work once search is stable.
- 033/034/035 are optional and must not block the first release.

## Stage gates

| Gate               | After tasks | Requirement                                   |
| ------------------ | ----------- | --------------------------------------------- |
| A — Rules correct  | 017, 018    | Perft exact; random playouts clean            |
| B — Search correct | 025         | Tactical suite passes                         |
| C — Playable       | 032         | GUI game and 1,000-game stability clean       |
| D — Release        | 038         | Full matrix, sanitizers, artifacts, and notes |

## Status summary

| Status      | Meaning                                                 |
| ----------- | ------------------------------------------------------- |
| Ready       | Dependencies are complete and the task can start        |
| In progress | Actively being implemented                              |
| Blocked     | Waiting on a decision or dependency; record the blocker |
| Done        | All acceptance criteria and validation pass             |
