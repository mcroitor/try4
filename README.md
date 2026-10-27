# try4

`try4` is a C++17 standard-chess engine with a UCI interface, reusable core,
debugging CLI, and optional opening books. **Version 0.1.0 is tagged locally as
`v0.1.0` after the final local regression matrix.** The repository has no remote,
so the tag and release archives are not published; see
[`docs/RELEASE_VALIDATION.md`](docs/RELEASE_VALIDATION.md) for evidence and
limitations.

## Current status

**Core, debugging CLI, search, evaluation, UCI, and opening books implemented.** The current code can:

- represent a position with a private 0x88 board and public `Square` indices;
- parse and serialize strict FEN;
- generate pseudo-legal and legal moves, including castling, en passant, and
  promotions;
- make and undo legal moves with exact state restoration;
- detect checkmate, stalemate, fifty-move, threefold-repetition, and
  insufficient-material draws;
- maintain an incremental Zobrist hash;
- format and parse UCI and SAN notation;
- inspect and play positions through a line-oriented CLI;
- run perft against six standard reference positions at depths 1вЂ“4;
- search legal moves with fail-soft alpha-beta, iterative deepening,
  quiescence, deterministic move ordering, a transposition table, and
  movetime/clock budgets with an atomic stop flag;
- evaluate positions with tapered material, piece-square tables, pawn
  structure, mobility, bishop pair, rook files, king safety, and tempo from the
  side-to-move perspective, with optional startup configuration of material,
  positional weights and PST scales;
- search bounded forcing continuations (captures, promotions and optionally
  quiet checks) while retaining legal fallbacks at limits;
- measure the full evaluator against a material-only baseline with reproducible
  self-play, color swapping, PGN/CSV logs, Elo margins, and a recorded opening
  set.

The `try4_uci` executable provides the UCI protocol and optional Polyglot or
tournament-text opening books. Automated evaluation-weight tuning is not
implemented yet. The project owner reports stable CuteChess play; the archived
1,000-game protocol run and final regression matrix are documented in
[`docs/RELEASE_VALIDATION.md`](docs/RELEASE_VALIDATION.md) and
[`docs/stability/uci-1000/`](docs/stability/uci-1000/).
See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the target design,
[`docs/ROADMAP.md`](docs/ROADMAP.md) for the staged plan,
[`docs/PERFORMANCE.md`](docs/PERFORMANCE.md) for the perft baseline,
[`docs/EVALUATION.md`](docs/EVALUATION.md) for the material, PST, and positional-term design,
[`docs/TACTICAL.md`](docs/TACTICAL.md) for the tactical baseline,
[`docs/selfplay/BASELINE.md`](docs/selfplay/BASELINE.md) for the recorded
self-play baseline, and
[`docs/HARDENING.md`](docs/HARDENING.md) for fuzz and sanitizer coverage,
[`docs/CONFIGURATION.md`](docs/CONFIGURATION.md) for startup configuration,
[`docs/FORCING_SEARCH.md`](docs/FORCING_SEARCH.md) for bounded forcing search,
and [`docs/RELEASE_NOTES.md`](docs/RELEASE_NOTES.md) for the 0.1.0 release notes.

## Prerequisites

- CMake 3.20 or newer
- A C++17 compiler (GCC, Clang, or MSVC)
- Ninja or Make (whichever CMake selects)
- Network access during the first CMake configure if Catch2 is not already
  available locally
- Optional: NetBeans with the existing project metadata
- Python 3 for process-level tests (when `TRY4_BUILD_TESTS=ON`)

Catch2 is fetched with CMake `FetchContent` when `TRY4_BUILD_TESTS=ON`. Tests do
not need network access at runtime. For an offline build, vendor Catch2 and
replace the `FetchContent_Declare` call in `CMakeLists.txt` with an
`add_subdirectory` call.

## Build and run

From the repository root:

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug -DTRY4_BUILD_TESTS=ON
cmake --build build/cmake --config Debug
```

Run the interactive CLI:

```sh
# Linux/macOS
./build/cmake/try4

