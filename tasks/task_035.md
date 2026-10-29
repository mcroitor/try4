# Task 035 — Add optional opening book, Syzygy tablebases, and Lazy SMP

| Field | Value |
|---|---|
| **ID** | 035 |
| **File** | `task_000000000000000000000000000035.md` |
| **Stage** | 7 — Performance and strength (optional) |
| **Depends on** | 034 |
| **Estimate** | 2 days |
| **Priority** | P2 |
| **Status** | Done |

**Split outcome.**
- **Opening book (Polyglot `.bin` + the text format):** shipped earlier,
  behind `OwnBook`/`BookFile`/`BookMaxPly` (see `docs/UCI.md`); load failures
  fall back to search with `info string Book load failed:`, which is the
  "fails gracefully when data is missing" contract.
- **Lazy SMP:** superseded by task 048's PoC (`build/smp-on`); the
  go/no-go and the follow-up live in `docs/SMP_POC.md`.
- **Syzygy tablebases: not pursued.** No tablebase assets are available on
  this machine or repository, so a probing implementation could not be
  tested at all; the smallest unblock action is obtaining a small set of
  3-4-5-piece `.rtbw`/`.rtbz` files. Probing, licensing and the
  fifty-move-rule integration remain requirements for that follow-up.

## Objective

Provide optional strength features behind explicit options, disabled by
default, without destabilizing the single-threaded engine.

## Context

Optional Stage 7 work. Treat each feature as a separate sub-task if it is
pursued: Polyglot opening book, Syzygy endgame tablebase probing, and Lazy
SMP multithreading. Do not enable any by default in v1.

## Scope

### In scope

- Add Polyglot opening-book support behind an option.
- Add Syzygy tablebase probing behind an option.
- Add Lazy SMP multithreading behind an option after single-thread stability.
- Document requirements, limitations, and failure behavior.

### Out of scope

- Downloading or bundling book/tablebase data without a licensing decision.
- Making any optional feature mandatory for the default build.
- Changing the evaluation to NNUE.

## Requirements

1. Each feature must be independently enableable and disableable; the default build must remain single-threaded and data-free.
2. Missing book/tablebase files must produce a clear warning, not a crash.
3. Syzygy probing must respect the fifty-move rule and tablebase limits.
4. Lazy SMP must not introduce data races; run thread sanitizer or equivalent tests where available.
5. Run 1,000-game stability and tactical/perft regressions for each enabled feature.
6. Document data licensing and external file layout.

## Deliverables

- Optional feature implementations
- Configuration/documentation
- Stability and regression reports

## Acceptance criteria

- [ ] Default build behavior is unchanged.
- [ ] Each optional feature works when enabled and fails gracefully when its data is missing.
- [ ] No correctness or stability regression.
- [ ] Documentation covers setup and limitations.

## Validation

```sh
cmake --build build/cmake --config Release
ctest --test-dir build/cmake -C Release --output-on-failure
# Run the documented feature-specific smoke and stability tests.
```

Expected: default build/tests pass; optional features pass their smoke
tests when enabled.

## Constraints and failure modes

- Do not bundle third-party data without checking its license.
- Do not enable optional features by default.
- Do not mix SMP work with rule or evaluation changes.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
