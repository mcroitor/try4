# Release engineering

The current candidate is **0.1.1**, prepared under task 047 on `feat/047`.
CMake's `project(... VERSION ...)` is the single version source. Its generated
header supplies both `--version` and UCI `id name`. Current observations and
remaining gates are in [`RELEASE_CANDIDATE.md`](RELEASE_CANDIDATE.md).

Tasks 036–038 and [`RELEASE_VALIDATION.md`](RELEASE_VALIDATION.md) record the
historical 0.1.0 local validation. GitHub now hosts the source and CI; no local
or remote tags or GitHub releases were present on 2026-11-03. Do not recreate or
move the historical `v0.1.0` tag to point at newer source.

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
`try4-0.1.1-windows-x86_64.zip` and `try4-0.1.1-linux-x86_64.zip`, each accompanied
by `<archive>.sha256`. ZIPs contain the UCI engine, debug CLI, license, notices,
and release/usage documents. They exclude opening books, tests, and test data.

Verify an archive (Python 3, standard library only):

```sh
python scripts/verify_release.py build/release-check/packages/try4-0.1.1-windows-x86_64.zip
```

The script checks SHA-256, extracts to a temporary directory, runs both version
commands and a UCI search/stop handshake with compiler directories removed from
PATH, and checks package contents. This is an isolated-directory smoke test;
it does not replace clean-machine verification.

Package-content checks require Syzygy setup documentation and the Fathom MIT
notice, and reject bundled `.rtbw`/`.rtbz` files as well as opening data.

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
reports. JUnit reports and test logs are retained even when a test fails;
successful package artifacts include source SHA, CMake/toolchain configuration
and Linux runner glibc provenance. The verifier rejects tags inconsistent with
the CMake version. Ordinary `ci.yml` success is Debug/sanitizer evidence, not
Release-package verification.
The workflow creates candidate artifacts; it does not publish a GitHub release.

After the candidate is committed and pushed, select **Actions → Release
artifacts → Run workflow → feat/047** to verify that exact commit before
tagging. Record the workflow URL, source SHA, JUnit counts, archive hashes and
platform/toolchain details in the candidate report. The current environment's
SSH push authentication is unavailable, so remote dispatch must be performed
from an authenticated client.

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

The historical 0.1.0 local matrix passed its recorded gates. Hosted Ubuntu,
Windows/MSVC and ASan/UBSan CI now pass for `e6d6b07`, but those results do not
certify the later 0.1.1 candidate or its packages. Task 047 still requires the
candidate Release workflow, separate clean-machine evidence, a supported Linux
ABI baseline and an archived GUI game. Do not describe a candidate as released.

To repeat the clean-checkout gate:

```sh
git clone https://github.com/mcroitor/try4.git try4-clean
cd try4-clean
git checkout <candidate-commit-sha>
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/release --config Release --parallel 2
ctest --test-dir build/release -C Release --output-on-failure
cpack --config build/release/CPackConfig.cmake -C Release
python scripts/verify_release.py build/release/packages/<exact-archive-name>.zip
```

For a Linux sanitizer build:

```sh
cmake -S . -B build/asan -DCMAKE_BUILD_TYPE=Debug -DTRY4_WARNINGS_AS_ERRORS=ON -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build/asan --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build/asan --output-on-failure
```

Archive commands, reports, compiler versions, hardware and checksums with the
candidate. Update `RELEASE_VALIDATION.md` with actual observations.
Once every gate passes, commit the tested source and tag `v0.1.1` on that
commit, push the tag, and verify the tag-triggered workflow. Publishing a GitHub
release with those exact archives, sidecars and reports is a separate step.
Never tag uncommitted working-tree changes or a failing candidate.
