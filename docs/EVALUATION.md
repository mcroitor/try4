# Evaluation

`chess::Evaluate` returns a tapered centipawn score from the side-to-move
perspective. The search uses it for quiescence stand-pat, depth-zero leaves, and
the forced fallback score. `chess::EvaluateDetailed` exposes the totals, phase,
individual positional terms, and tempo for tests and diagnostics.

## Scale and material

One pawn is nominally 100 centipawns. The evaluator uses separate middlegame and
endgame material values:

| Piece | Middlegame | Endgame |
|---|---:|---:|
| Pawn | 100 | 120 |
| Knight | 320 | 300 |
| Bishop | 330 | 320 |
| Rook | 500 | 540 |
| Queen | 900 | 900 |
| King | 0 | 0 |

`PieceValue` retains the middlegame values for MVV-LVA ordering and the
material-only `EvaluateMaterial` diagnostic baseline.

## Piece-square tables

Every piece has separate middlegame and endgame tables. Tables are written from
White's perspective with rank 1 first; Black looks up the vertically mirrored
rank. The values are deliberately modest, keeping material dominant while
rewarding centralization, pawn advancement, castled king placement, and king
activity in the endgame.

## Positional terms

All positional constants live in `src/core/evaluation.cpp` so task 028 can tune
them behind the self-play harness. Terms are computed for each color separately
and then expressed from the side-to-move perspective.

### Pawn structure

- **Doubled pawn:** 15 cp middlegame / 20 cp endgame penalty for each pawn on a
  file beyond the first.
- **Isolated pawn:** 12 cp middlegame / 15 cp endgame penalty when no friendly
  pawn remains on either adjacent file.
- **Passed pawn:** no enemy pawn on the same or an adjacent file ahead of the
  pawn. The rank-scaled bonus is:

  | Relative rank | 2 | 3 | 4 | 5 | 6 | 7 |
  |---|---:|---:|---:|---:|---:|---:|
  | Middlegame | 5 | 10 | 20 | 35 | 55 | 80 |
  | Endgame | 10 | 20 | 35 | 55 | 80 | 120 |

These are distinct features: a doubled passed pawn can receive both the doubled
penalty and the passed bonus.

### Mobility

Mobility counts pseudo-legal destination squares for bishops, rooks, and queens,
including captures and stopping at the first blocker. It does not filter pins,
checks, or king safety; that approximation keeps the term inexpensive at search
leaves. Each pseudo-move is worth 3 cp middlegame / 2 cp endgame.

Knights and pawns are intentionally excluded from mobility. Their contribution
comes from material and the piece-square tables.

### Bishop pair

Two or more bishops receive 30 cp middlegame / 50 cp endgame. A single bishop
receives nothing.

### Rook files

For each rook:

- no pawns on its file: 20 cp middlegame / 10 cp endgame;
- no friendly pawn but at least one enemy pawn: 10 cp middlegame / 5 cp
  endgame;
- any friendly pawn on the file: no bonus.

### King safety

When the king is on its relative first or second rank, each file in the three-file
shield receives:

- 12 cp middlegame / 3 cp endgame for a pawn one rank in front;
- 6 cp middlegame / 1 cp endgame for a pawn two ranks in front;
- otherwise a 10 cp middlegame / 2 cp endgame missing-pawn penalty.

The attacker count is the number of enemy pieces that attack any square in the
king's 3x3 zone. Each attacking piece costs 12 cp middlegame / 4 cp endgame.
Both the shield and attacker weights shrink in the endgame so the term does not
keep the king tied to the back rank in simplified positions.

### Tempo

The side to move receives a flat 10 cp bonus. It is added after interpolation and
is not phase-scaled.

## Phase and interpolation

Phase weights count the remaining non-pawn material:

- knight or bishop: 1
- rook: 2
- queen: 4
- pawn or king: 0

The weights sum to 24 for the initial position. The phase is capped at 24, and
the tapered score is:

```text
score = (middlegame * phase + endgame * (24 - phase)) / 24 + tempo
```

The `middlegame` and `endgame` breakdown fields include all material, PST, and
positional terms but exclude tempo. A perfectly symmetric board therefore scores
`+10` for the side to move rather than zero.

All breakdown fields are from the side-to-move perspective. A color-swapped and
rank-mirrored position preserves the side-to-move score, including tempo.
Swapping colors while keeping the same side to move reverses the positional part
but not the tempo component. The tables and all positional terms are
file-symmetric as well.

## Tests

`tests/evaluation_tests.cpp` covers:

- symmetric positions, which retain only the tempo bonus;
- a rich color-swapped/rank-mirrored position, including every term and the
  same-side color-swap relation;
- a queen advantage from both sides, including its black-queen mirror;
- phase 24, phase 0, and a mixed-material phase with the interpolation formula;
- doubled, isolated, passed, and blocked pawn structure boundaries;
- bishop, rook, and queen mobility, including blockers and direction rules;
- the bishop-pair boundary;
- open, semi-open, and closed rook files;
- pawn-shield and attacker king-safety boundaries plus endgame reduction;
- pawn, knight, bishop, rook, queen, and king PST color symmetry.

Current full-suite and tactical baselines are recorded in
[`TACTICAL.md`](TACTICAL.md) and the root test inventory.
