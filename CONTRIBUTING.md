# Contributing

Use C++17 and the existing `chess` APIs and coding style. Read the relevant
task in `tasks/` before starting. Keep unrelated changes separate and add
regression coverage for behavior changes.

```sh
cmake -S . -B build/dev -DCMAKE_BUILD_TYPE=Debug -DTRY4_WARNINGS_AS_ERRORS=ON
cmake --build build/dev --config Debug
ctest --test-dir build/dev -C Debug --output-on-failure
```

The first test-enabled configure fetches Catch2 v3.5.2. An existing offline
checkout can be supplied with `-DFETCHCONTENT_SOURCE_DIR_CATCH2=/path/to/Catch2`.
Build/test commands and package verification must pass before merging.

For bugs, include the version (`try4_uci --version`), operating system,
compiler/build mode, FEN, exact UCI command sequence, and expected/actual output.
Report timing failures with the time control and engine log. Performance or
strength changes need reproducible comparisons; avoid claims based on a small
self-play sample.

Contributions to the project code are provided under the [MIT license](LICENSE).
Identify the source and license of any external code or data. Opening books are
not bundled in runtime packages. See [third-party notices](THIRD_PARTY_NOTICES.md).
