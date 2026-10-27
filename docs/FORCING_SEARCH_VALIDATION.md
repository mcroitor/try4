# Forcing-search validation — 2026-10-26

See [`CONFIGURATION_VALIDATION.md`](CONFIGURATION_VALIDATION.md) for the complete
local regression matrix and [`FORCING_SEARCH.md`](FORCING_SEARCH.md) for bounds
and incomplete-result semantics. This report covers the final try4 0.1.0
candidate.

## Independently verified tactical fixtures

The `try4_forcing_tests` target has six cases and 3,414 assertions. It compares
the optimized search against a separate bounded exhaustive minimax reference
with no TT, sorting or alpha-beta where the fixtures finish below the hard
bound.

### Légal's mate

```text
r2qkbnr/ppp2ppp/2np4/4N3/2B1P3/2N4P/PPPP1PP1/R1BbK2R w KQkq - 0 7
```

This is Légal's mate after `...Bxd1??`. `Bxf7+` (`c4f7`) has exactly one legal
reply, `Ke7` (`e8e7`), followed by quiet `Nd5#` (`c3d5`). The test verifies that
reply count and checkmate through legal move generation, independently of the
search score. With normal depth 1 and tactical max 2, forcing checks returns
`kMateScore - 3` and the three-move PV. Disabling quiet checks does not report a
mate. The corresponding black-to-move fixture verifies `score mate -1`.

### Oracle positions

Four low-branching fixtures are compared against the exhaustive reference at
depth 1 with `forcing_max_plies=2`: a king/queen position, a king/rook position,
a castling-rights position and an en-passant position. Both ordering/TT-enabled
and ordering/TT-disabled searches must return the reference score, a legal move
and an unchanged input position.

### Candidate coverage

Each candidate class is verified by making the named move and confirming that
it gives check, then requiring optimized search to match the exhaustive
reference:

| Fixture FEN | Move | Class |
|---|---|---|
| `5k2/8/8/8/8/8/8/4K2R w K - 0 1` | `e1g1` | checking castle |
| `3k4/1P6/8/8/8/8/8/K7 w - - 0 1` | `b7b8q` | promotion check |
| `8/2k5/8/3pP3/8/8/8/7K w - d6 0 2` | `e5d6` | en-passant check |
| `k7/8/8/8/N7/8/8/R6K w - - 0 1` | `a4b6` | discovered check |

The promotion fixture was corrected during the final matrix: the earlier FEN
left the black king already in check with White to move, so the white pawn could
also capture the king. The new FEN is legal before the promotion and `b7b8q`
gives check. This was a test-fixture defect, not an engine defect; the corrected
case passes in Debug, Release and sanitizers.

### Checked horizon and hard boundary

```text
3rkr2/2pp1p1p/8/8/8/2b5/3P3K/4R3 b - - 0 1
```

After `Re1+`, Black has exactly one legal evasion, the quiet block `...Be5`
(`c3e5`). With a zero nominal tactical budget and a checked node, the search
must not evaluate a static leaf. The test requires the quiet block to be
searched and refuted by `Rxe5#`, with score below the mate threshold and the
expected two-move PV.

A second fixture (`6k1/8/3b4/8/2b4q/8/6P1/R5K1 w - - 0 1`) sets
`forcing_max_plies=forcing_hard_plies=1` and requires the iteration to abort
without a completed depth, while preserving a legal fallback move. Cancellation
and a 100-node global limit are checked on a checking tree; position restoration
is checked after every search.

## Recorded A/B benchmark

```text
python scripts/benchmark_forcing.py --engine build/feature-check/try4_uci.exe \
  --output build/feature-validation/benchmark-final.json
```

The final run contains 36 natural-completion UCI searches and 21 tactical cases:
three configurations (built-ins, explicit default INI, forcing checks) on four
positions and seven tactical fixtures. Version, platform, exact INI text, FENs,
responses and wall times are in the JSON.

Fixed-depth sample:

| Position | Captures-only score/nodes | Forcing-check score/nodes |
|---|---|---|
| Start | +43 cp / 41 | +43 cp / 41 |
| Légal fixture | −386 cp / 182 | mate in 2 / 282 |
| Kiwipete | +91 cp / 2385 | +91 cp / 3091 |
| Hanging queen | +909 cp / 53 | +909 cp / 53 |

All 7 tactical fixtures passed in all three configurations. At points 1000 and
movetime 100 the forcing configuration found the Légal mate with 271 nodes;
captures-only did not. Quiet checks increase work substantially: Kiwipete depth 1
took about 28 ms captures-only versus 172 ms with checks in this sample. There is
no speed or Elo improvement claim. Node/time limits still truncate the search.

The built-in and explicit-default-file fixed-depth outputs matched on all four
positions. Movetime node counts are not expected to match because they depend on
wall-clock scheduling.

## Process-level and release evidence

The real-process suite verifies the same behavior through UCI and `--config`:
with `forcing_max_plies=2`, the Légal position returns `bestmove c4f7` and
`score mate 2` when checks are enabled; disabling checks removes the mate score.
The reverse-side fixture returns `score mate -1`. The full 1,000-game UCI
stability run (default forcing setting: checks disabled) completed with no
failures; see [`stability/uci-1000/README.md`](stability/uci-1000/README.md).

## Limits of the evidence

- "All candidates" means coverage within the configured budget; alpha-beta and
  TT cutoffs can avoid searching branches without falsely claiming enumeration.
- The oracle fixtures are intentionally small and bounded. They are not a proof
  for every legal position or every budget.
- The benchmark is a small A/B measurement, not a strength experiment.
- Default behavior retains checks disabled, so releases are not claiming a
  strength change from this feature.
