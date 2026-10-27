# Task 004 — Fix compiler warnings and header hygiene

| Field | Value |
|---|---|
| **ID** | 004 |
| **File** | `task_000000000000000000000000000004.md` |
| **Stage** | 0 — Baseline and guardrails |
| **Depends on** | 002, 003 |
| **Estimate** | 0.5 day |
| **Priority** | P0 |
| **Status** | Done |

## Objective

Remove the warning-level issues in the current code without changing
chess behavior, so later refactors start from a clean build.

## Context

A C++17 build with `-Wall -Wextra -Wpedantic` reports ignored `const`
qualifiers, member initialization order, an unhandled `NONE` switch case,
unused parameters, and a missing direct `<string>` include. See
`docs/ARCHITECTURE.md` findings F-09 through F-15.

## Scope

### In scope

- Fix all reported warnings in the current sources.
- Remove the unused `is_white_move` member or document why it remains.
- Add direct includes for types used by each header.
- Make `Piece::ToString()` handle `piece_t::NONE` explicitly.
- Keep the public behavior and demo output unchanged.

### Out of scope

- Replacing `Board`/`Engine` with `Position`; that is Stage 1 work.
- Adding new chess features.
- Changing the NetBeans build.

## Requirements

1. Remove `const` from by-value return types such as `Piece::Color()`, `Piece::Type()`, and `Piece::ToString()`.
2. Match `Piece` member declaration order in both constructors.
3. Include `<string>` in `piece.h` and any other header that uses `std::string` directly.
4. Handle `piece_t::NONE` in `Piece::ToString()` and keep the empty-square output as a single space.
5. Use `static_cast<unsigned char>` before `std::tolower`.
6. Mark or remove unused `main` parameters and the unused `SetBoard` parameter without breaking the API.
7. Remove the dead `is_white_move` member if it has no readers; otherwise add a TODO explaining the future owner.
8. Build with `-DTRY4_WARNINGS_AS_ERRORS=ON` and run the full test suite.

## Deliverables

- A warning-clean build
- Updated source and headers
- A regression test confirming `Piece::ToString()` for the empty piece

## Acceptance criteria

- [ ] The project builds with `TRY4_WARNINGS_AS_ERRORS=ON`.
- [ ] All characterization tests pass.
- [ ] The initial board and quiet move/undo output are unchanged.
- [ ] No new warnings are suppressed with pragmas.

## Validation

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug --output-on-failure
```

Expected: no warnings and all tests pass.

## Constraints and failure modes

- Do not refactor to the new `Position` API in this task.
- Do not change chess behavior, including the empty-piece rendering.
- If a warning fix would require an API change, report it as a follow-up instead of forcing it.

## Completion notes

- The project builds with `-Wall -Wextra -Wpedantic -Werror` and all existing
  tests pass.
- Demo output remains identical to the baseline.
- The legacy `is_white_move` member and unused parameters were removed or
  unnamed without changing the public API.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