# Windows (Ninja/MinGW)
build\cmake\try4.exe
```

The CLI starts from the normal initial position. Commands are `startpos`,
`fen <FEN>`, `board`, `moves`, `move <uci|san>`, `undo`, `perft <depth>`,
`result`, `help`, and `quit`. For example:

```text
startpos
move e2e4
move Nf6
board
moves
result
quit
```

To use warning-as-error mode:

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/cmake --config Debug
```

Build Release executables without the test suite:

```sh
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTRY4_BUILD_TESTS=OFF -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/release --config Release
```

See [`docs/selfplay/README.md`](docs/selfplay/README.md) for the protocol and
reproduction command.

## Run in CuteChess

Select `build/release/try4_uci.exe` (Windows) or `build/release/try4_uci`
(Linux) as a UCI engine. `try4` is the separate debugging CLI.

```sh
build/release/try4_uci --version
```

Both the command above and UCI `id name` report `try4 0.1.0`. See
[`docs/UCI.md`](docs/UCI.md) for commands, options, and current limitations.

## Install and package

```sh
cmake --install build/release --config Release --prefix build/install
cpack --config build/release/CPackConfig.cmake -C Release
```

ZIPs and SHA-256 files are written to `build/release/packages/`, named
`try4-<version>-<platform>-<architecture>.zip`. Packages include the UCI engine,
debug CLI, license, and usage/release documents; opening books are selected
separately. For a tested candidate build, artifact verification, and release
gates, see [`docs/RELEASE.md`](docs/RELEASE.md).

## Engine configuration and forcing search

```sh
build/release/try4_uci --config config/try4.ini
```

Without `--config`, optional `try4.ini` beside the executable supplies startup
defaults. Later UCI options override the file. Invalid files fail startup with
a diagnostic. The example includes piece values, MG/EG positional weights,
PST scale factors, and bounded tactical-search controls. To include quiet checks
beyond the normal depth horizon, set `[search] forcing_checks=true`.

See [configuration schema](docs/CONFIGURATION.md) and
[forcing-search semantics](docs/FORCING_SEARCH.md). Piece-square table cells
remain built in; this feature adds horizon search, not main-search extensions.

## UCI opening books

Build `try4_uci` with the CMake commands above, then add
`build/release/try4_uci.exe` (or the Debug executable) as a UCI engine in
CuteChess. In the engine's settings, configure:

| Option | Default | Meaning |
|---|---|---|
| `OwnBook` | `true` | Use the selected opening book. |
| `BookFile` | empty | Path to a Polyglot `.bin` or tournament `.txt` book. |
| `BookMaxPly` | `40` | Stop using the book after this many half-moves; `0` means unlimited. |

No book is loaded until `BookFile` is set. Prefer an absolute path in CuteChess,
since relative paths are resolved against the engine process's working directory.
For the supplied book, set `BookFile` to the full path of
`docs/selfplay/tournament.txt`. Paths containing spaces are supported; UCI values
are supplied without surrounding quotation marks.

Example UCI session, launched from the repository root:

```text
uci
setoption name BookFile value docs/selfplay/tournament.txt
setoption name OwnBook value true
setoption name BookMaxPly value 40
isready
position startpos
go movetime 100
```

A book hit emits `info string Book move <move>` followed by `bestmove <move>`
without searching. Book misses, disabled books, and the ply limit fall back to
normal search. `go infinite` and `go ponder` bypass the book. The ply limit uses
the position's fullmove number and side to move, so it also works with FEN input.

Polyglot books use the standard keys, big-endian records, and weights (zero-weight
entries are ignored). Castling, promotions, captures, and en passant are supported.
The `.txt` format is the same as the self-play opening set:

```text
# Opening family variants; fields separated by a tab
italian-1	e2e4 e7e5 g1f3 b8c6 f1c4
italian-2	e2e4 e7e5 g1f3 b8c6 f1c4 f8c5
```

