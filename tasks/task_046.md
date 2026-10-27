# Task 046 — Validate and document configurable evaluation and forcing search

| Field | Value |
|---|---|
| ID | 046 |
| Stage | Feature acceptance and release regression |
| Depends on | 041, 042, 043, 044, 045 |
| Estimate | 1–2 days plus match runtime |
| Priority | P1 |
| Status | Done |

## Objective

Verify both owner-requested features as a combined increment using reproducible
configuration, tactical reference tests, clock/stop checks and honest A/B data.
Update release evidence without bypassing tasks 032, 038 or 039.

## Requirements and deliverables

1. Extend the self-play/benchmark entry point to accept the same explicit config
   snapshot if required for A/B measurement. Do not silently change material/full
   baseline meaning. Record effective settings or a normalized snapshot plus hash,
   engine version, build, openings, limits, seed and hardware in result reports.
2. Compare built-in defaults, explicit default file, and a controlled changed
   configuration on the same FEN/opening set. Default-file behavior must match
   defaults; changed settings must have an observable, expected effect.
3. Compare legacy forcing-disabled and new forcing-enabled policies at fixed
   depth/node limits and fixed time. Record tactical pass rate, node counts,
   elapsed time and stop latency. Higher node counts are not evidence of stronger
   play; strength claims require enough recorded paired games and uncertainty.
4. Run full Debug/Release CTest on Windows and Linux, then Linux ASan/UBSan.
   Update `tests/README.md`, configuration/forcing docs and the changelog.
5. Exercise packaged executables from a different working directory with an
   explicit config path containing spaces, relative BookFile resolution, invalid
   file startup, and a GUI-style option override sequence.
6. Write `docs/CONFIGURATION_VALIDATION.md` and `docs/FORCING_SEARCH_VALIDATION.md`
   with commands, exact fixtures/settings, observed results and failures.
   Separate measurements from assumptions and unverified outcomes.
7. A fresh build's archive must include the non-active example and schema docs.
   Rebuild/reverify checksums and record the tested source revision. Existing
   Stage 8 archives cannot stand in for the changed search implementation.

## Acceptance matrix

| Case | Expected result |
|---|---|
| No config vs explicit default config | Equivalent evaluator/search results at deterministic limits |
| Changed MG/EG and positional parameters | Predicted targeted score/breakdown effect |
| INI typo/bad array/missing explicit file | Diagnostic startup failure, no partial settings |
| Later UCI override + ucinewgame | Override persists; no race or stale TT score |
| Checks/captures/quiet evasions corpus | Verified coverage and reference scores at stated budgets |
| Cycle/budget exhaustion/stop | Bounded work; correct incomplete-result handling and legal move |
| Tight clock after book miss | Existing clock tolerances hold across forcing search |
| Packaged engine + config outside cwd | Same behavior as source-tree execution |
| All regression/sanitizer suites | Pass; failing gate stays visible |

## Validation commands

```sh
cmake -S . -B build/feature-check -DCMAKE_BUILD_TYPE=Release -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/feature-check --config Release
ctest --test-dir build/feature-check -C Release --output-on-failure --output-junit regression.xml
cpack --config build/feature-check/CPackConfig.cmake -C Release
```

Use `scripts/verify_release.py` for each archive after extending it with the
new explicit-configuration smoke scenario. Run the sanitizer recipe in
`docs/RELEASE.md`; include observed duration and findings in the report.

## Completion report and failure handling

Return status, changed files, commands/results, acceptance matrix, benchmark
comparisons and deferred issues. Stop release advancement if a required check
fails. No public tag or assertion of playing-strength improvement is justified
by these task documents or by a few successful games.

## Completion — 2026-10-26

**Status:** Done.

| Acceptance case | Result |
|---|---|
| No config vs explicit default config | Fixed-depth bestmove/score/depth/nodes/PV identical on four positions |
| Changed MG/EG/positional parameters | +100 pawn value changed the real UCI score by exactly +100; PST scales parsed and bounded |
| INI typo/bad array/missing explicit file | Nonzero startup, stderr diagnostic, empty stdout, no partial settings |
| UCI override + `ucinewgame` | Override persisted; no race; TT invalidated on policy changes |
| Checks/captures/quiet evasions corpus | Six forcing cases / 3,414 assertions; independent oracle and candidate-class checks |
| Cycle/budget exhaustion/stop | Bounded abort, legal fallback, last completed result preserved; no unbounded recursion |
| Tight clock after book miss | Legal move returned under the process timeout |
| Packaged engine + config outside cwd | Windows/Linux archives passed `verify_release.py` |
| All regression/sanitizer suites | 23/23 on Windows Debug/Release, Linux Release, Linux ASan/UBSan |
| 1,000-game stability | 1000/1000, zero protocol failures |
| Deep perft | Depth 5 all six; depth 6 all six (see `RELEASE_VALIDATION.md`) |

- Benchmark: 36 searches, 21 tactical runs, 7/7 per configuration. Forcing found
  the verified Légal mate; no strength claim.
- Documentation: `CONFIGURATION_VALIDATION.md`, `FORCING_SEARCH_VALIDATION.md`,
  `tests/README.md`, changelog and release reports updated.
- Packaging/checksums and the local `v0.1.0` tag are recorded in
  `docs/RELEASE_VALIDATION.md`. Hosted CI, MSVC and a separate physical clean
  machine remain deferred to task 047.

Files: `scripts/benchmark_forcing.py`, `scripts/uci_stability.py`,
`scripts/verify_release.py`, `tests/engine_process_tests.py`,
`tests/forcing_tests.cpp`, `docs/CONFIGURATION_VALIDATION.md`,
`docs/FORCING_SEARCH_VALIDATION.md`, `docs/RELEASE_VALIDATION.md`,
`tests/README.md`, `CHANGELOG.md`.
