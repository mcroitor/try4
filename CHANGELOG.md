# Changelog

Changes follow Keep a Changelog and Semantic Versioning. A version is released
only when its tag and release report exist.

## [Unreleased]

## [0.1.1] — candidate (not released)

### Added

- Bitboard occupancy-mask and iteration helpers with boundary tests; the full
  Position/move-generation migration remains task 033.
- Default-off Lazy SMP proof of concept and its measured go/no-go report.

- Task 035: optional Syzygy WDL+DTZ probing via `SyzygyPath` /
  `[uci] syzygy_path`, using the vendored MIT-licensed Fathom library. Root
  probing respects the fifty-move clock; interior WDL probing is gated on a
  zero halfmove clock. Missing data falls back to search, and no tablebase
  data is bundled. See `docs/SYZYGY.md`.
- Task 058: configurable resignation. `[search] resign_cp` (default -600, 0
  disables) and the UCI `Resign` spin option; a completed search below the
  threshold emits `info string resign ...` followed by `bestmove 0000`.
  Benchmarks and forcing/tactical measurements disable it explicitly.
- Task 057: reworked clock budget (`soft`/`hard`/safety formula with the move
  overhead), an iteration-start predictor with a measured branching factor
  and a best-move-instability extension to the hard ceiling; the
  `scripts/match_report.py` PGN match reporter and a PGN opening book for
  managed matches.

### Changed

- Allocation-free search primitives, fixed PV storage, staged move ordering,
  improved transposition-table reuse and single-pass evaluation (tasks 050–054).
- Principal-variation search, aspiration windows and guarded search pruning;
  LMR and check extensions remain disabled by default (tasks 055–056).
- Task 047 prepares 0.1.1 and reconciles task/release status with hosted CI
  evidence. Release artifacts retain JUnit/logs and source/toolchain provenance.
- The extracted-package verifier requires Syzygy documentation and the Fathom
  license and rejects bundled Syzygy tablebase data.

- UCI now emits one `info depth ...` line after each completed iterative-deepening
  iteration instead of only a single line when the search finishes. A terminal
  root, immediate stop, or zero budget still emits an `info depth 0` line before
  `bestmove`.

### Fixed

- Aligned bitboard helper return types, implemented the missing iterator and
  corrected invalid-square behavior and test arguments.
- Avoided MSVC's deprecated `getenv` in Syzygy tests with `_dupenv_s`.

## [0.1.0] — 2026-10-26

### Added

- Startup INI configuration for UCI defaults, MG/EG material and positional
  weights, PST scales, and tactical-horizon budgets; optional executable-adjacent
  discovery and explicit `--config` selection.
- Bounded forcing continuation search including optional quiet checks, all check
  evasions and explicit hard-limit iteration aborts.
- Immutable search settings, evaluation/forcing TT policy invalidation, and
  real-process configuration/protocol and independent tactical-oracle tests.
- Stage 8 release preparation: shared CMake version, `--version`, versioned
  UCI identity, install rules, ZIP packages, SHA-256 checksums, and release CI.
- MIT license, contribution instructions, UCI usage, and release documentation.

### Fixed

- `go infinite` now waits for `stop`, including mate/terminal results; ready
  handling remains responsive. Mate scores use UCI mate units/signs.
- UCI input validation, transactional position commands, whitespace support,
  unsupported-mode diagnostics, and removal of ignored Chess960 advertising.
- Quiescence no longer treats an unresolved checked hard-limit node as an
  exact static evaluation.
- Corrected the position-3 depth-6 perft reference from 11,000,083 to
  11,030,083; the engine count was confirmed independently with Stockfish 19.
- Corrected the promotion-check test fixture so the position is legal before
  the promotion.

### Validation

- 23/23 CTest targets passed on Windows Debug, Windows Release, Ubuntu Release
  and Linux ASan/UBSan. The sanitizer run produced no findings.
- Depth 5 and depth 6 perft passed for all six reference positions.
- A 1,000-game UCI protocol stability run completed with no failures; maximum
  move wait was 67.745 ms and maximum `stop` latency 0.516 ms.
- Windows and Linux archives were extracted and verified. A fresh local clone
  repeated the build/test/package flow.

See `docs/RELEASE_VALIDATION.md` for commands, commit and checksums.

### Initial release contents

- C++17 chess core with legal move generation, strict FEN, make/unmake,
  draw detection, SAN/UCI notation, and incremental hashing.
- Alpha-beta search with iterative deepening, quiescence, move ordering,
  transposition table, time budgets, and positional evaluation.
- UCI engine with asynchronous search and a separate debugging CLI.
- Optional Polyglot and tournament-text opening books.
- Unit, perft, property, tactical, search, UCI, and opening-book tests.
- Self-play evaluation harness with PGN/CSV reports.

### Known limitations

- Single-thread search; no Chess960, pondering, `searchmoves`, `go mate`, NNUE,
  SMP or tablebase probing.
- Evaluation and forcing settings are file-only and require a restart.
- The 1,000-game run is identical-engine stability testing, not strength
  measurement. No calibrated rating is claimed.
- At the original validation date no remote, hosted CI, MSVC run or separate
  physical clean machine was available. Current post-0.1.0 evidence is tracked
  in `docs/RELEASE_CANDIDATE.md`; the original tag is absent from the current
  checkout and GitHub tags API.