Each text line is replayed from the initial position and indexed at every ply.
Identical continuations are merged; their weights count occurrences across lines.
Both formats select randomly in proportion to move weights and validate every
candidate against legal moves. Transpositions are matched by position, independent
of move clocks. Loading an invalid file reports `info string Book load failed: ...`
and clears the previous book so the engine continues with normal search. Empty
books and malformed text lines are rejected. Set `BookFile` to `<empty>` to unload.

## Test

```sh
ctest --test-dir build/cmake -C Debug --output-on-failure
```

The suite has 23 CTest targets: 21 Catch2 executables, one Python real-process
suite, and one CMake version/UCI smoke test. It covers unit, CLI, FEN, fuzz,
perft, randomized property, search, tactical, UCI, opening-book, configuration,
forcing-search, and package-entry-point behavior.
See [`tests/README.md`](tests/README.md) for labels, counts,
data paths, and the property-test seed. The 1,000-game protocol stability harness
is separate from CTest; its archived release report is in
[`docs/stability/uci-1000/`](docs/stability/uci-1000/).

## NetBeans

The original NetBeans project remains in `nbproject/`. The files under
`nbproject/Makefile-*.mk` are generated and must not be edited by hand. Prefer
the CMake build for tests and CI. If you use NetBeans, keep the generated
project files intact.

## Project layout

```text
CMakeLists.txt          Canonical build
include/chess/          Public headers: types, Position, movegen, search, eval, config
src/core/               Core implementation (0x88, FEN, moves, hash, search, eval, config)
src/uci/                UCI adapter and option/configuration integration
src/cli/                Testable CLI command loop
apps/                   CLI and UCI executable entry points
bench/                  Perft benchmark and self-play executables
config/                 Non-active example try4.ini
scripts/                Deep-perft, benchmark, stability and package verification helpers
tests/                  Unit, CLI, FEN, fuzz, perft, property, search, tactical, process tests
docs/                   Architecture, roadmap, release, configuration, forcing, self-play, hardening notes
tasks/                  Executable task backlog with dependencies and gates
```

## Documentation

- [Architecture assessment and target design](docs/ARCHITECTURE.md)
- [Development roadmap](docs/ROADMAP.md)
- [Perft performance baseline](docs/PERFORMANCE.md)
- [Evaluation design and positional terms](docs/EVALUATION.md)
- [Tactical suite and fixed-depth search baseline](docs/TACTICAL.md)
- [Self-play harness protocol](docs/selfplay/README.md)
- [Evaluation baseline report](docs/selfplay/BASELINE.md)
- [Fuzzing and sanitizer notes](docs/HARDENING.md)
- [Task backlog](tasks/README.md)
- [Test notes](tests/README.md)
- [UCI usage and limitations](docs/UCI.md)
- [Startup configuration schema](docs/CONFIGURATION.md)
- [Bounded forcing search](docs/FORCING_SEARCH.md)
- [Release engineering](docs/RELEASE.md)
- [0.1.0 release notes](docs/RELEASE_NOTES.md)
- [1,000-game UCI stability report](docs/stability/uci-1000/README.md)
- [Final release validation](docs/RELEASE_VALIDATION.md)
- [Changelog](CHANGELOG.md)
- [Contributing](CONTRIBUTING.md)

## Development workflow

Work through the backlog in dependency order:

1. Open the task file and read its objective, scope, and acceptance criteria.
2. Confirm every dependency listed in the task is complete.
3. Implement only what is in scope.
4. Run the task's validation commands.
5. Mark the task status as `Done` only when all acceptance criteria pass.
6. Report changed files, commands, results, and any follow-ups.

A task is complete only when the build succeeds, new behavior has tests, the full
test suite passes, no new warnings are introduced, and relevant documentation is
updated. See [`tasks/README.md`](tasks/README.md) for the project-wide definition
of done and stage gates.

## License

Project code is licensed under the [MIT license](LICENSE), selected by the
project owner. See [third-party notices](THIRD_PARTY_NOTICES.md) for format
constants, test dependencies, and externally supplied opening data.
