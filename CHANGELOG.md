# Changelog

Changes follow Keep a Changelog and Semantic Versioning. A version is released
only when its tag and release report exist.

## [Unreleased]

### Added

- Task 058: configurable resignation. `[search] resign_cp` (default -600, 0
  disables) and the UCI `Resign` spin option; a completed search below the
  threshold emits `info string resign ...` followed by `bestmove 0000`.
  Benchmarks and forcing/tactical measurements disable it explicitly.

### Changed

- UCI now emits one `info depth ...` line after each completed iterative-deepening
  iteration instead of only a single line when the search finishes. A terminal
  root, immediate stop, or zero budget still emits an `info depth 0` line before
  `bestmove`.

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
- The tag is local because no git remote is configured; hosted CI, MSVC and a
  separate physical clean machine were not available.
