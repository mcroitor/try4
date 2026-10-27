# Task 040 — Agree configuration and forcing-search behavior

| Field | Value |
|---|---|
| ID | 040 |
| Stage | Engine configuration and tactical-search enhancement |
| Depends on | Existing evaluator/search/UCI implementation |
| Estimate | 0.5 day |
| Priority | P1 |
| Status | Done |

## Objective

Turn the owner's two requested changes into an explicit implementation contract:
one configuration file for UCI options and evaluation parameters, and search of
forcing continuations comprising checks and captures. This is requirements work,
not permission to implement an unlimited exhaustive tree search.

## Source requirements and current behavior

### Owner decisions approved — 2026-10-26

The owner selected values and MG/EG PST scales (not table-cell overrides),
horizon forcing search (not main-search extensions), and the proposed INI
startup discovery/precedence/error behavior. The finite soft/hard budget and
abort-on-unresolved-checked-hard-limit policy in this contract is adopted.
These selections resolve the decisions below; their proposal wording is retained
as the original decision record.

- Owner requested file-based UCI defaults, piece values, and other evaluation
  weights, plus verification of forcing combinations during depth search.
- `include/chess/evaluation.hpp` and `src/core/evaluation.cpp` currently expose
  fixed piece values, MG/EG material/PSTs, pawn/mobility/bishop-pair/rook-file/
  king-safety terms and tempo.
- `src/core/search.cpp::Quiescence` searches captures, promotions, and all check
  evasions, capped at eight extra plies. Quiet checks are not included when the
  side to move is not in check. At the cap it statically evaluates even in check.
- Task 039 independently tracks existing UCI conformance defects. These feature
  tasks neither complete nor replace that task or the Stage 8 release gates.

## Decisions requiring owner confirmation

| Decision | Proposed assumption, not yet approved | Alternative |
|---|---|---|
| Configuration format | INI, UTF-8, documented sections and typed values | Owner-selected format |
| PST coverage | Keep table cells built in; expose MG/EG PST scale weights | Expose full tables as well |
| Forcing search | Extended quiescence at the normal depth horizon | Also extend forcing lines inside the main search |
| Limit behavior | Finite configurable tactical budget; abort incomplete iterations at a hard safety limit while still in check | Another explicitly bounded policy |
| Discovery | Optional `try4.ini` beside executable; explicit `--config <path>` takes precedence | Explicit path only or another documented location |

Do not infer confirmation from the owner's request to prepare these tasks.
The PST and extension questions affect schema, score baselines and validation;
resolve them before dependent implementation. The other defaults may be adopted
as explicitly recorded assumptions unless the owner changes them.

## Proposed contract for approval

1. Precedence: built-in defaults < startup file < later UCI `setoption`.
   A partial file inherits defaults. `--version` does not load a file.
2. Missing optional discovered file is allowed. Missing explicit file or any
   malformed file fails startup with a stderr diagnostic and nonzero status;
   no partial settings are applied. Protocol stdout remains machine-readable.
3. Relative book paths are relative to the configuration file's directory.
4. Startup loading only; hot reload and UCI editing of evaluation weights are
   outside the first increment. UCI options must use task 039's validated ranges.
   Unsupported options such as Chess960 cannot become functional via config.
5. MG/EG piece values and every existing positional scalar/rank-array weight
   are configurable, retaining today's defaults. Kings retain zero material value.
6. At a non-check tactical node, candidate continuations include all legal
   captures, promotions, and moves giving check, including discovered checks
   and checking castling. In check, all legal evasions are candidates, including
   quiet king moves and blocks. No stand-pat is allowed in check.
7. "All" means candidate coverage within the approved budget. Sound alpha-beta
   cutoffs remain allowed. Time/node/stop, repetition, and absolute-ply limits
   take precedence; no claim of a forced win from an incomplete search.
8. Define soft tactical-budget exhaustion separately from absolute recursion
   safety: checks must not be accepted as quiet leaves. An aborted iteration
   must preserve the last completed result and a legal fallback move.

## Deliverables

- Approved decisions recorded here with date and owner response.
- Agreed schema/settings inventory in `docs/CONFIGURATION.md` and tactical
  semantics in `docs/FORCING_SEARCH.md` (implementation tasks create these).
- Dependency/index updates if the owner chooses full PST tables or main-search
  extensions; expand scope explicitly instead of hiding it in a subtask.

## Acceptance criteria

- [x] PST and main-search extension decisions are resolved.
- [x] File discovery, precedence, error policy, paths and field types are defined.
- [x] Candidate coverage, pruning, limits and incomplete-result semantics are defined.
- [x] Every requested capability maps to tasks 041–046 without an implicit exclusion.

## Validation and completion report

Review the contract against the current headers and the owner's request. No build
is needed for this document-only task. Return approved decisions, remaining open
questions, and exact dependent tasks unblocked. Keep status Blocked while scope
decisions remain unanswered.

### Completion — 2026-10-26

Owner approved INI configuration, MG/EG material and positional values with PST
scale controls (not per-cell tables), horizon forcing search (not main-search
extensions), the finite soft/hard tactical budget, and optional
executable-adjacent discovery with explicit `--config` precedence. Tasks
041–046 implemented and validated the contract; evidence is in
`docs/CONFIGURATION_VALIDATION.md`, `docs/FORCING_SEARCH_VALIDATION.md` and
`docs/RELEASE_VALIDATION.md`. No open product decision remains.
