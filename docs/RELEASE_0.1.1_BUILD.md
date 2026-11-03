# try4 0.1.1 — local Windows and Linux packages

Built on 2026-11-03 from `81dfa2d22860145f58d3c9d652b511c3171fe731`
plus the working-tree executable rename and packaging/documentation changes.
These are local candidate artifacts, not a tagged or published release.

## Artifacts

The upload bundle is in `build/releases/0.1.1/`. Both archives contain
`bin/try4` (the UCI engine) and `bin/try4_cli` (the debugging console), with
`.exe` suffixes on Windows, plus documentation, configuration and licenses.
Opening books and Syzygy data are external.

| Archive | SHA-256 |
|---|---|
| `try4-0.1.1-windows-x86_64.zip` | `3bf20f62ffd40d4b0ff39d19cb494c841cd622d58e6bacb8fbc290f7bd0b2e3a` |
| `try4-0.1.1-linux-x86_64.zip` | `f4df7e14ab60feffe81990bd65aa34182ec73b5c7d5c6aca67fed2ed83b21707` |

Adjacent `.sha256` files identify the exact archives. The bundle also includes
platform-specific JUnit reports, test logs, build caches, compiler metadata,
the working-tree diff and release notes.

## Verification

- Windows Release: **30/30 CTest targets passed**, 13.48 seconds.
- Ubuntu/WSL2 Release: **30/30 CTest targets passed**, 30.79 seconds, with
  `TRY4_SYZYGY_PATH=/mnt/c/games/see/Syzygy345` enabling tablebase tests.
- Both ZIPs passed `scripts/verify_release.py`, including SHA-256, extracted
  version checks, UCI handshake/search/stop, configuration and relative-book
  behavior, reduced-PATH execution, documentation and data-exclusion checks.
- Packaging verification now filters executable matches to files, so Linux's
  `share/doc/try4` directory is not mistaken for an additional engine.

Both builds use Release mode, warnings as errors, tests enabled and
`TRY4_SMP_POC=OFF`. Windows uses `TRY4_PORTABLE_WINDOWS=ON` and MSYS2 UCRT GCC
16.2.0 with static compiler runtimes. Linux uses GCC 15.2.0 on Ubuntu 26.04.1
under WSL2, glibc 2.43. It dynamically requires `libstdc++.so.6`,
`libgcc_s.so.1` and `libc.so.6`; the engine's recorded symbol requirements reach
`GLIBC_2.34`, `GLIBCXX_3.4.32` and `CXXABI_1.3.9`. Older-distribution
compatibility is not verified.

## Reproduction

Windows build directory: `build/task047-release`; Linux build directory:
`build/task047-linux`. Both were reconfigured and rebuilt after the rename.

```sh
cmake -S . -B build/release-check -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTRY4_BUILD_TESTS=ON -DTRY4_WARNINGS_AS_ERRORS=ON \
  -DTRY4_PORTABLE_WINDOWS=ON -DTRY4_SMP_POC=OFF
cmake --build build/release-check --parallel 4
ctest --test-dir build/release-check -C Release --output-on-failure
cpack --config build/release-check/CPackConfig.cmake -C Release
python scripts/verify_release.py build/release-check/packages/<archive>.zip
```

For Linux use `python3`; external tablebase tests can be enabled with
`TRY4_SYZYGY_PATH`. See [the candidate report](RELEASE_CANDIDATE.md) for
publication gates. This build did not rerun hosted CI or sanitizers, perform
separate physical clean-machine verification, tag source, or publish to GitHub.
