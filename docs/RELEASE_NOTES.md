# try4 0.1.0 — release notes

**Release status:** version 0.1.0 is tagged locally as `v0.1.0` after the final
local regression matrix. The repository has no configured remote, so this tag
and the Windows/Linux archives have not been published. Hosted CI and a separate
physical clean machine were unavailable; see the limitations below.

## Included

- Standard-chess UCI engine with single-thread alpha-beta search, iterative
  deepening, quiescence, a transposition table, clock/movetime/node limits and
  positional tapered evaluation.
- Debugging CLI for FEN, board/move inspection, undo, results and perft.
- Optional Polyglot and tournament-text opening books selected with UCI options.
- Startup INI configuration for UCI defaults, MG/EG material and positional
  weights, PST scales and bounded forcing-search controls. Explicit `--config`
  or optional executable-adjacent `try4.ini`; built-ins < file < session
  `setoption`.
- Bounded horizon forcing search: captures, promotions and optionally quiet
  checks when not in check; all legal evasions when in check. Checks are disabled
  by default for controlled A/B behavior.
- Native Windows/Linux ZIP packaging with shared executable/UCI version and
  SHA-256 checksums.
- MIT-licensed project code; see third-party notices for external data.

## Validation summary

Final local gates passed:

- Windows Debug, Windows Release, Ubuntu Release and Linux ASan/UBSan suites:
  **23/23 CTest targets each**; sanitizer run clean (194.75 s).
- Deep perft: **depth 5 and depth 6 passed for all six reference positions**.
- Recorded 1,000-game UCI protocol stability run: **1000/1000 games, zero
  protocol failures**, maximum move wait 67.745 ms, maximum `stop` latency
  0.516 ms (1000 ms + 50 ms, `movestogo 200`).
- Windows and Linux archives verified after extraction with a reduced `PATH`,
  including versions, UCI handshake, explicit config with spaces, relative book
  path and invalid-config failure.
- The same build/test/package flow passed from a fresh local clone of the tagged
  source.

Exact commands, commit, archive sizes and checksums are in
[`RELEASE_VALIDATION.md`](RELEASE_VALIDATION.md). Feature evidence is in
[`CONFIGURATION_VALIDATION.md`](CONFIGURATION_VALIDATION.md),
[`FORCING_SEARCH_VALIDATION.md`](FORCING_SEARCH_VALIDATION.md) and the archived
[`stability/uci-1000/`](stability/uci-1000/) report.

## Limitations and non-goals

- No native GUI, Chess960, pondering, `searchmoves`, `go mate`, NNUE, SMP or
  Syzygy support. Unsupported go modes are rejected rather than silently
  starting a different search.
- Search info is emitted at completion, not periodically during a search.
- Evaluation and forcing settings are file-only and require a restart; there is
  no hot reload.
- The 1,000-game run is protocol-level identical-engine self-play performed on
  Windows. It is not a strength measurement and does not replace GUI-side
  testing. The owner separately reported stable CuteChess play, but no GUI
  transcript was archived.
- The configurable evaluation and forcing search have small A/B evidence, not a
  calibrated Elo claim.
- The Linux archive was built on Ubuntu with glibc 2.43. Compatibility with older
  distributions has not been established.
- The release workflow was not executed by hosted CI, and no MSVC build was run.
  Local clean-clone verification plus isolated-PATH archive extraction is the
  substitute. The repository has no remote, so the tag is local and unpublished.
- Opening data and opening books are excluded from runtime ZIPs. Supply your own
  book path.

## After 0.1.0

Deferred verification and hardening work is tracked in
[`tasks/task_047.md`](../tasks/task_047.md): hosted CI execution, a separate
physical clean machine, MSVC coverage, older-glibc packaging if needed, a GUI
transcript, and further strength evaluation only with reproducible paired games
and uncertainty estimates.
