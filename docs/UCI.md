# Running try4 in a chess GUI

`try4_uci` is the engine executable. `try4` is a separate inspection CLI and
must not be selected as the engine in CuteChess.

1. Extract the versioned runtime ZIP.
2. Add `bin/try4_uci.exe` on Windows or `bin/try4_uci` on Linux to CuteChess
   as a UCI engine. No command-line arguments are needed.
3. Choose a time control in the GUI. The engine accepts `go depth N`,
   `go nodes N`, `go movetime MS`, and clock fields `wtime`, `btime`, `winc`,
   `binc`, `movestogo` (times in milliseconds).
4. For a book, set `BookFile` to an absolute `.bin` or `.txt` path. Books
   are not included in the runtime ZIP.

The project owner reports successful and stable play in CuteChess. The archived
release evidence is a 1,000-game protocol-level run using two real UCI processes
at 1000 ms + 50 ms with `movestogo 200`; it completed without illegal moves,
hangs, crashes or time forfeits. See
[`stability/uci-1000/`](stability/uci-1000/). No GUI transcript was archived, so
this does not claim GUI-specific latency or universal GUI compatibility.

## Options

| Name | Default | Notes |
|---|---|---|
| Hash | 16 MB | Transposition table size; advertised range 1–131072. |
| Move Overhead | 20 ms | Subtracted from movetime when movetime exceeds the overhead. |
| Threads | 1 | Search is single-threaded. |
| OwnBook | true | Book use requires a selected BookFile. |
| BookFile | empty | Polyglot `.bin` or `<id><TAB><UCI moves>` `.txt`. |
| BookMaxPly | 40 | Half-move cutoff; 0 means unlimited. |

Startup values can be supplied by `--config PATH` or optional executable-adjacent
`try4.ini`. UCI declarations reflect those values; later GUI overrides win and
survive `ucinewgame`. See [CONFIGURATION.md](CONFIGURATION.md) for evaluation
weights and [FORCING_SEARCH.md](FORCING_SEARCH.md) for tactical-horizon controls.

Paths with spaces are accepted as an unquoted UCI option value. Opening-book
selection is weighted random. A miss or disabled book falls back to search;
infinite/ponder commands bypass the book.

## Protocol example

Send these commands interactively, waiting for the indicated replies:

```text
uci
# wait for uciok; id name includes the version
isready
# wait for readyok
ucinewgame
position startpos moves e2e4 e7e5
go movetime 100
# wait for bestmove before sending the next position/go
quit
```

The `#` lines above are explanatory and should not be sent by a GUI.
`--version` prints the same version without starting the protocol loop.
`stop` requests cancellation, and `quit` joins the search thread. Search info
is currently emitted at completion, not periodically.

## Protocol behavior and remaining limitations

- `go infinite` retains its result until `stop`, including a found mate or a
  terminal root; `isready` remains responsive. Recursion remains safety-bounded.
- Mate scores use `score mate`; `ucinewgame` does not emit `readyok`.
- Pondering, `searchmoves`, `go mate`, and Chess960 are not implemented.
  Unsupported go modes are rejected with `info string Unsupported or invalid go
  command`, without starting a misleading unrestricted search. `ponderhit` is
  ignored because no ponder search can be started.
- Position commands are transactional and coordinate-UCI only; a malformed
  suffix retains the previous position. General command whitespace is accepted.
- Invalid option values retain the prior valid setting. Search resource changes
  stop and join an existing worker before mutation.

Process tests cover natural depth/time completion, infinite/stop (including
mate), option restart, startup failures and actual configured evaluation/book use.
The final archived 1,000-game release stability run is in
[`stability/uci-1000/`](stability/uci-1000/). It is identical-engine
protocol-level testing, not a strength or GUI-latency measurement.
