# Task 021 — Expand fuzzing and add sanitizer CI

| Field | Value |
|---|---|
| **ID** | 021 |
| **File** | `task_000000000000000000000000000021.md` |
| **Stage** | 3 — Complete game state and hardening |
| **Depends on** | 018, 020 |
| **Estimate** | 1.5 days |
| **Priority** | P1 |
| **Status** | Done |

## Objective

Harden the core against malformed input and long randomized sessions, and
run AddressSanitizer/UndefinedBehaviorSanitizer in Linux CI.

## Context

Task 018 covers 10k random legal games. This task expands to the
architecture target of 100k games, fuzzes FEN parsing, and adds sanitizer
coverage. MinGW lacks first-class ASan support; use Ubuntu CI.

## Scope

### In scope

- Add FEN parser fuzz cases with invalid and adversarial strings.
- Expand random playouts to 100k games or a documented time-boxed equivalent.
- Add an ASan/UBSan CI job on Ubuntu.
- Fix all findings and add regression tests.

### Out of scope

- Search fuzzing.
- UCI protocol fuzzing.
- Performance optimization.

## Requirements

1. Fuzz malformed FEN: empty input, long input, invalid UTF-8 bytes where applicable, missing fields, extra fields, bad digits, and impossible ranks.
2. Ensure the parser fails gracefully without crashes, out-of-bounds access, or partial state mutation.
3. Run 100,000 random legal games, or a time-boxed run that still covers all move types; document any reduction and why.
4. Verify make/unmake, FEN, hash, and result invariants throughout.
5. Add a Linux CI job configured with `-fsanitize=address,undefined -fno-omit-frame-pointer`.
6. Keep sanitizer suppressions empty unless a finding is proven to be a third-party issue; document any exception.

## Deliverables

- FEN fuzz tests
- Expanded randomized suite
- Sanitizer CI job
- A short findings/fixes log

## Acceptance criteria

- [ ] Malformed FEN never crashes or corrupts state.
- [ ] The expanded random suite completes without invariant failures.
- [ ] ASan/UBSan CI is clean.
- [ ] Any sanitizer fix has a regression test.
- [ ] No existing test regresses.

## Validation

```sh
# On Linux/WSL:
cmake -S . -B build/asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build/asan
ctest --test-dir build/asan --output-on-failure
```

Expected: sanitizer build and tests pass. On Windows, rely on the Ubuntu
CI job and document that MinGW sanitizers are unavailable.

## Constraints and failure modes

- Do not ignore sanitizer reports.
- Do not suppress findings without a written justification.
- Do not make CI so slow that it is routinely skipped; use a documented time box.

## Completion notes

- Added `tests/fuzz_tests.cpp` with adversarial malformed FEN cases, a fixed
  10,000-byte input, high-bit/invalid bytes, missing/extra fields, bad digits,
  impossible ranks, invalid clocks, and a 2,000-string deterministic corpus.
  Rejections must set an error, leave an existing position untouched, and never
  crash. Accepted strings are round-tripped and checked against the hash oracle.
- Made the property workload configurable via `TRY4_PROPERTY_GAMES` and
  `TRY4_PROPERTY_MAX_PLIES` while keeping the fast default at 10,000 games and
  16 plies. A 100,000-game Debug run completed in 113.9 s with 6,697,265
  assertions and verified FEN, board, state, history, hash, result, and undo
  invariants throughout.
- Added an Ubuntu ASan/UBSan CI job with leak detection, frame pointers, and no
  suppressions. The full 13-test suite passed locally under the same flags in
  135 s (GCC 15.2.0, Ubuntu WSL2); all findings were clean.
- Documented the workload, extended-run command, results, and sanitizer policy
  in `docs/HARDENING.md`; updated README and test documentation.

## Completion report

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
