# Performance Baseline

Perft is the Stage 2 correctness gate and the first repeatable performance
measurement. The numbers below are a baseline, not a target: they were captured
before any search or evaluation work.

## Environment

| Field | Value |
|---|---|
| Date | 2026-10-23 |
| CPU | 13th Gen Intel Core i5-13600HX (14 cores / 20 logical processors, 2.6 GHz base) |
| OS | Windows 11 Pro |
| Compiler | g++ (MSYS2) 16.2.0 |
| Build system | CMake 4.4.3, Ninja 1.13.2 |
| Build type | `Release` (`-O3 -DNDEBUG`) |
| Warnings | `-Wall -Wextra -Wpedantic -Werror` |

The measurements are single-threaded and use the `try4_perft_bench` executable.

## Results

| Position | Depth | Nodes | Time | Nodes/second |
|---|---:|---:|---:|---:|
| Start | 5 | 4,865,609 | 204 ms | 23,851,025 |
| Kiwipete | 4 | 4,085,603 | 299 ms | 13,664,224 |
| Kiwipete | 5 | 193,690,690 | 19,569 ms | 9,897,833 |

The start position has fewer legal moves and simpler branches, so its NPS is
higher than Kiwipete's. These numbers include full legal-move filtering and
make/unmake for every node.

## Reproduce

Build the optimized benchmark:

```sh
cmake -S . -B build/cmake-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DTRY4_BUILD_TESTS=OFF \
  -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/cmake-release --config Release
```

Run one position:

```sh
./build/cmake-release/try4_perft_bench "<FEN>" 5
```

Run the six reference positions at depth 5 (or 6) and compare every count:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/perft_deep.ps1 `
  -Exe build/cmake-release/try4_perft_bench.exe `
  -Depth 5
```

Use `-Depth 6` for the full deep run and `-Position kiwipete` to run a single
position. Depth 6 takes substantially longer and is intentionally excluded from
default CI.

The normal CI gate remains depths 1–4 for all six positions:

```sh
ctest --test-dir build/cmake -C Debug -L perft --output-on-failure
```
