# Task 036 — Add versioning, changelog, and license metadata

| Field | Value |
|---|---|
| **ID** | 036 |
| **File** | `task_000000000000000000000000000036.md` |
| **Stage** | 8 — Release engineering |
| **Depends on** | 032 |
| **Estimate** | 0.5 day |
| **Priority** | P1 |
| **Status** | Done |

## Objective

Give releases a stable identity and make the project's licensing and change
history explicit.

## Context

The engine is playable and documented. This task adds a version string,
changelog, and license. The license choice is a project-owner decision and
must not be guessed.

## Scope

### In scope

- Define a semantic version and expose it in the build and UCI id.
- Add `CHANGELOG.md` following a recognized format.
- Add a license approved by the project owner.
- Update README metadata.

### Out of scope

- Packaging/release artifacts; task 037.
- Changing engine behavior.
- Choosing a license without approval.

## Requirements

1. Expose `TRY4_VERSION` (or equivalent) to the build and `--version`/UCI `id`.
2. Use semantic versioning and start at `0.x` or `1.0.0` according to readiness.
3. Add a changelog with an Unreleased section and the initial release entry.
4. Ask the user which license to use if none is specified; do not select one silently.
5. Add the license file and reference it in the README.

## Deliverables

- Version definition
- `CHANGELOG.md`
- `LICENSE` (user-approved)
- Updated README metadata

## Acceptance criteria

- [x] `try4-uci` reports the version in `id` (`try4_uci` is the actual target name).
- [x] `--version` prints the same version.
- [x] The changelog and license are present and linked from the README.
- [x] The Windows and Linux local builds pass.

## Validation

```sh
cmake --build build/cmake --config Release
printf 'uci\nquit\n' | ./build/cmake/Release/try4-uci
```

Expected: the UCI `id` includes the release version.

## Constraints and failure modes

- Do not choose a license without user approval.
- Do not claim a version is stable if Stage 6/8 gates are incomplete.
- Do not rename the project without checking downstream references.

## Completion report

### Stage 8 progress (2026-10-25)

- Owner selected MIT; `LICENSE`, notices, and `CONTRIBUTING.md` added.
- Candidate semantic version 0.1.0 defined once in CMake, exposed via generated
  `chess/version.hpp`, both app `--version` commands and UCI `id name`.
- Changelog, UCI usage and candidate release notes added.
- Validation results are recorded in `docs/RELEASE_VALIDATION.md`.
- Dependency 032's archived 1,000-game report remains unverified; owner reports
  stable CuteChess play. Stage completion awaits the final gates, not code alone.

### 2026-10-26 completion

**Status:** Done.

- MIT license, notices and contribution instructions are present.
- Version 0.1.0 remains defined only in CMake and is exposed consistently by
  `try4 --version`, `try4_uci --version` and UCI `id name` on Windows
  Debug/Release and Linux Release/sanitizer builds.
- `CHANGELOG.md` records the 0.1.0 contents, fixes and limitations.
- 032's archived 1,000-game report now exists at
  `docs/stability/uci-1000/`.
- The final gates, artifact checksums and local tag are recorded in
  `docs/RELEASE_VALIDATION.md`. Hosted CI/MSVC were unavailable and remain
  documented limitations.

Files: `CMakeLists.txt`, `cmake/version.hpp.in`, `CHANGELOG.md`, `LICENSE`,
`THIRD_PARTY_NOTICES.md`, `CONTRIBUTING.md`, `README.md`,
`docs/RELEASE_NOTES.md`.

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
