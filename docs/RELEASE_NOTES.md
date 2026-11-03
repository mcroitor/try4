# try4 0.1.1 — candidate release notes

**Status (2026-11-03):** release preparation under task 047; not tagged or
published. Source is hosted at [mcroitor/try4](https://github.com/mcroitor/try4).
The merged baseline `e6d6b07` passed hosted Ubuntu, Windows/MSVC and ASan/UBSan
CI. Candidate validation and remaining gates are recorded in
[`RELEASE_CANDIDATE.md`](RELEASE_CANDIDATE.md).

## Changes since 0.1.0

- Executable names now match their roles: `try4` is the playable UCI engine
  (previously `try4_uci`), and `try4_cli` is the debugging console (previously
  `try4`). Select `try4.exe` on Windows or `try4` on Linux in your chess GUI.
- Optional Syzygy WDL+DTZ probing through `SyzygyPath` and `[uci] syzygy_path`,
  using the vendored MIT-licensed Fathom library. Root probes honor the
  fifty-move clock; interior WDL probes require a zero halfmove clock. Missing
  data falls back to search. See [`SYZYGY.md`](SYZYGY.md).
- Configurable UCI resignation: `Resign` / `[search] resign_cp`, default
  `-600` cp, with `0` disabling it. The diagnostic precedes `bestmove 0000`.
  Consumers must recognize this convention; GUI behavior still needs an
  archived verification transcript.
- Allocation-free search primitives, fixed PV storage, a score-once staged
  move picker, improved transposition-table reuse and single-pass evaluation.
- Principal-variation search and aspiration windows; guarded null-move and
  delta pruning. Late-move reductions and check extensions are implemented but
  disabled by default after their measured regressions.
- Revised clock budgets, iteration-start prediction and bounded best-move
  instability extension. Per-iteration UCI information replaces completion-only
  information.
- Bitboard mask/iteration helpers and tests. The engine's Position and move
  generation still use 0x88; task 033's migration is not complete.
- An experimental, default-off Lazy SMP build option. Its private-table PoC
  has recorded limitations; this candidate ships with `TRY4_SMP_POC=OFF`.
- CI fixes for the bitboard helper/test API and deprecated `getenv` under MSVC.
- Release evidence uploads now retain JUnit/test logs on failure and include
  source/toolchain provenance with successful candidate packages.
- Extracted-package verification checks Syzygy documentation, the Fathom license
  and exclusion of external tablebase data.

## Included runtime and data

The package contains the UCI engine, debugging CLI, MIT license, third-party
notices, Fathom license, configuration example and usage/release documentation.
Opening books and Syzygy files are external; neither is bundled. Existing
startup INI configuration and session UCI overrides remain supported.

## Compatibility and limitations

- C++17; Windows and Linux builds. Default runtime search is single-threaded.
- No native GUI, Chess960, pondering, `searchmoves`, `go mate` or NNUE.
- Evaluation/forcing startup settings require a restart; no hot reload.
- `bestmove 0000` on resignation can be interpreted differently by different
  GUIs. Set `Resign` to `0` when the consumer does not recognize the convention.
- The local Linux x86-64 package was tested on Ubuntu 26.04.1 under WSL2
  (glibc 2.43). It dynamically links the system C/C++ runtimes and requires
  symbols through `GLIBC_2.34`, `GLIBCXX_3.4.32` and `CXXABI_1.3.9`.
  Compatibility with older distributions has not been verified.
- Hosted Debug/sanitizer CI is verified for the merged baseline. The candidate
  Release workflow, separate physical clean machine and GUI transcript remain
  task 047 gates; see its evidence matrix before publication.
- Archived protocol stability and short TSCP matches are not calibrated Elo
  evidence. The planned 200-game, zero-forfeit strength gate is not established.

The original 0.1.0 matrix and artifact checksums remain in
[`RELEASE_VALIDATION.md`](RELEASE_VALIDATION.md); those results do not certify
the newer candidate.
