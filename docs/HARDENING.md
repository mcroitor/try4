# Hardening notes

This note records the hardening work from task 021 and the search-phase
sanitizer pass through task 024: malformed-FEN fuzzing, extended random
playouts, and sanitizer coverage.

## FEN fuzzing

`tests/fuzz_tests.cpp` exercises:

- empty and whitespace-only input;
- very long input (10,000 bytes);
- invalid UTF-8/high-bit bytes;
- missing and extra FEN fields;
- bad rank digits and impossible rank widths;
- invalid pieces, side-to-move, castling rights, en passant targets, and clocks;
- 2,000 deterministic pseudo-random strings (seed `0x5EEDF00D`) drawn from a
  FEN-like alphabet.

Every rejected input must produce a non-empty error, must not throw, and must
leave an existing `Position` unchanged. Accepted inputs are round-tripped through
`ToFen()`, re-parsed, and checked against `ComputeHash()` and legal-move
generation. The fixed valid corpus also covers the accepted path.

No parser changes were needed for the current corpus; the tests are retained as
regression coverage.

## Random playouts

The default property suite runs 10,000 games with a 16-ply cap to keep CI fast.
The same binary accepts environment overrides:

```sh
TRY4_PROPERTY_GAMES=100000 ./build/cmake/try4_property
# optional: TRY4_PROPERTY_MAX_PLIES=32
```

A full 100,000-game run (`TRY4_PROPERTY_GAMES=100000`, 16-ply cap, Debug build,
GCC 16.2.0 on Windows) completed in 113.9 seconds with 6,697,265 assertions.
It checks, after every move and undo:

- exact FEN, board rendering, side, rights, en passant, clocks, history depth,
  and hash restoration;
- `Hash() == ComputeHash()`;
- terminal results and legal-move counts.

Special-move and terminal coverage is also exercised by the perft corpus and the
dedicated legal/draw/result suites; the extended run is not part of default CTest
because of its runtime.

The default suite is deterministic: seed `0x5EEDC0FFEE`, 10,000 games, 16 plies.

## Sanitizers

The Ubuntu CI job configures a Debug build with:

```sh
-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
```

and runs the complete CTest suite with:

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
```

Local verification used Ubuntu WSL2 with GCC 15.2.0 and a Debug tree configured
with `-fsanitize=address,undefined`. All 17 tests passed (183.90 seconds; perft
70.79 s, property 58.67 s, tactical 32.48 s, search/TT/time 15.90 s, evaluation
0.19 s). No AddressSanitizer, LeakSanitizer, or UndefinedBehaviorSanitizer
reports were produced. The hard-deadline poll interval is 32 nodes so the
50 ms clock tolerance also holds under sanitizer overhead.

Sanitizer suppressions are intentionally empty. No third-party exception has
been needed.

### Final 0.1.0 verification (2026-10-26)

The 0.1.0 release source was rebuilt under Ubuntu/WSL2 with GCC 15.2.0 and
ASan/UBSan, including UCI, opening books, configuration, forcing search and
version smoke coverage. **All 23 CTest targets passed in 194.75 seconds** with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. No sanitizer findings were
reported. The older 2026-10-25 run covered only 20 targets and preceded the
task 039/041–046 changes.

Feature and release evidence, including exact commands, is recorded in
[`RELEASE_VALIDATION.md`](RELEASE_VALIDATION.md).
