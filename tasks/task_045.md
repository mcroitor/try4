# Task 045 — Connect forcing controls to configuration and UCI search

| Field | Value |
|---|---|
| ID | 045 |
| Stage | Configuration and tactical-search integration |
| Depends on | 043, 044 |
| Estimate | 0.5–1 day |
| Priority | P1 |
| Status | Done |

## Objective

Ensure configured tactical policy changes the search actually run by the UCI
engine, with consistent precedence, bounded settings and cache invalidation.

## Requirements

1. Map task 042's approved `[search]` keys to task 044's settings without a
   second set of independent defaults or magic numbers.
2. Preserve an immutable per-search snapshot combining evaluation and forcing
   parameters; starting a later search cannot mutate an earlier worker's state.
3. File-only controls stay file-only unless task 040 approved corresponding UCI
   options. If exposed, document names/types/ranges and implement file < UCI
   override precedence with stop/join and TT invalidation.
4. `ucinewgame` retains selected settings. Book hits remain legal immediate
   responses; out-of-book searches use the selected tactical configuration.
5. Keep node/time budgets global to main plus forcing search, not separate
   allowances that can silently double a GUI's requested time or nodes.
6. Complete configuration/example/UCI/forcing documentation and packaged example
   coverage. No active config file is installed by default.

## Validation cases

- A fixed FEN with a verified quiet-check continuation produces the approved
  enabled/disabled comparison through the real process and `--config`.
- Book miss routes to configured search; changing BookFile does not reset
  evaluation or forcing settings.
- Changed forcing budget/policy with a reused engine gives the same score as
  a fresh engine with equivalent effective configuration.
- Invalid budget values are rejected, and a finite time/node/stop request holds
  even in a long checking sequence. Output remains valid UCI and contains a
  legal bestmove exactly once per completed go/stop transaction.

## Acceptance criteria and commands

- [x] No gap between parsed settings, effective UCI state and search behavior.
- [x] Combined evaluation/forcing cache and snapshot tests pass.
- [x] Default compatibility and book/search/time behavior remain documented.
- [x] Config/UCI/book/tactical/time tests pass in Debug and Release.

Use CMake/CTest commands from the README and task 043's process harness.
Report exact effective settings and commands with each tactical result.

## Completion report

Return files, precedence/settings mapping, actual process-level results,
cache-invalidation evidence, failures and smallest follow-up. Do not mark a
search complete when only configuration parsing has been exercised.

### Completion — 2026-10-26

**Status:** Done.

- `[search]` keys map directly to the single `ForcingSearchSettings` contract;
  no duplicate defaults or magic second budget.
- Each `SearchLimits` snapshot combines evaluation and forcing policy. Starting
  another search cannot mutate an earlier worker; TT validity rejects entries
  from a different evaluation/forcing/quiescence policy.
- Evaluation and forcing remain file-only (as approved). Reference/time/limits
  are global to main plus forcing search; limits are not doubled.
- `ucinewgame` preserves settings. A book hit returns immediately; a book miss
  uses the selected forcing configuration.
- Process test: `--config` with checks disabled gives `-386 cp`; enabled gives
  `mate 2`/`c4f7`; reverse side gives `mate -1`. A book miss with a tight clock
  still returns a legal move.
- Twelve-iteration stop/restart/option reload stress passed; every stop was
  under the 250 ms assertion bound, with no deadlock or duplicate bestmove.
- Default settings remain checks-disabled for A/B compatibility.
- Debug, Release and sanitizer suites pass; packaged config/override smoke
  passes.

Files: `src/uci/uci.cpp`, `src/core/configuration.cpp`,
`include/chess/configuration.hpp`, `src/core/search.cpp`,
`tests/configuration_tests.cpp`, `tests/engine_process_tests.py`,
`docs/CONFIGURATION.md`, `docs/UCI.md`.
