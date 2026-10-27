# Third-party references and data

## Polyglot format constants

`src/core/polyglot_random.inc` contains the 781 numeric random keys defined by
the Polyglot book format. These are interoperability data, not the engine's
internal Zobrist keys. The values were obtained from the table in
https://github.com/niklasf/python-chess/blob/master/chess/polyglot.py and are
specified at https://www.chessprogramming.org/Polyglot.
The book reader and hash implementation in try4 are independently implemented.

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
