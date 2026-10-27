# Task 002 — Add a canonical CMake build with C++17 and warning flags

| Field | Value |
|---|---|
| **ID** | 002 |
| **File** | `task_000000000000000000000000000002.md` |
| **Stage** | 0 — Baseline and guardrails |
| **Depends on** | 001 |
| **Estimate** | 1 day |
| **Priority** | P0 |
| **Status** | Done |

## Objective

Provide a reproducible CMake build that compiles the current sources as
C++17, separates the core from the executable, and exposes a warnings
option for CI.

## Context

The current build uses NetBeans-generated makefiles and does not specify a
C++ standard or warning flags. The toolchain is g++ 16.2 on Windows. The
NetBeans project should continue to work, but CMake becomes the canonical
build for tests and CI.

## Scope

### In scope

- Add a top-level `CMakeLists.txt`.
- Build a static library target `try4_core` from the non-main sources.
- Build an executable target `try4` from `main.cpp`, linked to `try4_core`.
- Configure C++17 with compiler extensions disabled.
- Add a warnings interface with `-Wall -Wextra -Wpedantic` for GCC/Clang and `/W4` for MSVC.
- Add `TRY4_WARNINGS_AS_ERRORS` (default `OFF`) for later CI use.
- Use `build/cmake` as the documented build directory and keep it ignored.

### Out of scope

- Changing source behavior or fixing warnings.
- Adding tests or test frameworks.
- Editing generated NetBeans makefiles.
- Removing the existing `Makefile` or `nbproject` project.

## Requirements

1. Require CMake 3.20 or newer and `LANGUAGES CXX`.
2. Set `CMAKE_CXX_STANDARD 17`, `CMAKE_CXX_STANDARD_REQUIRED ON`, and `CMAKE_CXX_EXTENSIONS OFF`.
3. Set `CMAKE_EXPORT_COMPILE_COMMANDS ON` for editor tooling.
4. Define `try4_core` from `board.cpp`, `piece.cpp`, and `engine.cpp`; define `try4` from `main.cpp`.
5. Apply warning flags only to project targets, not to third-party code added later.
6. Keep the Debug/Release configuration working on single- and multi-config generators.
7. Document the configure, build, and run commands in the task report; README updates are handled by task 005.

## Deliverables

- `CMakeLists.txt`
- A successful Debug and Release configure/build
- A short note confirming the NetBeans build was left intact

## Acceptance criteria

- [ ] `cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug` succeeds.
- [ ] `cmake --build build/cmake --config Debug` succeeds.
- [ ] The `try4` executable runs and prints the same initial board and demo sequence as before.
- [ ] The NetBeans project files are unchanged.
- [ ] `TRY4_WARNINGS_AS_ERRORS=ON` is available but not required to pass yet.

## Validation

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug --output-on-failure
```

Expected: configure and build succeed; `ctest` may report no tests until
task 003. Run the produced `try4` executable and compare its output with
the baseline recorded in task 001.

## Constraints and failure modes

- Do not edit `nbproject/Makefile-*.mk` or other generated files.
- Do not add external dependencies in this task.
- If the CMake build differs from the NetBeans build, report the difference rather than silently changing behavior.

## Completion notes

- CMake 3.20+ project with C++17, extensions disabled, and `try4_core` /
  `try4` targets.
- Debug and Release builds both succeed with Ninja and GCC 16.2.
- The demo output is identical to the pre-CMake baseline.
- `TRY4_WARNINGS_AS_ERRORS` is available and defaulted to `OFF`.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
