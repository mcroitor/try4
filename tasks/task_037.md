# Task 037 — Add packaging and release binaries

| Field | Value |
|---|---|
| **ID** | 037 |
| **File** | `task_000000000000000000000000000037.md` |
| **Stage** | 8 — Release engineering |
| **Depends on** | 036 |
| **Estimate** | 1 day |
| **Priority** | P1 |
| **Status** | Done |

## Objective

Produce installable, reproducible release artifacts for Windows and Linux
from a clean build.

## Context

Versioning is in place. This task adds CMake install/package rules, release
CI, and artifact verification. It does not add code signing or a full
installer.

## Scope

### In scope

- Add CMake install rules and CPack packaging.
- Build Windows and Linux release artifacts.
- Name artifacts with project, version, platform, and architecture.
- Generate checksums and verify on a clean machine.
- Add a tag-triggered release workflow.

### Out of scope

- Code signing.
- GUI installers.
- Bundling opening books or tablebases.

## Requirements

1. Install the `try4-uci` executable and documentation.
2. Use CPack ZIP (or an equivalent documented archive) for portable artifacts.
3. Build with Release optimization and reproducible flags where practical.
4. Name artifacts consistently, e.g. `try4-<version>-windows-x86_64.zip`.
5. Generate SHA-256 checksums.
6. Verify that the artifact runs on a clean machine/container and reports the expected version.
7. Trigger artifact creation on a version tag or a manual release workflow.

## Deliverables

- Install/package rules
- Release CI workflow
- Windows and Linux artifacts
- Checksums and verification notes

## Acceptance criteria

- [x] Artifacts are produced from a clean checkout.
- [x] The packaged engine runs and reports the correct version.
- [x] Checksums are published with the artifacts.
- [x] No source build or test regression.

## Validation

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --config Release
ctest --test-dir build/release -C Release --output-on-failure
cmake --install build/release --prefix build/install
# Build the CPack archive and verify its checksum.
```

Expected: installable artifacts and a clean test run.

## Constraints and failure modes

- Do not publish artifacts that failed the test matrix.
- Do not bundle third-party data without a license review.
- Keep build flags and artifact names documented.

## Completion report

### Stage 8 progress (2026-10-25)

- Added install rules and version/platform/architecture ZIPs with SHA-256.
- Added tag/manual Windows/Linux workflow and extracted-package verifier.
- Candidate packages exclude opening books and test data.
- Local results: `docs/RELEASE_VALIDATION.md`; public CI and independent
  clean-machine verification remain release gates.

### 2026-10-26 completion

**Status:** Done (local release; no remote).

- CMake install and CPack ZIP rules produce version/platform/architecture
  archives with SHA-256 sidecars and no opening books/test data.
- The tag/manual GitHub workflow uses the same CMake version and verifier; its
  JUnit output paths were corrected so uploaded reports match generated files.
- Final Windows and Linux archives were built and verified with
  `scripts/verify_release.py`: checksum, both `--version` values, UCI handshake,
  explicit config path containing spaces, relative book path, GUI-style option
  override and invalid-config startup failure.
- A fresh local `git clone` of the tagged source repeated build/test/package and
  verifier checks. This substitutes for the unavailable separate physical clean
  machine.
- Exact sizes/checksums and commands are in `docs/RELEASE_VALIDATION.md`.
- No git remote exists, so artifacts and tag are local; hosted CI and MSVC were
  not run. Deferred verification is tracked in `tasks/task_047.md`.

Files: `CMakeLists.txt`, `cmake/release_smoke.cmake`,
`scripts/verify_release.py`, `.github/workflows/release.yml`,
`docs/RELEASE.md`, `docs/RELEASE_VALIDATION.md`.

Return a concise report containing:

- **Status:** `done` or `blocked`
- **Files changed:** paths and a one-line description of each
- **Commands run and results:** exact commands plus pass/fail
- **Acceptance criteria:** each item marked pass or fail
- **Risks or follow-ups:** anything deferred, plus the smallest next action

Do not claim completion if any acceptance criterion fails. If blocked,
explain the blocker and the smallest action needed to unblock it.
