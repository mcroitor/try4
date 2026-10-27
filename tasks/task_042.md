# Task 042 — Implement the configuration schema and strict file parser

| Field | Value |
|---|---|
| ID | 042 |
| Stage | Engine configuration |
| Depends on | 040, 041 |
| Estimate | 1–1.5 days |
| Priority | P1 |
| Status | Done |

## Objective

Parse the approved file format into a validated complete engine-settings value
without applying side effects or partially changing a running engine.

## Scope

Typed settings, parsing, diagnostics, example file and schema documentation.
Startup/UCI application belongs to 043. No heavyweight dependency is needed
for the proposed INI subset; reconsider if 040 chooses a different format.

## Requirements

1. Define sections `[uci]`, `[evaluation]`, `[search]` or an approved equivalent.
   Use stable documented key names rather than relying on C++ member spellings.
2. Inventory every supported UCI option from the actual engine after task 039.
   Include Hash, Move Overhead, Threads (only supported values), OwnBook,
   BookFile and BookMaxPly. Unsupported advertised features are excluded.
3. Include task 041's entire parameter inventory and task 044's agreed tactical
   controls. Clearly distinguish file-only settings from UCI-exposed options.
4. Specify integer, boolean, string and fixed-size array grammar. Document
   UTF-8/BOM, blank lines, CRLF/LF, comments, whitespace and section/key casing.
   Paths with spaces, `#`, `;`, backslashes and `=` must be unambiguous; do not
   strip comment markers inside literal path values. Do not expand environment
   variables or interpret file contents as commands.
5. Unknown sections/keys, duplicate keys, malformed values, out-of-range numbers,
   overflow and wrong array sizes produce an error with path, line, key and
   reason. Do not silently ignore a typo or accept a truncated array.
6. Parse transactionally over defaults. Cross-field validation must reject
   inconsistent or numerically unsafe settings before resources are allocated.
7. Return errors using the repository's explicit error/result conventions;
   the parser itself must not write UCI output, resize hash, load books, or spawn
   threads. File loading/path resolution must remain separately testable.
8. Provide a commented `config/try4.ini` example matching current defaults.
   Do not create an automatically loaded root/executable-adjacent file during
   development or overwrite user files.

## Deliverables

- Configuration model, parser/load API and meaningful parser tests.
- `config/try4.ini` and `docs/CONFIGURATION.md` with schema, precedence,
  error examples, per-key ranges and copy/use instructions.
- New CMake test target/label consistent with repository patterns.

## Validation cases

Valid: empty/partial/complete files, comments, CRLF, optional BOM, paths with
spaces and punctuation, MG/EG values and exact-size arrays.

Invalid: unknown and duplicate keys, missing section/value, bad booleans,
trailing numeric garbage, overflow, excessive array entries, negative penalty
magnitudes, unsupported thread counts, malformed encoding according to the
chosen grammar, and unsafe combined weights. Each failure must be diagnostic
and leave the caller's prior settings unchanged.

## Acceptance criteria

- [x] Example parses and matches the built-in default settings exactly.
- [x] Partial files inherit defaults; all defined invalid cases fail atomically.
- [x] Schema covers UCI, evaluation and approved forcing controls.
- [x] Configuration tests and existing suite pass with warnings-as-errors.

Run the new config-labeled tests, evaluator tests and full CTest suite with
the build commands in `tasks/README.md`/the repository README.

## Completion report

Report schema/defaults, files, executed commands, parser test results and any
format trade-offs. Do not claim startup integration is finished in this task.

### Completion — 2026-10-26

**Status:** Done.

- Strict transactional UTF-8 INI parser with `[uci]`, `[evaluation]`, `[search]`;
  typed integers, booleans, strings and exact-size arrays; BOM/CRLF, whole-line
  comments, case-sensitive keys, duplicate/unknown-key rejection and
  file/line/key diagnostics.
- Paths preserve spaces, `#`, `;`, backslashes and `=`; no inline comments,
  environment expansion or command interpretation.
- Cross-field validation rejects unknown threads, unsafe arrays/weights,
  `forcing_max_plies > forcing_hard_plies` and numeric overflow before side
  effects.
- `config/try4.ini` exactly matches built-in defaults; partial files inherit
  defaults; errors leave caller settings unchanged.
- `docs/CONFIGURATION.md` records grammar, precedence, ranges, examples and the
  file-vs-UCI BookFile distinction.
- Five configuration Catch2 cases and the full 23-target suite pass in Debug,
  Release and sanitizers with warnings-as-errors.

Files: `include/chess/configuration.hpp`, `src/core/configuration.cpp`,
`config/try4.ini`, `docs/CONFIGURATION.md`, `tests/configuration_tests.cpp`,
`CMakeLists.txt`.
