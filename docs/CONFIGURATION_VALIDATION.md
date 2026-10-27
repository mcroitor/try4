# Configuration implementation validation — 2026-10-26

Tasks 040–046 implement the owner-approved INI/default-precedence contract,
MG/EG values and PST scales, and bounded horizon forcing search. Main-search
extensions and full PST-cell editing were explicitly not selected. This report
records the final local matrix for try4 0.1.0; the cross-feature release matrix
is in [`RELEASE_VALIDATION.md`](RELEASE_VALIDATION.md).

## Implementation

- `configuration.hpp/.cpp`: strict transactional UTF-8 INI model/parser/load,
  optional executable-adjacent discovery, explicit CLI path, relative book paths,
  normalized snapshots and diagnostics.
- `EvaluationParameters`: explicit per-search values/weights with legacy defaults,
  validated arithmetic bounds, fixed phase and ±90000 static score clamp.
- `Searcher`: copies limits, including evaluation/forcing parameters, and clears
  TT when evaluation, mode, forcing or quiescence policy changes.
- UCI: configured startup declarations, file < session overrides, persistent
  options across newgame, resource stop/join, proper infinite/stop and mate output.
- Self-play: explicit `--config` loads evaluation/forcing/hash; result directories
  save normalized `engine-settings.ini` and engine version.

## Final local matrix

| Check | Observed result |
|---|---|
| Windows Debug, GCC 16.2.0 | **23/23 CTest targets passed**, 64.04 s |
| Windows Release, GCC 16.2.0 | **23/23 passed**, 6.17 s |
| Ubuntu/WSL Release, GCC 15.2.0 | **23/23 passed**, 6.09 s |
| Linux ASan/UBSan | **23/23 passed**, 194.75 s; no findings |
| Real-process tests | **10/10 passed**, including natural completion, mate signs, infinite/stop, quit-during-search, book miss/tight clock, and 12-iteration restart stress |
| Example/snapshot parser equivalence | Passed; `config/try4.ini` parses to the built-in defaults |
| Default INI vs built-ins at fixed depth | Identical score, depth, nodes, PV and bestmove on four positions |
| Configured endgame pawn value +100 | Actual UCI score increased by exactly 100 on the controlled pawn fixture |
| Book path relative to explicit file, containing spaces/#/;/= | Actual configured book move observed (`bestmove e2e4`) |
| Invalid startup, discovery, override/newgame, TT policy | Covered by config/process tests |
| Packaged explicit config + relative book + GUI override | Windows and Linux archives passed `scripts/verify_release.py` |

Commands (repository root):

```text
cmake --build build/cmake --config Debug --parallel 4
ctest --test-dir build/cmake -C Debug --output-on-failure --output-junit regression-final.xml
cmake --build build/feature-check --config Release --parallel 4
ctest --test-dir build/feature-check -C Release --output-on-failure --output-junit regression-final.xml
python tests/engine_process_tests.py --engine build/feature-check/try4_uci.exe --source .
python scripts/benchmark_forcing.py --engine build/feature-check/try4_uci.exe --output build/feature-validation/benchmark-final.json
```

Linux commands were invoked via `wsl -d Ubuntu --`, using absolute source/build
paths under `/mnt/c/projects/nbp/try4`:

```text
cmake --build /mnt/c/projects/nbp/try4/build/stage8-linux --parallel 4
ctest --test-dir /mnt/c/projects/nbp/try4/build/stage8-linux -C Release --output-on-failure --output-junit regression-final.xml
cmake --build /mnt/c/projects/nbp/try4/build/asan-feature --parallel 4
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir /mnt/c/projects/nbp/try4/build/asan-feature --output-on-failure --output-junit regression-final.xml
```

### Recorded A/B benchmark

`build/feature-validation/benchmark-final.json` contains 36 natural-completion
UCI searches and 21 tactical cases (7 fixtures × built-ins, default-file and
forcing configurations). The forcing file changes only
`forcing_checks=false` → `true`; the default tactical budget remains 8 soft / 32
hard plies.

| Configuration | Tactical | Searches | Nodes | Max stop |
|---|---:|---:|---:|---:|
| built-ins | 7/7 | 12 | 45,041 | 0.416 ms |
| explicit default file | 7/7 | 12 | 45,671 | 0.451 ms |
| forcing checks | 7/7 | 12 | 15,127 | 0.472 ms |

Fixed-depth default-file and built-in results matched on all four benchmark
positions. Movetime node counts differ because they are time-dependent. The
forcing run found the verified Légal mate-in-2 at depth 1 (282 nodes versus
−386 cp / 182 nodes with captures only). It also visited more nodes on Kiwipete
at depth 1 (3,091 versus 2,385) and took longer (about 172 ms versus 28 ms).
The lower forcing node total is caused by early proven mates, not a general
speedup. No Elo or strength claim follows from these measurements.

## Scope of evidence

Hardware: Intel i5-13600HX, Windows 11 and Ubuntu/WSL2, Ninja. Tests use built-in
defaults unless their fixture explicitly overrides settings. Benchmark JSON
records the actual configuration text and process wall times. Concurrent
build/test activity can affect timing; this small run is not a performance
guarantee.

The feature code is committed and the release archives are verified in
[`RELEASE_VALIDATION.md`](RELEASE_VALIDATION.md). Hosted CI, MSVC and a separate
physical clean machine remain unverified; the local clean-clone run and
isolated-PATH package checks are the substitute. No public release was possible
because the repository has no remote.
