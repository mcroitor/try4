# Task 043 — Apply startup configuration and UCI overrides

| Field | Value |
|---|---|
| ID | 043 |
| Stage | Engine configuration integration |
| Depends on | 039, 041, 042 |
| Estimate | 1–1.5 days |
| Priority | P1 |
| Status | Done |

## Objective

Make `try4_uci` load the approved settings at startup, apply evaluation and UCI
defaults consistently, and preserve later GUI overrides without search races.

## Scope and requirements

1. Implement `--config <path>` and task 040's approved optional discovery policy.
   Explicit paths win over discovery. Report malformed/missing arguments.
   `--version` remains usable without file access. Define behavior for unknown
   arguments instead of silently starting an interactive loop.
2. Load and validate all settings before allocating configured resources or
   starting protocol processing. Startup errors go to stderr with a nonzero
   exit code; no debug/configuration text appears as unstructured UCI stdout.
3. Resolve relative BookFile values against the selected configuration directory;
   later UCI BookFile values keep their documented runtime-path semantics.
   Record the distinction clearly in CONFIGURATION.md and UCI.md.
4. Apply built-ins < file < session `setoption`. UCI option declarations must
   reflect effective startup defaults, including values loaded from the file.
   `ucinewgame` clears game/search state, not configured defaults or session
   overrides. Use task 039's range validation and response conventions.
5. Pass task 041's evaluation snapshot into all normal searches. Configured
   values must change real scores, not only a settings object or display text.
6. Stop/join before changing resources used by a worker; invalidate affected TT
   data. Keep configuration ownership and output-stream lifetimes explicit.
   Reloading files during search is outside scope.
7. Keep the CLI/core/self-play default behavior unchanged unless an entry point
   is explicitly opted into the same configuration API. Document which entry
   points support `--config`; 046 adds harness support for reproducible A/B use.
8. Install a non-active example under a documented examples directory and include
   schema documentation in runtime packages. Do not ship a live try4.ini beside
   the engine that overrides existing users' settings without their choice.

## Tests and expected behavior

- Launch from an unrelated working directory with an explicit path containing
  spaces; configured Hash/book/default options and an actual book move are observed.
- Missing optional file succeeds with defaults; missing explicit or malformed
  file exits nonzero before search; protocol stdout remains clean.
- File value followed by UCI override uses the override; `ucinewgame` preserves it.
- A material-imbalance FEN searched at a fixed depth uses configured piece values;
  compare to a directly parameterized evaluator/search oracle, avoiding snapshots
  that merely repeat implementation details.
- `--version` matches the build version even when a discovered file is invalid.
- Reloaded BookFile and Hash via UCI during/after search do not race or retain
  stale TT values. Invalid UCI updates retain the prior valid setting.

## Acceptance criteria and validation

- [x] Startup discovery/precedence/errors work in subprocess tests.
- [x] Configuration changes actual UCI options, book use and evaluation.
- [x] Session overrides, isolation, TT invalidation and thread lifetime tests pass.
- [x] Packaged executable loads the example with explicit `--config`.
- [x] UCI/config/book/evaluation/time tests and the full suite pass.

Use a Release subprocess test with bounded timeouts, Debug regression tests, and
the package verifier from Stage 8. Include a Linux sanitizer run in 046.

## Completion report

List files, exact startup rules, subprocess evidence, commands/results and
configuration-to-score evidence. Do not mark task 039 done merely because these
new options work, and do not claim hot reload support.

### Completion — 2026-10-26

**Status:** Done.

- Startup rules: built-ins < explicit `--config` or optional
  executable-adjacent `try4.ini` < session `setoption`. `--version` never reads a
  file. Missing discovery is allowed; missing explicit file, unknown argument or
  malformed file exits nonzero with stderr only.
- UCI declarations show effective startup defaults. `ucinewgame` retains
  defaults/overrides and does not emit `readyok`.
- Relative config `book_file` resolves against the config directory; later UCI
  `BookFile` remains relative to process cwd. The example is installed under
  `share/doc/try4/examples/`, never beside the engine.
- Real-process test covers a path containing spaces, `#`, `;` and `=`, actual
  book move, override/newgame, missing/malformed files and `--version`.
- Configured pawn value changed a real UCI score by exactly +100; configured
  hash/book values appeared in declarations and behavior.
- Packaged Windows/Linux engines loaded an external config from a different cwd
  and passed `scripts/verify_release.py`.
- No hot reload is implemented or claimed.

Files: `apps/uci_main.cpp`, `include/chess/uci.hpp`, `src/uci/uci.cpp`,
`src/core/search.cpp`, `src/core/opening_book.cpp`,
`tests/engine_process_tests.py`, `docs/CONFIGURATION.md`, `docs/UCI.md`.
