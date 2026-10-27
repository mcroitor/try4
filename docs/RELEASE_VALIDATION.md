# try4 0.1.0 release validation — 2026-10-26

**Candidate:** try4 0.1.0, tagged locally as `v0.1.0`.
**Validated source:** `60226fa56f1098b25a9ca07a3abb44874db8b77a`.
**Decision:** release gates passed for the tested local source and archives.
**Publication:** no remote is configured, so the tag and artifacts are local
only. Hosted CI and a separate physical clean machine were not available; see
[Residual limitations](#residual-limitations).

The annotated tag points at the documentation commit that records these
results. Relative to the validated source it changes only this report and
task-status files, none of which are packaged; see
[Packaging and clean-clone verification](#packaging-and-clean-clone-verification).

The previous report described the pre-fix 2026-10-25 candidate. This report
replaces it with the task 039 UCI fixes, tasks 040–046 configuration/forcing
work, and the final regression matrix.

## Environment and source

- CPU: Intel Core i5-13600HX (14 cores / 20 logical processors).
- Windows 11 build 26200; MSYS2 UCRT GCC 16.2.0, CMake 4.4.3, Ninja 1.13.2.
- Ubuntu on WSL2 kernel 6.18.54.1; GCC 15.2.0, CMake 4.2.3, Ninja 1.13.2,
  glibc 2.43.
- Windows Release tests used `build/feature-check` configured with
  `-DCMAKE_BUILD_TYPE=Release -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON`.
- Windows Debug tests used `build/cmake`.
- Linux Release tests used `build/stage8-linux`; ASan/UBSan used
  `build/asan-feature`.
- Exact source commit and tested archive checksums are recorded below.

## Final gate matrix

| Gate | Observed result |
|---|---|
| Windows Debug full CTest | **23/23 passed**, 64.04 s |
| Windows Release full CTest | **23/23 passed**, 6.17 s |
| Linux Release full CTest | **23/23 passed**, 6.09 s |
| Linux ASan/UBSan full CTest | **23/23 passed**, 194.75 s; no findings |
| Deep perft, depth 5, six positions | **6/6 passed** |
| Deep perft, depth 6, six positions | **6/6 passed** (18,839,030,791 nodes) |
| Recorded 1,000-game UCI stability run | **1000/1000 games**, 0 failures |
| Capped clock | 1000 ms + 50 ms, `movestogo 200`, max 120 plies |
| Move-time maximum | 67.745 ms (threshold 1000 ms) |
| `go infinite` / `stop` latency | 40 probes, max 0.516 ms |
| `isready` latency | 2040 probes, max 4.776 ms |
| A/B benchmark | 36 natural searches + 21 tactical runs; 7/7 tactical per configuration |
| Windows ZIP + SHA-256 + verifier | **PASS** |
| Linux ZIP + SHA-256 + verifier | **PASS** |
| Clean-clone Windows Release full CTest | **23/23 passed**, 10.79 s |
| Clean-clone Linux Release full CTest | **23/23 passed**, 6.24 s |
| Clean-clone ZIPs + SHA-256 + verifier | **PASS** (both platforms) |
| Tag `v0.1.0` | Created locally on the documentation commit |

The 23 CTest targets are 21 Catch2 executables, one Python real-process suite,
and one CMake executable smoke test. There are 160 Catch2 test cases in total.

## Commands executed

Windows build and matrix (repository root):

```powershell
cmake --build build/cmake --config Debug --parallel 4
ctest --test-dir build/cmake -C Debug --output-on-failure --output-junit regression-final.xml

cmake --build build/feature-check --config Release --parallel 4
ctest --test-dir build/feature-check -C Release --output-on-failure --output-junit regression-final.xml

powershell -ExecutionPolicy Bypass -File scripts/perft_deep.ps1 `
  -Exe build/feature-check/try4_perft_bench.exe -Depth 5
foreach ($p in "start","kiwipete","position3","position4","position5","position6") {
  powershell -ExecutionPolicy Bypass -File scripts/perft_deep.ps1 `
    -Exe build/feature-check/try4_perft_bench.exe -Depth 6 -Position $p
}

python scripts/benchmark_forcing.py --engine build/feature-check/try4_uci.exe `
  --output build/feature-validation/benchmark-final.json

cpack --config build/feature-check/CPackConfig.cmake -C Release
python scripts/verify_release.py build/feature-check/packages/try4-0.1.0-windows-x86_64.zip
```

Linux/WSL matrix:

```bash
cmake --build build/stage8-linux --parallel 4
ctest --test-dir build/stage8-linux -C Release --output-on-failure \
  --output-junit regression-final.xml

cmake --build build/asan-feature --parallel 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build/asan-feature --output-on-failure \
  --output-junit regression-final.xml

cpack --config build/stage8-linux/CPackConfig.cmake -C Release
python3 scripts/verify_release.py build/stage8-linux/packages/try4-0.1.0-linux-x86_64.zip
```

JUnit reports are under each build directory (`regression-final.xml`); detailed
logs are in `Testing/Temporary/LastTest.log`. The sanitizer command used leak
detection and halt-on-error as shown. No suppression file is used.

## Stability run

The archived report is
[`docs/stability/uci-1000/README.md`](stability/uci-1000/README.md):

```sh
python scripts/uci_stability.py --engine build/feature-check/try4_uci.exe \
  --games 1000 --workers 4 \
  --output build/feature-validation/stability-1000 \
  --stop-probe-every 25 \
  --hardware "Intel i5-13600HX / Windows 11 26200" \
  --label v0.1.0-final
```

Result: 1000/1000 games in 1290.4 s, W/D/L 248/488/264, 99,293 plies,
92,839 moves, zero protocol failures, maximum move wait 67.745 ms, maximum
`stop` latency 0.516 ms. Every move was replayed through the CLI legal-move
judge. This is a stability check between identical engines, not a strength
claim. The owner's separate CuteChess play report is not a substitute for the
archived protocol run, and no GUI transcript was archived.

## Deep perft and the reference correction

Depth 5 passed for all six positions. Depth 6 was run for all six:

```text
start      119060324
kiwipete   8031647685
position3  11030083
position4  706045033
position5  3048196529
position6  6923051137
```

The final deep run exposed a stale test-data value: `position3` depth 6 was
recorded as `11000083`, while the engine returned `11030083`. The engine count
was confirmed with Stockfish 19 (`go perft 6`) as `11030083`; the standard
position-3 reference is also that value. Only
`tests/data/perft/reference.tsv` was corrected. This was a depth-6 fixture
typo; depths 1–5 were unaffected and no engine change was made.

## Configuration and forcing validation

The feature reports are
[`CONFIGURATION_VALIDATION.md`](CONFIGURATION_VALIDATION.md) and
[`FORCING_SEARCH_VALIDATION.md`](FORCING_SEARCH_VALIDATION.md). Key results:

- An explicit default INI and built-ins produced identical best moves, scores,
  depths and node counts at fixed depth on four positions.
- Changed MG/EG pawn value (+100) changed the real UCI score by exactly +100 on
  the controlled fixture.
- Relative `book_file` values with spaces, `#`, `;` and `=` resolved against the
  explicit config directory and produced a real book move.
- Invalid startup files exited nonzero with clean stdout; UCI overrides survive
  `ucinewgame`; `--version` ignores a discovered invalid file.
- Forcing checks found the independently verified quiet finish in Légal's mate;
  disabling checks did not report a mate.
- 21 benchmark tactical runs (7 fixtures × 3 configurations) passed. At depth 1
  the forcing configuration found mate in 2 on the Légal fixture with 282 nodes;
  captures-only returned −386 cp with 182 nodes.

## Packaging and clean-clone verification

Both archives contain exactly one UCI executable, the CLI, license/notices,
README, changelog, UCI/configuration/forcing docs, and one non-active
`examples/try4.ini`. The verifier extracted each archive, removed compiler
directories from `PATH`, checked both `--version` outputs, ran a UCI handshake
and search, loaded a config from a path containing spaces, loaded a relative
text book, applied a GUI-style `Hash` override, and confirmed an invalid config
exits nonzero with empty stdout.

The same build/test/package procedure was repeated from fresh local `git
clone`s of commit `60226fa`, not from the working tree:

- Windows clone: `%TEMP%\try4-final-60226fa`; Release build, **23/23 tests in
  10.79 s**, CPack ZIP and verifier passed.
- Linux clone: `build/clean-linux-60226fa`; Release build, **23/23 tests in
  6.24 s**, CPack ZIP and verifier passed.

This substitutes for an unavailable separate physical machine. The resulting
archives were copied to `build/release-artifacts/` and are the release
artifacts. Checksums identify those exact artifacts; ZIP metadata and toolchain
differences can change bytes on another rebuild.

| Archive | Size (bytes) | SHA-256 |
|---|---:|---|
| `try4-0.1.0-windows-x86_64.zip` | 1430208 | `b79daaefc3c5f1c861deac94893285f5c4ca9861eb66c8f0d87fca3c0c9afd06` |
| `try4-0.1.0-linux-x86_64.zip` | 168259 | `87477fc1b4b50f39ec7eee115e2a58d5a64c65f23e48368ea483e83a1f37adf6` |

ZIP archives are not byte-reproducible: rebuilding the same Linux commit
produced the same file size but a different container checksum. The adjacent
`.sha256` files in `build/release-artifacts/` are authoritative for the tested
archives.

The annotated tag points at the commit that adds this report. `git diff
--name-only 60226fa v0.1.0` contains only this report and task-status files,
none of which are packaged. A full rebuild from the tag passed 23/23 tests and
the verifier on both platforms. Its Linux package entries are byte-identical to
the tested archive; the Windows package has the same entries, and its two
executables differ only in the four/five PE-header bytes that hold the linker
timestamp and checksum. The compiled inputs are unchanged.

## Residual limitations

- No git remote is configured. `v0.1.0` and the archives are local; no public
  push or hosted GitHub Actions result exists. The workflow was syntax-checked
  and its JUnit paths were corrected, but it was not executed by GitHub.
- No separate physical clean machine and no MSVC run were available. Local
  clean-clone plus isolated-PATH package extraction is the substitute. Linux
  packages target glibc 2.43 and are not verified on older distributions.
- The stability run is protocol-level identical-engine self-play. It does not
  establish playing strength, GUI-side latency, or compatibility with every
  GUI. No strength claim is made for the configurable evaluation or forcing
  search.
- Pondering, `searchmoves`, `go mate`, Chess960, NNUE, SMP and tablebases remain
  unsupported. Search info is emitted on completion rather than periodically.
- A first-run `try4_release_smoke` failure occurred once while a rebuild loop was
  running against the same build tree; the CLI binary was found truncated to
  zero bytes by a failed relink, not by an engine defect. Removing it and
  rebuilding restored a 2,764,500-byte executable; 10 consecutive first-run
  version checks and the final sequential clean-clone runs on both platforms
  (23/23 each) passed. Do not build and test the same tree concurrently.

Deferred verification work is tracked in
[`tasks/task_047.md`](../tasks/task_047.md).
