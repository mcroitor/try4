# Release engineering

Stage 8 consists of tasks 036–038. Version **0.1.0** was tagged locally as
`v0.1.0` on 2026-10-26 after the final gate matrix; the repository has no remote,
so the tag and archives are unpublished. CMake's `project(... VERSION ...)` is
the single version source. Its generated header supplies both `--version` and
UCI `id name`. Actual observations and checksums are in
[`RELEASE_VALIDATION.md`](RELEASE_VALIDATION.md).

## Build, test, install, package

From the repository root (commands work with single- and multi-config CMake):

```sh
cmake -S . -B build/release-check -DCMAKE_BUILD_TYPE=Release -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/release-check --config Release --parallel 2
ctest --test-dir build/release-check -C Release --output-on-failure --output-junit regression.xml
cmake --install build/release-check --config Release --prefix build/install
cpack --config build/release-check/CPackConfig.cmake -C Release
```

Archives are written under `build/release-check/packages/`, for example
`try4-0.1.0-windows-x86_64.zip` and `try4-0.1.0-linux-x86_64.zip`, each accompanied
by `<archive>.sha256`. ZIPs contain the UCI engine, debug CLI, license, notices,
and release/usage documents. They exclude opening books, tests, and test data.

Verify an archive (Python 3, standard library only):

```sh
python scripts/verify_release.py build/release-check/packages/try4-0.1.0-windows-x86_64.zip
```

The script checks SHA-256, extracts to a temporary directory, runs both version
commands and a UCI search/stop handshake with compiler directories removed from
PATH, and checks package contents. This is an isolated-directory smoke test;
it does not replace clean-machine verification.

Windows defaults to static C++ runtimes (`TRY4_PORTABLE_WINDOWS=ON`): `/MT` for
MSVC and static GCC/libstdc++/winpthread linkage for MinGW. Linux uses the host
system ABI; build on the oldest Linux distribution you intend to support.
No host-specific `-march=native` flags or build timestamps are added. Archive
bytes may still vary with toolchain and ZIP timestamps: checksums identify
specific tested artifacts, not a guarantee of bit-for-bit reproducibility.

## Automation

`.github/workflows/release.yml` runs on `v*` tags or manual dispatch. It runs
Linux sanitizers, tests Windows/Linux Release builds, creates packages, verifies
extracted executables/checksums, and uploads workflow artifacts and regression
reports. The verifier rejects tags inconsistent with the CMake version.
The workflow creates candidate artifacts; it does not publish a GitHub release.

## Final release gate

Task 038 requires evidence for all of these on the final source:

- Full CTest suite on Windows and Linux, including UCI/books/package smoke.
- Linux ASan/UBSan with no findings (see `HARDENING.md`).
- Deep perft reference checks at release depths using `scripts/perft_deep.ps1`
  (depth 5 and depth 6 for all six positions for 0.1.0).
- Recorded 1,000-game UCI stability run with time control, engine version,
  hardware, no crashes/hangs/illegal moves/time forfeits, and stop latency.
- Installed/archive executables verified from a clean checkout and an isolated
  directory. A separate physical clean machine is preferred; for 0.1.0 a fresh
  local clone plus removed compiler `PATH` entries was the available substitute.
- All P0/P1 issues resolved, including task 039's UCI conformance pass.
- MIT license, changelog, release notes and contribution instructions present.

The 0.1.0 matrix passed those gates. Hosted CI, MSVC and a separate physical
clean machine remain documented limitations because no remote was configured and
those environments were unavailable. Never describe the local tag as a public
release.

To repeat the clean-checkout gate:

```sh
git clone <local-repository> try4-clean
cd try4-clean
git checkout v0.1.0
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/release --config Release --parallel 2
ctest --test-dir build/release -C Release --output-on-failure
cpack --config build/release/CPackConfig.cmake -C Release
python scripts/verify_release.py build/release/packages/try4-0.1.0-*.zip
```

For a Linux sanitizer build:

```sh
cmake -S . -B build/asan -DCMAKE_BUILD_TYPE=Debug -DTRY4_WARNINGS_AS_ERRORS=ON -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build/asan --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build/asan --output-on-failure
```

Archive commands, reports, compiler versions, hardware and checksums with the
candidate. Update `RELEASE_VALIDATION.md` with actual observations.
Once every gate passes, commit the tested source and tag `v0.1.0` on that
commit. Never tag uncommitted working-tree changes or a failing candidate.
