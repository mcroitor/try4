# Tests

The suite has twenty-one Catch2 executables, one Python real-process suite,
and one CMake smoke test registered with CTest (23 test targets):

| CTest test | Label | Coverage |
|---|---|---|
| `try4_types` | `unit` | Colors, pieces, squares, move flags, UCI formatting |
| `try4_position` | `unit` | Position setup, state, rendering, castling consistency |
| `try4_attack` | `unit` | Pawn, knight, king, and sliding attack detection |
| `try4_movegen` | `unit` | Pseudo-legal pawn, knight, king, and sliding moves |
| `try4_legal` | `unit` | Make/unmake, legal filtering, castling, en passant, promotion |
| `try4_result` | `unit` | Checkmate and stalemate detection |
| `try4_draw` | `unit` | Zobrist hashing, fifty-move, repetition, insufficient material |
| `try4_notation` | `unit` | UCI and SAN formatting, parsing, disambiguation |
| `try4_evaluation` | `unit` | Material, PST, pawn structure, mobility, bishop pair, rook files, king safety, tempo, symmetry |
| `try4_fen` | `fen` | FEN fixtures, round-trips, invalid input, errors |
| `try4_fuzz` | `fuzz` | Malformed FEN, deterministic random corpus, state safety |
| `try4_perft` | `perft` | Six reference positions at depths 1–4, bulk-count check |
| `try4_property` | `property` | 10,000 seeded random playouts with make/undo snapshots |
| `try4_search` | `search` | Alpha-beta, iterative deepening, quiescence, ordering, PVs |
| `try4_search_time` | `search` | Transposition table, mate-score adjustment, clock budgets, stop latency |
| `try4_tactical` | `tactical` | Curated mates, winning capture, stalemate trap, fixed-depth regressions |
| `try4_cli_tests` | `cli` | Scripted CLI sessions, moves, errors, perft, results |
| `try4_uci_tests` | `uci` | Handshake, options, scripted position/search/stop sessions |
| `try4_book_tests` | `book` | Polyglot reference hashes, special moves, text books, UCI selection/fallback |
| `try4_release_smoke` | `release` | Executable versions agree with CMake and UCI identity |
| `try4_config_tests` | `config` | Strict schema, defaults, parameter effects and TT policy changes |
| `try4_forcing_tests` | `forcing` | Legal's mate beyond horizon, independent minimax oracle, limits and restoration |
| `try4_engine_process` | `uci;config;forcing` | Natural completion, infinite/stop, real startup/discovery/config/override behavior |

There are 160 Catch2 test cases in total: 15 type/move, 10 position, 10 attack,
17 movegen, 14 legal, 6 result, 8 draw/hash, 6 notation, 10 evaluation, 6 FEN,
2 fuzz, 4 perft, 1 property, 9 search, 7 TT/time, 1 tactical, 4 CLI,
10 UCI, 9 book, 5 configuration, and 6 forcing cases. The separate CMake smoke
test launches both apps; the Python suite has ten process-level cases, including
quit-during-search, book-miss/tight-clock, and stop/restart/option stress.
`scripts/uci_stability.py` is the separate long-running 1,000-game protocol
harness; it is not part of CTest.

Run everything:

```sh
ctest --test-dir build/cmake -C Debug --output-on-failure
```

Run the fast suites or individual labels separately:

```sh
ctest --test-dir build/cmake -C Debug -L unit --output-on-failure
ctest --test-dir build/cmake -C Debug -L cli --output-on-failure
ctest --test-dir build/cmake -C Debug -L fen --output-on-failure
ctest --test-dir build/cmake -C Debug -L fuzz --output-on-failure
ctest --test-dir build/cmake -C Debug -L perft --output-on-failure
ctest --test-dir build/cmake -C Debug -L property --output-on-failure
ctest --test-dir build/cmake -C Debug -L search --output-on-failure
ctest --test-dir build/cmake -C Debug -L tactical --output-on-failure
ctest --test-dir build/cmake -C Debug -L uci --output-on-failure
ctest --test-dir build/cmake -C Debug -L book --output-on-failure
ctest --test-dir build/cmake -C Debug -L release --output-on-failure
ctest --test-dir build/cmake -C Debug -L config --output-on-failure
ctest --test-dir build/cmake -C Debug -L forcing --output-on-failure
```

The FEN corpus lives in `tests/data/fen/`, perft references and depth 1–6
counts live in `tests/data/perft/reference.tsv`, the tactical corpus lives in
`tests/data/tactical/positions.tsv`, and the test targets receive absolute paths
through compile definitions. Tests do not depend on the working directory.
See [`docs/TACTICAL.md`](../docs/TACTICAL.md) for the tactical data format and
current baseline.

The release gate also runs the depth 5 and depth 6 reference counts with
`scripts/perft_deep.ps1`. During 0.1.0 validation, the position-3 depth-6 value
was corrected from 11,000,083 to 11,030,083 after the engine and Stockfish 19
both returned 11,030,083. This was a fixture typo, not an engine change.

The property test uses the fixed seed `0x5EEDC0FFEE` and a 16-ply cap. On a
failure it reports the seed, game number, ply, and move log for reproduction.
Set `TRY4_PROPERTY_GAMES` or `TRY4_PROPERTY_MAX_PLIES` to run the extended
workload described in [`docs/HARDENING.md`](../docs/HARDENING.md).
It also verifies the incremental Zobrist hash against full recomputation after
every make and undo.

## CLI sessions

The CLI handler accepts injected input/output streams, so the `cli` tests run
deterministic scripts rather than launching a subprocess. The same binaries can
be exercised manually:

```text
startpos
move e2e4
move e7e5
moves
undo
result
perft 3
quit
```

The suite is deterministic and does not use the network at test runtime. Catch2
is fetched during CMake configuration when `TRY4_BUILD_TESTS=ON`; for an offline
build, vendor Catch2 and replace the `FetchContent_Declare` call in
`CMakeLists.txt` with an `add_subdirectory` call.

## UCI stability run

`scripts/uci_stability.py` drives two real UCI processes through repeated clock
searches, validates every returned move by replaying it through the debugging
CLI (perft-tested legal move generation), probes `isready`/`go infinite`/`stop`,
and archives JSON, CSV, PGN and a text summary. It counts illegal moves, crashes,
hangs, deadline violations and clock forfeits; any failure makes it exit nonzero.

```sh
python scripts/uci_stability.py --engine build/feature-check/try4_uci.exe \
  --games 1000 --workers 4 --output build/feature-validation/stability-1000
```

The harness uses 1000 ms + 50 ms increment with `movestogo 200` by default and
is a stability check, not a strength measurement. Result reports from the
release run are summarized in [`docs/RELEASE_VALIDATION.md`](../docs/RELEASE_VALIDATION.md).
