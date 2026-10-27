# Task 001 — Initialize git and establish repository hygiene

| Field | Value |
|---|---|
| **ID** | 001 |
| **File** | `task_000000000000000000000000000001.md` |
| **Stage** | 0 — Baseline and guardrails |
| **Depends on** | — |
| **Estimate** | 0.5 day |
| **Priority** | P0 |
| **Status** | Done |

## Objective

Put the existing prototype under version control and exclude generated
artifacts so every later change has a reversible baseline.

## Context

The project is not currently a git repository. `build/`, `dist/`, and
`nbproject/private/` contain generated files. Source files, the
`Makefile`, `nbproject` project metadata, and `docs/` should be tracked.
Read `docs/ARCHITECTURE.md` §3 and `docs/ROADMAP.md` Stage 0.

## Scope

### In scope

- Initialize a git repository at the project root.
- Add `.gitignore` entries for build output, private IDE metadata, and binaries.
- Add `.gitattributes` for consistent line endings.
- Create a baseline commit containing source, build metadata, and docs.
- Verify that generated artifacts and private files are not tracked.

### Out of scope

- Changing source code or fixing warnings.
- Adding CMake, tests, CI, or README content.
- Rewriting history or deleting existing build artifacts.

## Requirements

1. Run `git init` at `C:\projects\nbp\try4` unless a repository already exists.
2. Ignore at minimum: `build/`, `dist/`, `nbproject/private/`, `*.o`, `*.obj`, `*.exe`, `*.d`, `*.pdb`, `*.ilk`, `.vscode/`, `.idea/`, and `cmake-build-*/`.
3. Track `board.*`, `piece.*`, `engine.*`, `defines.h`, `main.cpp`, `Makefile`, `nbproject/*.xml` and `nbproject/Makefile-*.mk` that are project metadata, and `docs/`.
4. Use `.gitattributes` with `* text=auto` and sensible binary rules if needed.
5. Create a baseline commit with a message such as `chore: baseline try4 prototype`.
6. Check `git status --short` and `git ls-files` before reporting completion.

## Deliverables

- `.gitignore`
- `.gitattributes`
- A baseline commit containing the current project and docs
- A short note listing any files intentionally left untracked

## Acceptance criteria

- [ ] A git repository exists at the project root.
- [ ] `git status --short` is clean after the baseline commit.
- [ ] `git ls-files` includes all source files and docs, but no files from `build/`, `dist/`, or `nbproject/private/`.
- [ ] The baseline commit exists and contains the current prototype without source modifications.

## Validation

```sh
git status --short
git log --oneline -1
git ls-files
```

Expected: clean status, one baseline commit, tracked source/docs, and no
generated build or private IDE files.

## Constraints and failure modes

- Do not modify source code in this task.
- Do not delete `build/` or `dist/`; they are simply ignored.
- If git user identity is missing, use a repository-local identity only after noting it in the report; do not change global git configuration.

## Completion notes

- Git repository initialized with baseline commit `ea20082`.
- `build/`, `dist/`, and `nbproject/private/` are intentionally untracked,
  together with object files, binaries, IDE state, and CMake output.
- Source, NetBeans project metadata, docs, and the task backlog are tracked.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
