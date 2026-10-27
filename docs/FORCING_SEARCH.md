# Bounded forcing search

The approved feature is **horizon quiescence**, not extensions inside the normal
depth search. At the normal horizon, captures (including en passant), all
promotions, and optionally quiet checking moves continue the search.
Set `[search] forcing_checks=true` to enable the added checks. Defaults keep
checks disabled to preserve a controlled legacy comparison.

When not in check, all legal captures/promotions and, when enabled, all moves
giving check are candidates. Checking captures appear once. Check detection
makes and undoes the legal move, so discovered checks and checking castles are
included. Standard stand-pat and alpha-beta cutoffs remain: candidate coverage
is not exhaustive enumeration of every line. No SEE/delta pruning is added.

When in check, **all legal evasions** are candidates, including quiet king
moves and blocks. There is no stand-pat in check. At `forcing_max_plies`, a
non-check node is evaluated statically. A checked node continues evasions;
if still unresolved at `forcing_hard_plies` or absolute search ply 127, the
iteration is aborted. No incomplete score is stored as exact or reported as a
completed depth. The last completed result/legal fallback is preserved.

Time, node, stop, repetition, draw, and mate handling apply to the combined
main and tactical search. These take precedence over tactical completeness.
Configuration changes invalidate TT scores. Perpetual checks must terminate
through draw detection, budget exhaustion, or cancellation, not unbounded recursion.

This corrects the old behavior that statically evaluated checked nodes at the
eight-ply cap. It can change some legacy node counts/scores at that boundary.
There is no assertion of stronger play without a recorded A/B measurement.
