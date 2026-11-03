# try4 0.1.1 release candidate — task 047

**Latest local packages:** the executable-rename rebuild, current archive hashes
and verification results are in [RELEASE_0.1.1_BUILD.md](RELEASE_0.1.1_BUILD.md).
The original local archive hashes below refer to the earlier pre-rename build.

**Updated:** 2026-11-03. **Status:** In progress; not tagged or published.
**Branch:** `feat/047`. **Merged baseline:**
`e6d6b07fcf13fdffd516fa32afaa64f8af376699` (PR #11).

The candidate changes the CMake version to 0.1.1, reconciles task/release
documents, improves release-workflow evidence collection and extends package
verification for Syzygy documentation, licensing and external-data exclusion. Local builds in
this report are from that baseline plus the uncommitted preparation changes;
they are not an immutable, independently verified release. Replace this
description with the committed candidate SHA when the hosted release workflow
has run. The original 0.1.0 evidence is preserved in
[`RELEASE_VALIDATION.md`](RELEASE_VALIDATION.md).

## Hosted baseline evidence

[CI run 41582984398](https://github.com/mcroitor/try4/actions/runs/41582984398)
completed successfully on the merged baseline:

| Job | Result | Evidence |
|---|---|---|
| Ubuntu Debug, warnings as errors | Pass | [Job 125741837424](https://github.com/mcroitor/try4/actions/runs/41582984398/job/125741837424) |
| Windows Debug, MSVC, warnings as errors | Pass | [Job 125741837387](https://github.com/mcroitor/try4/actions/runs/41582984398/job/125741837387) |
| Ubuntu ASan/UBSan | Pass | [Job 125741837221](https://github.com/mcroitor/try4/actions/runs/41582984398/job/125741837221) |

These are ordinary CI jobs, not the Release artifacts workflow. They verify
the recent bitboard and MSVC environment-reading fixes on the merged source.
No tags or GitHub releases were returned by the public GitHub API, and
`git tag` returned no local tags on 2026-11-03. Earlier reports of a local
`v0.1.0` tag describe a prior checkout; that ref is not available here.

## Local candidate verification

Environment: Intel Core i5-13600HX, Windows 11 build 26200, MSYS2 UCRT GCC
16.2.0, CMake 4.4.3, Python 3.12.10. Both builds use warnings as errors,
`TRY4_BUILD_TESTS=ON`, `TRY4_PORTABLE_WINDOWS=ON` and `TRY4_SMP_POC=OFF`.

| Check | Result | Report |
|---|---|---|
| Windows Debug full CTest | **30/30 passed**, 120.56 s | `build/cmake/task047-debug.xml` |
| Windows Release full CTest (new build directory) | **30/30 passed**, 13.32 s | `build/task047-release/regression.xml` |
| Ubuntu/WSL2 Release full CTest (new build directory) | **30/30 passed**, 11.89 s | `build/task047-linux/regression.xml` |
| Windows install, ZIP and reduced-PATH extracted verifier | **Pass** | `build/task047-release/packages/` |
| Linux ZIP and reduced-PATH extracted verifier | **Pass** | `build/task047-linux/packages/` |
| Windows Syzygy data-dependent tests | **9/9 cases passed**, 86 assertions | Local `c:\games\see\Syzygy345` fallback |
| Linux Syzygy with external data | **9/9 cases passed**, 86 assertions, 25.44 s | `build/task047-linux/syzygy-data.xml` |
| Package-verifier negative checks | **3/3 passed** | Missing Syzygy docs, missing Fathom license, bundled `.rtbw` each rejected |
| Release workflow YAML / matrix / report paths | **Pass** (local structural check) | Hosted execution still pending |
| Changed Markdown relative links and whitespace | **Pass** | Local validation |

Commands executed from the repository root:

```powershell
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug `
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON -DTRY4_SMP_POC=OFF
cmake --build build/cmake --config Debug --parallel 4
ctest --test-dir build/cmake -C Debug --output-on-failure `
  --output-junit "C:/projects/git/try4/build/cmake/task047-debug.xml"

cmake -S . -B build/task047-release -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON `
  -DTRY4_PORTABLE_WINDOWS=ON -DTRY4_SMP_POC=OFF `
  -DFETCHCONTENT_SOURCE_DIR_CATCH2=C:/projects/git/try4/build/cmake/_deps/catch2-src
cmake --build build/task047-release --config Release --parallel 4
ctest --test-dir build/task047-release -C Release --output-on-failure `
  --output-junit "C:/projects/git/try4/build/task047-release/regression.xml"
cmake --install build/task047-release --config Release --prefix build/task047-install
cpack --config build/task047-release/CPackConfig.cmake -C Release
python scripts/verify_release.py build/task047-release/packages/try4-0.1.1-windows-x86_64.zip
```

The Catch2 override reuses the already-fetched v3.5.2 source; a clean networked
checkout can omit it. The verifier checks SHA-256, both version commands,
extracted UCI search/stop, configuration/relative-book behavior and reduced
compiler PATH. Generated reports and binaries remain under ignored `build/`
directories. This evidence report is kept beside the candidate in the repository,
not inside the runtime ZIP, so archive hashes can be recorded without a
self-referential package checksum.

### Linux environment and reproduction

Ubuntu **26.04.1 LTS** on WSL2 kernel `6.18.54.1-microsoft-standard-WSL2`, GCC
15.2.0, CMake 4.2.3, Python 3.14.4 and glibc **2.43**. The Linux suite initially
passed all 30 targets with seven data-dependent Syzygy cases skipped; the
separate data-enabled run passed all nine Syzygy cases without skips.

Commands executed inside Ubuntu, from `/mnt/c/projects/git/try4`:

```bash
cmake -S . -B build/task047-linux -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON -DTRY4_SMP_POC=OFF \
  -DFETCHCONTENT_SOURCE_DIR_CATCH2=/mnt/c/projects/git/try4/build/cmake/_deps/catch2-src
cmake --build build/task047-linux --parallel 4
ctest --test-dir build/task047-linux --output-on-failure \
  --output-junit /mnt/c/projects/git/try4/build/task047-linux/regression.xml
TRY4_SYZYGY_PATH=/mnt/c/games/see/Syzygy345 \
  ctest --test-dir build/task047-linux -R '^try4_syzygy$' --output-on-failure \
  --output-junit /mnt/c/projects/git/try4/build/task047-linux/syzygy-data.xml
cpack --config build/task047-linux/CPackConfig.cmake -C Release
python3 scripts/verify_release.py build/task047-linux/packages/try4-0.1.1-linux-x86_64.zip
objdump -p build/task047-linux/try4_uci
objdump -p build/task047-linux/try4
```

**Linux support boundary:** this local ZIP is verified only on the above
Ubuntu 26.04.1/glibc 2.43 environment. Both executables dynamically require
`libstdc++.so.6`, `libgcc_s.so.1` and `libc.so.6`; their highest recorded symbol
requirements are `GLIBC_2.34`, `GLIBCXX_3.4.32` and `CXXABI_1.3.9`.
Those symbols alone do not prove compatibility with an older distribution;
no older-glibc or older-libstdc++ support is claimed. Hosted packages must record
their own runner/runtime requirements and be verified on their declared target.

### Local candidate archives

| Archive | Bytes | SHA-256 |
|---|---:|---|
| `try4-0.1.1-windows-x86_64.zip` | 1472908 | `df6b473de50df96db61b7cf1a0687c1f3843832d4118c6fcafdd6e025173d0c1` |
| `try4-0.1.1-linux-x86_64.zip` | 211797 | `ae81d16f07be26a79317356f51f8c47f1df6ecefd49ed3399b33c5f26393ae23` |

Paths: `build/task047-release/packages/` and `build/task047-linux/packages/`.
Adjacent `.sha256` files identify these exact local archives; a hosted rebuild
will have its own checksum. No opening or tablebase data is included. Both ZIPs
contain the updated notes and release documents and pass the enhanced verifier.

## Task 047 evidence matrix

| Requirement | State | Next action |
|---|---|---|
| Hosted Ubuntu/Windows/MSVC/sanitizer baseline CI | Pass for `e6d6b07` | Preserve linked run; rerun on candidate commit |
| Candidate Release workflow, both packages and JUnit reports | Pending | Push committed `feat/047`; manually dispatch Release artifacts for that SHA |
| Separate physical clean-machine package verification | Pending | Copy the exact ZIP/sidecar to another machine without a compiler in PATH; record output |
| Supported Linux ABI baseline | Explicitly limited for local ZIP | Ubuntu 26.04.1/glibc 2.43 only, symbol requirements above; hosted ZIP target verification pending |
| Archived real GUI game transcript | Pending | Record CuteChess/Arena options, clock, game, termination and engine log, including resignation handling |
| Strength claim | Not made | Eight-game TSCP sample does not satisfy the planned 200-game S3 gate |
| Tag and public release | Pending | Complete gates, tag `v0.1.1`, verify tag workflow, publish exact tested artifacts |

Task 047 remains **In progress**. Hosted runner builds do not substitute for
the separate physical clean-machine requirement.

## Candidate reproduction and publication

See [`RELEASE.md`](RELEASE.md) for build/test/package commands and workflow
dispatch. Use `TRY4_BUILD_TESTS=ON`, `TRY4_WARNINGS_AS_ERRORS=ON`,
`TRY4_PORTABLE_WINDOWS=ON` and `TRY4_SMP_POC=OFF` for the release matrix.

1. Commit and push the candidate branch; record the exact SHA here.
2. Dispatch **Release artifacts** on `feat/047` from an authenticated client.
3. Download `try4-asan`, `try4-regression-<runner>` and `try4-<runner>` artifacts.
   Preserve JUnit, `LastTest.log`, source/toolchain provenance, ZIPs and SHA-256
   sidecars with the candidate.
4. Verify the exact downloaded archives on a separate clean machine and the
   declared Linux ABI target; archive a real GUI game transcript.
5. Update this matrix with actual observations. Tag the final tested commit as
   `v0.1.1` only after all required gates pass. Do not move `v0.1.0`.
6. Publish a GitHub release with matching notes, archives, sidecars and reports.

The current shell cannot push through `origin` because SSH public-key
authentication fails, and GitHub CLI is not installed. Public API evidence is
readable. Remote dispatch and publication remain pending authenticated access.
