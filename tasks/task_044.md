# Task 044 — Search bounded forcing continuations including quiet checks

| Field | Value |
|---|---|
| ID | 044 |
| Stage | Tactical-search enhancement |
| Depends on | 040 |
| Estimate | 2–3 days |
| Priority | P1 |
| Status | Done |

## Objective

Extend the existing tactical horizon search to cover the approved forcing-line
contract: legal checks, captures, promotions and every legal reply to check,
while preserving sound result, termination and stop behavior.

## Scope and constraints

Modify search, typed search settings and tactical regressions. Reuse current
move generation and make/unmake. No bitboard rewrite, unrelated pruning, or
new strength feature. Main-search extensions are conditional on 040 approval;
if chosen, document their separate per-line budget and TT-depth semantics here
before implementation. Do not quietly equate horizon quiescence with extensions
throughout the principal search.

## Requirements

1. For a non-check tactical node include every legal capture (including en
   passant), every promotion/underpromotion, and every legal checking move,
   including quiet/discovered/castling checks. Deduplicate overlaps.
2. In check, consider all legal evasions, including quiet blocks, king moves,
   and moves creating a counter-check. Never use stand-pat in check.
3. Define how stand-pat outside check and alpha-beta/TT cutoffs interact with
   checking continuations. Pruning must not falsely claim the entire tactical
   tree was enumerated. No SEE/delta/futility/late-move filtering that discards
   required candidates without an explicitly approved and tested policy.
4. Replace the magic eight-ply cap with a validated `ForcingSearchSettings`
   contract: enable checks, tactical budget, absolute safety limit, and approved
   defaults. Document whether captures/promotions share the budget and how
   check evasions consume it. Defaults preserve legacy behavior where the new
   feature is disabled, enabling controlled A/B comparisons.
5. At the nominal budget, do not statically evaluate a checked node as a quiet
   leaf. Continue necessary evasions within a hard safety budget; if the hard
   bound is hit unresolved, abort that iteration (or use 040's approved explicit
   incomplete-result policy). Do not store incomplete scores as exact TT entries.
6. Honor existing clock/node/atomic-stop limits at every tactical recursion.
   Enforce an absolute ply bound before recursion/array access. Repetition,
   fifty-move and material draws, checkmate and stalemate retain current rules.
7. Make/unmake restores board, rights, en passant, clocks, history and hash on
   all paths, including timeout, cutoff and abort. Preserve a legal root fallback
   and the last completed iterative-deepening result when interrupted.
8. Include forcing policy/budget in TT validity semantics or clear relevant
   entries when it changes. A TT entry produced by a shallower/different tactical
   policy must not substitute an incompatible exact result.
9. Provide a small, independent bounded reference search or equivalent oracle
   for selected low-branching positions. Compare scores with alpha-beta/ordering/
   TT enabled versus disabled; node-count equality is not required.

## Tactical test matrix

| Scenario | Observable expected result |
|---|---|
| Only winning continuation starts with a quiet check beyond horizon | Approved forcing search finds the independently verified line |
| Checking sacrifice followed by capture/recapture | Best move/score agrees with reference at the stated budget |
| Quiet/discovered check, checking castle | Move belongs to the candidate set if legal |
| Promotion and en-passant check/capture | Included once with correct flags |
| Check with only a quiet block/king evasion | Evasion is searched; no false mate/stand-pat |
| Perpetual checking cycle | Draw/termination rules hold; no unbounded recursion |
| Checked node exactly at nominal/hard limits | Approved continuation/abort policy, no false exact score |
| Tempting check that loses to a legal defense | Losing line is refuted, not overvalued |
| Stop or tiny clock during a combination | Prompt completion, legal fallback, exact position restoration |
| Policy change while reusing TT | Same score as a fresh searcher under that policy |

Every tactical fixture needs its FEN, limit settings, expected move/score or
mate bound and an independent rationale. Include losing/refuted combinations;
mate-in-one tests alone cannot establish this feature's correctness.

## Deliverables and acceptance

- [x] Bounded forcing search, settings contract and TT policy implemented.
- [x] Tests establish candidate coverage and score correctness, not merely that
      more nodes were visited or the chosen move looks tactical.
- [x] Default/disabled compatibility and existing tactical tests pass.
- [x] No time, node, recursion, position-restoration or mate/draw regression.
- [x] `docs/FORCING_SEARCH.md` explains algorithm, pruning and completeness limits.

Build Debug/Release, run search/tactical/time/property labels and the full suite.
Task 046 performs cross-feature process tests, sanitizers and performance A/B.

## Failure modes and completion report

Watch for check explosion, missed quiet defenses, double-applied checks, false
mate-band values, stale TT scores and in-check static leaves. Report independent
oracle comparisons, solved/refuted fixtures, latency/node measurements and
remaining scope decisions. Never report "all combinations verified" for an
interrupted or budget-limited search.

### Completion — 2026-10-26

**Status:** Done.

- Candidates at a non-check horizon: all legal captures (including en passant),
  all promotions/underpromotions, and, when enabled, every legal quiet/discovered
  checking move and checking castle. In check: all legal evasions, no stand-pat.
- Contract: `forcing_checks`, `forcing_max_plies` (soft), `forcing_hard_plies`
  and absolute ply bound. Checks disabled by default preserve legacy behavior.
- Checked nodes are never statically evaluated as quiet leaves. At the hard
  bound the iteration aborts without an exact score; a legal fallback and the
  last completed result are preserved. Incomplete scores are not stored as exact
  TT entries.
- Time/node/stop/repetition/draw/absolute-ply limits apply across main and
  forcing search; make/unmake restoration is tested on all paths.
- Six forcing cases and 3,414 assertions include an independent exhaustive
  minimax oracle, Légal's mate, candidate classes, a checked quiet block,
  hard-bound abort, cancellation and node limits.
- Final full suites pass on Windows Debug/Release and Linux Release; sanitizers
  are clean. The final fixture set also fixed an illegal promotion-check FEN.
- Measured cost: Kiwipete depth 1 about 172 ms with checks versus 28 ms without;
  no speed or strength claim.
- `docs/FORCING_SEARCH.md` and `docs/FORCING_SEARCH_VALIDATION.md` document the
  algorithm, pruning and completeness limits.

Files: `include/chess/search.hpp`, `src/core/search.cpp`,
`tests/forcing_tests.cpp`, `tests/engine_process_tests.py`,
`scripts/benchmark_forcing.py`, `docs/FORCING_SEARCH.md`,
`docs/FORCING_SEARCH_VALIDATION.md`.
