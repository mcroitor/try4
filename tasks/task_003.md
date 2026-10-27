# Task 003 — Add Catch2/CTest and characterization tests, then wire CI

| Field | Value |
|---|---|
| **ID** | 003 |
| **File** | `task_000000000000000000000000000003.md` |
| **Stage** | 0 — Baseline and guardrails |
| **Depends on** | 002 |
| **Estimate** | 1.5 days |
| **Priority** | P0 |
| **Status** | Done |

## Objective

Establish an automated safety net around the current prototype and make
it run in CI on Windows and Linux.

## Context

There are no tests today. Characterization tests must pin the existing
behavior before the core is refactored. The build is CMake-based after
task 002.

## Scope

### In scope

- Add Catch2 v3 through `FetchContent` or vendor a test framework with an offline fallback.
- Add a CMake option `TRY4_BUILD_TESTS` (default `ON`) and register tests with CTest.
- Add tests for `Piece`, `Board`, `Engine`, and the current demo behavior.
- Add GitHub Actions workflows for Ubuntu and Windows.
- Run configure, build, and `ctest` in CI.

### Out of scope

- Refactoring production APIs.
- Testing features that do not exist yet, such as legal move generation or FEN.
- Adding performance benchmarks.

## Requirements

1. Test the default `Piece`: empty, `Color() == NONE`, `Type() == NONE`, and `ToString() == " "`.
2. Test white and black piece rendering and `operator==`.
3. Test initial `Board::Init()` placement, `SetPiece`, `GetPiece`, and the initial ASCII board.
4. Test a quiet move and undo on the demo position, including the restored board string.
5. Keep tests deterministic and independent of working directory.
6. Add `ctest` labels such as `unit` for fast tests.
7. CI must use a clean checkout, configure with CMake, build, and run `ctest --output-on-failure`.
8. If `FetchContent` requires network access, document the fallback; tests must not require network at runtime.

## Deliverables

- A test target with Catch2 and CTest
- Characterization tests for the current behavior
- `.github/workflows/ci.yml` (or equivalent) for Ubuntu and Windows
- A note recording the baseline demo output used by the tests

## Acceptance criteria

- [ ] `ctest --test-dir build/cmake -C Debug --output-on-failure` passes locally.
- [ ] The test suite fails if the initial board string or quiet move/undo behavior changes.
- [ ] CI is green on both Windows and Ubuntu.
- [ ] No production source files are modified except for minimal build integration.

## Validation

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug -DTRY4_BUILD_TESTS=ON
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug --output-on-failure
```

Expected: all characterization tests pass.

## Constraints and failure modes

- Do not write tests against future APIs such as `Position` or `MoveFlag`.
- Do not delete the existing demo; tests should capture its behavior.
- Keep CI runtime under roughly 10 minutes.

## Completion notes

- Catch2 v3.5.2 is fetched at configure time; the characterization suite has
  six test cases and passes in Debug and Release.
- The GitHub Actions workflow for Ubuntu and Windows is present under
  `.github/workflows/ci.yml`.
- CI execution has not been verified because this repository has no remote
  yet; it must be checked after the first push.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
