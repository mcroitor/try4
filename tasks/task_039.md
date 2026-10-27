# Task 039 — Close UCI conformance gaps before release

| Field | Value |
|---|---|
| ID | 039 |
| Stage | 8 — Release follow-up |
| Priority | P1 |
| Status | Done |

## Objective

Resolve the protocol gaps identified while preparing the Stage 8 release.
Task 038 remains blocked until this focused follow-up and the final regression
matrix pass. This task does not add strength features.

## Scope and acceptance criteria

- [x] `go infinite` stays active until `stop`, including after finding a mate.
- [x] Emit mate scores using UCI `score mate` with tested distance/sign.
- [x] Remove unsupported advertised options (Chess960), enforce option ranges,
      and accept general command whitespace without partial-position surprises.
- [x] Emit `readyok` only as an `isready` response.
- [x] Document or implement `searchmoves`/ponder behavior consistently.
- [x] Exercise depth/time searches to natural completion in process-level tests;
      immediate `quit` tests alone do not prove limits work.
- [x] Stress `isready`, stop/restart, option reload and quit during search;
      record stop latency and absence of deadlocks/output corruption.
- [x] Rerun unit/UCI/book/time tests and Linux sanitizers.

## Evidence

### Implementation progress — 2026-10-26

Implemented bounded analysis depth plus adapter wait-until-stop (including mate
and terminal results), condition-variable cancellation, mate output, validated
go/option ranges, transactional position parsing, whitespace support, removal
of ignored Chess960 advertisement, and no unsolicited newgame readyok.
Ponder/searchmoves/go mate are rejected with an explicit UCI info diagnostic.
Real-process tests cover natural completion and stop/option restart. Final
cross-platform/sanitizer results are recorded in the feature validation reports.
The evidence below describes the pre-fix candidate, not current behavior.

The current parser treats `infinite` as a no-op, while `Searcher::Search`
defaults unbounded searches to depth 4 and exits early on mate. Current UCI
output reports all scores in centipawns, advertises ignored Chess960 support,
and sends `readyok` from `ucinewgame`. See `docs/UCI.md`.

### Final completion — 2026-10-26

**Status:** Done.

- `go infinite` remains active until `stop`, including terminal roots and found
  mates, while `isready` stays responsive.
- UCI mate scores use `score mate` with verified sign/distance; the process suite
  checks both a winning and a losing mate fixture.
- Chess960 is no longer advertised; option ranges and transactional position
  parsing are validated; general whitespace is accepted.
- `ucinewgame` no longer emits an unsolicited `readyok`.
- `ponder`, `searchmoves` and `go mate` are rejected with a documented
  `info string` diagnostic; `ponderhit` is ignored because no ponder search can
  start.
- Ten real-process tests cover natural depth/time/nodes completion, mate output,
  infinite/stop, quit during search, startup/config/book failures, a tight
  clock after a book miss, and a 12-iteration stop/restart/option stress loop.
- Final suites: 23/23 CTest targets on Windows Debug, Windows Release and Linux
  Release; 23/23 under Linux ASan/UBSan (194.75 s, no findings).
- Stop latency: 40 release-run probes, maximum 0.516 ms; process-test stress
  loop passed with all stops under the 250 ms assertion bound.
- 1,000-game protocol run completed with zero illegal moves, hangs, crashes,
  time forfeits or duplicate bestmoves:
  `docs/stability/uci-1000/`.

Files: `src/uci/uci.cpp`, `include/chess/uci.hpp`, `src/core/search.cpp`,
`tests/uci_tests.cpp`, `tests/engine_process_tests.py`, `docs/UCI.md`,
`scripts/uci_stability.py`.

Small follow-ups kept out of scope: periodic `info` during search, pondering,
`searchmoves`, and `go mate`. None is a release blocker.
