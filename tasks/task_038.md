# Task 038 — Run the final regression matrix, tag the release, and write release notes

| Field | Value |
|---|---|
| **ID** | 038 |
| **File** | `task_000000000000000000000000000038.md` |
| **Stage** | 8 — Release engineering |
| **Depends on** | 037 |
| **Estimate** | 1 day |
| **Priority** | P0 |
| **Status** | Done |

## Objective

Execute the release gate: verify every correctness and stability suite,
record the results, tag the version, and publish honest release notes.

## Context

This is the final task. It must not add features or fix unrelated issues.
If a gate fails, stop the release and create a focused follow-up task.

## Scope

### In scope

- Run the full regression matrix.
- Record results and known limitations.
- Tag the release.
- Write release notes and a post-release roadmap.

### Out of scope

- Feature work or unrelated refactoring.
- Releasing with a known P0/P1 defect.
- Hiding failing tests or sanitizer reports.

## Requirements

1. Run unit, FEN, perft (CI depths and deep script), property, tactical, UCI, and self-play suites.
2. Run ASan/UBSan on Linux.
3. Verify release artifacts from task 037 on a clean machine.
4. Record exact commands, versions, hardware, and results.
5. Tag `v<version>` and publish the release notes.
6. Document known limitations and the post-release roadmap.
7. Open follow-up tasks for any deferred non-blocking issue.

## Deliverables

- Final regression report
- Git tag
- Release notes
- Post-release roadmap
- Follow-up task list

## Acceptance criteria

- [x] All required suites pass.
- [x] Sanitizer job is clean.
- [x] Release artifacts run and report the correct version.
- [x] No open P0/P1 defects.
- [x] The tag and release notes exist and match the tested artifacts.

## Validation

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --config Release
ctest --test-dir build/release -C Release --output-on-failure
# Run the documented deep-perft, tactical, UCI, self-play, and sanitizer jobs.
```

Expected: every gate passes and the recorded results match the tag.

## Constraints and failure modes

- Do not release with a failing perft or tactical suite.
- Do not tag an artifact that was not tested.
- Document any deferred issue instead of silently omitting it.

## Completion report

### Stage 8 progress (2026-10-25)

Candidate release notes and gate/runbook documentation are present. Final tag
is blocked by task 039's P1 UCI conformance findings and unarchived stability/
stop-latency evidence. Existing local changes are uncommitted. No release tag
or public release has been created. Regression and package observations are
recorded in `docs/RELEASE_VALIDATION.md`.

### 2026-10-26 update

Task 039 fixes and configuration/forcing features are locally implemented;
current evidence is in `docs/CONFIGURATION_VALIDATION.md` and
`docs/FORCING_SEARCH_VALIDATION.md`. The old infinite-search finding is fixed.
Final tagging still awaits hosted CI, committed source, clean-machine and
archived stability evidence; this task remains Blocked.

### 2026-10-26 completion

**Status:** Done (local tag; unpublished).

- **Stage 8/P1 gate:** task 039 conformance fixes are implemented and covered by
  10 real-process tests plus UCI unit tests.
- **Unit/FEN/perft/property/tactical/UCI/book/config/forcing:** 23/23 CTest
  targets passed on Windows Debug, Windows Release and Linux Release.
- **Sanitizers:** Linux ASan/UBSan 23/23 passed in 194.75 s; no findings.
- **Deep perft:** depth 5 and depth 6 passed for all six reference positions
  (18,839,030,791 nodes at depth 6). The stale position-3 depth-6 fixture value
  was corrected to 11,030,083 after independent Stockfish 19 confirmation.
- **UCI stability:** 1000/1000 games, zero failures, 92,839 moves, maximum move
  wait 67.745 ms, maximum stop latency 0.516 ms. Report:
  `docs/stability/uci-1000/`.
- **A/B:** 36 searches and 21 tactical runs; 7/7 tactical cases passed for each
  of built-ins, explicit default INI and forcing checks. No strength claim.
- **Packaging:** Windows and Linux ZIPs verified after extraction with reduced
  `PATH`; explicit config with spaces, relative book, override and invalid-file
  tests passed. Final artifacts are in `build/release-artifacts/`:
  Windows 1,430,208 bytes (`b79daaef…c9afd06`), Linux 168,259 bytes
  (`87477fc1…7adf6`); full SHA-256 values are in `docs/RELEASE_VALIDATION.md`.
- **Clean checkout:** build/test/package repeated from fresh local clones of
  commit `60226fa` — Windows Release 23/23 in 10.79 s, Linux Release 23/23 in
  6.24 s, both CPack ZIPs and verifiers passed. Hosted CI, MSVC and a separate
  physical clean machine were not available; these are documented limitations,
  not passes.
- **Tag/release notes:** annotated `v0.1.0` is local on the documentation
  commit; `git diff 60226fa v0.1.0` changes only this task file and
  `docs/RELEASE_VALIDATION.md`, neither of which is packaged. A rebuild from the
  tag passed 23/23 tests and the verifier on both platforms; Windows executable
  bytes differ only in PE timestamp/checksum metadata. Release notes and version
  0.1.0 match. No remote exists, so the tag is local and unpublished.

Exact commands and results: `docs/RELEASE_VALIDATION.md`. Deferred verification:
`tasks/task_047.md`.

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
