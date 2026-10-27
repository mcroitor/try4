# Engine startup configuration

`try4_uci --config PATH` loads an explicit UTF-8 INI file. Without arguments,
the engine looks for optional `try4.ini` beside the executable (not in the
working directory). No file means built-in defaults. `--version` does not
read configuration. Invalid arguments, unreadable explicit files, invalid
discovered files, or invalid values exit nonzero with a stderr diagnostic.
No partial settings are applied. The CLI does not load this file.

`try4_selfplay --config PATH` explicitly loads evaluation/forcing/hash settings
for both candidate and material-only baseline. It does not auto-discover files
or use the configured UCI book; its `--openings` seeds the test games. CLI
`--tt-mb`, node and movetime options remain authoritative. Each result directory
includes `engine-settings.ini`, a normalized effective settings snapshot, and
the existing openings, seed, compiler, limits and hardware records. Supply a
hardware description through `--hardware` when collecting reusable benchmarks.

Precedence: built-ins < startup file < later UCI `setoption`. `ucinewgame`
retains session overrides. UCI advertises effective startup defaults. Evaluation
and forcing settings are file-only; restart to change them. Each search owns
a settings snapshot, and changed evaluation/forcing/quiescence policy clears
the TT before reuse. No environment expansion or file hot reload is performed.

Copy `config/try4.ini` (all defaults) or the packaged documentation example.
It is never automatically installed beside the executable. Relative `book_file`
paths are relative to the file's directory. Later UCI BookFile paths remain
relative to process working directory. Paths are unquoted literal values:
spaces, `#`, `;`, backslashes, and `=` are preserved internally. Leading/trailing
whitespace is stripped. `book_file=` means no book.

## Grammar and schema

Sections/keys and booleans are case sensitive; only `true`/`false` are booleans.
LF/CRLF and an optional initial UTF-8 BOM are supported. Comments start with
`#` or `;` at the beginning of a trimmed line; there are **no inline comments**.
Integers are decimal, with no leading `+` or trailing garbage. Arrays are exact
length comma-separated integers. Unknown sections/keys, duplicates, malformed
UTF-8/NUL, lines over 64 KiB, and invalid ranges fail with file/line/key context.

| `[uci]` key | Range/default |
|---|---|
| hash_mb | 1–131072; 16 |
| move_overhead_ms | 0–5000; 20 |
| threads | 1 only; other values are rejected with an explicit message (SMP is not implemented) |
| own_book | true |
| book_file | empty string |
| book_max_ply | 0–10000; 40; 0 means unlimited |

`[evaluation]` defaults are listed in the example:

- `material_mg`, `material_eg`: exactly seven integers (empty, pawn, knight,
  bishop, rook, queen, king), each 0–10000 centipawns. Empty/king must be zero.
- `passed_mg`, `passed_eg`: exactly eight integers, 0–1000, indexed by relative rank.
- Scalar weights (all 0–1000): `mobility_mg/eg`, `bishop_pair_mg/eg`,
  `rook_open_mg/eg`, `rook_semi_open_mg/eg`, `doubled_pawn_mg/eg`,
  `isolated_pawn_mg/eg`, `shield_one_mg/eg`, `shield_two_mg/eg`,
  `shield_missing_mg/eg`, `king_attacker_mg/eg`, `tempo`.
- `pst_mg_percent`, `pst_eg_percent`: 0–1000 percent; default 100. Table cells
  stay built in. Scale per square with integer truncation toward zero.
- Penalties are positive magnitudes subtracted by evaluation. Phase weights
  remain fixed. Material-only A/B evaluation and move ordering use configured
  middlegame material values. Full evaluation uses MG/EG tapering.
- Validated score arithmetic is bounded (at most 64 squares); final static
  scores are clamped to ±90000, below the mate band. Invalid direct API settings
  return neutral evaluation; file input reports an error before any search.

`[search]`: `forcing_checks` (false), `forcing_max_plies` (0–120, default 8),
`forcing_hard_plies` (1–120, default 32). Max must not exceed hard.
See [FORCING_SEARCH.md](FORCING_SEARCH.md) before increasing the checking budget.

Example to enable forcing checks and a different knight value:

```ini
[evaluation]
material_mg=0,100,340,330,500,900,0
[search]
forcing_checks=true
forcing_max_plies=8
forcing_hard_plies=32
```
