# Third-party references and data

## Polyglot format constants

`src/core/polyglot_random.inc` contains the 781 numeric random keys defined by
the Polyglot book format. These are interoperability data, not the engine's
internal Zobrist keys. The values were obtained from the table in
https://github.com/niklasf/python-chess/blob/master/chess/polyglot.py and are
specified at https://www.chessprogramming.org/Polyglot.
The book reader and hash implementation in try4 are independently implemented.

## Fathom Syzygy probing library

`src/third_party/fathom/` contains the Fathom Syzygy tablebase probing
library (copyright 2013-2018 Ronald de Man, 2015 basil00, and 2016-2024 Jon
Dart) under the MIT license. The license text is in
`src/third_party/fathom/LICENSE`. Fathom is compiled into the engine to read
external `.rtbw`/`.rtbz` files; no tablebase data is bundled or redistributed.
The user selects a tablebase directory with `SyzygyPath` or
`[uci] syzygy_path`. Syzygy tablebase files themselves are external data; see
[docs/SYZYGY.md](docs/SYZYGY.md) for layout and provenance guidance.

## Catch2

Tests fetch Catch2 v3.5.2, copyright Catch2 contributors, under the Boost
Software License 1.0: https://github.com/catchorg/Catch2/blob/v3.5.2/LICENSE.txt.
Catch2 is used only by test executables and is not included in the runtime ZIP.

## Opening sets

The user-supplied `docs/selfplay/tournament.bk` and its converted
`tournament.txt` are external data with no recorded source/license provenance.
The project MIT license does not assert ownership of third-party data.
Opening books and other self-play data are excluded from binary packages;
users select their own book file using the `BookFile` UCI option.
