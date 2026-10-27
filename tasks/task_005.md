# Task 005 — Write the README and developer documentation

| Field | Value |
|---|---|
| **ID** | 005 |
| **File** | `task_000000000000000000000000000005.md` |
| **Stage** | 0 — Baseline and guardrails |
| **Depends on** | 002, 003 |
| **Estimate** | 0.5 day |
| **Priority** | P1 |
| **Status** | Done |

## Objective

Give a new contributor one reliable path from a clean checkout to a
running build and passing tests, while stating the project's real
limitations.

## Context

The repository has architecture and roadmap documents but no README.
The project is still a prototype: it prints a board and supports a naive
demo move, not a playable engine.

## Scope

### In scope

- Add a top-level `README.md`.
- Document prerequisites, CMake configure/build/run/test commands, and the optional NetBeans path.
- Document the current status and limitations honestly.
- Link to `docs/ARCHITECTURE.md`, `docs/ROADMAP.md`, and the task backlog.
- Describe the task execution workflow and the definition of done.

### Out of scope

- Claiming that the engine is playable or supports UCI.
- Selecting a license; that decision belongs to task 036.
- Duplicating the full architecture or roadmap in the README.

## Requirements

1. Include a short project overview and the current maturity level.
2. List prerequisites: CMake 3.20+, a C++17 compiler, and optionally NetBeans.
3. Show the canonical commands using `build/cmake`.
4. Explain that `nbproject/Makefile-*.mk` is generated and must not be hand-edited.
5. List known limitations: no legal move generation, no FEN, no search, no UCI.
6. Link to architecture, roadmap, and task index documents.
7. Explain that each task must be completed against its acceptance criteria and validation commands.
8. Keep the document concise and free of unverified claims.

## Deliverables

- `README.md`
- A short docs index or links section
- A verified build/run/test walkthrough

## Acceptance criteria

- [ ] A new contributor can build, run, and test from the README alone.
- [ ] All commands in the README work on a clean checkout.
- [ ] The limitations section accurately reflects the current code.
- [ ] All relative links resolve.

## Validation

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build/cmake --config Debug
ctest --test-dir build/cmake -C Debug --output-on-failure
```

Expected: the README commands reproduce a successful build and test run.

## Constraints and failure modes

- Do not claim features that are not implemented.
- Do not choose a license in this task.
- Prefer links to the existing docs over duplicating them.

## Completion notes

- Top-level `README.md` documents prerequisites, build/run/test commands,
  NetBeans rules, current limitations, and the task workflow.
- The documented commands were validated locally from the repository root.
- A clean-clone build is validated after the Stage 0 commit.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
