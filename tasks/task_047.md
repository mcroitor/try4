# Task 047 — Complete independent release verification

| Field      | Value                     |
| ---------- | ------------------------- |
| ID         | 047                       |
| Stage      | Post-release verification |
| Depends on | 037, 038                  |
| Estimate   | 0.5–1 day plus CI runtime |
| Priority   | P2                        |
| Status     | In progress               |

## Objective

Close verification gaps that could not be exercised during the local 0.1.0
release: hosted CI, MSVC, a separate physical clean machine, broader Linux ABI
coverage and a captured GUI transcript. This task does not add engine features
and does not retroactively change the 0.1.0 source or artifacts.

## Background

The historical 2026-10-26 report records a local 0.1.0 tag and no remote. The final local
matrix passed from the working tree and from a fresh local clone, including
Windows Debug/Release, Ubuntu Release, Linux ASan/UBSan, deep perft depth 5–6,
the 1,000-game protocol run and extracted-archive verification. Those results
are recorded in `docs/RELEASE_VALIDATION.md`.

**Reconciled 2026-11-03:** source is hosted on GitHub and merged `e6d6b07`
passes Ubuntu, Windows/MSVC and ASan/UBSan CI. No local/remote tags or GitHub
releases are currently present. This task prepares **0.1.1** on `feat/047`
because the newer engine includes post-0.1.0 changes; it does not re-tag old
source. Current evidence and publication steps are in
[`docs/RELEASE_CANDIDATE.md`](../docs/RELEASE_CANDIDATE.md).

## Scope

1. Run `.github/workflows/release.yml` manually on the committed 0.1.1 candidate,
   then on its final `v0.1.1` tag. Confirm sanitizer and both package matrix jobs
   pass; archive the JUnit reports, source/toolchain provenance and checksums.
2. Build and run the full suite with MSVC on Windows in addition to the tested
   MinGW/GCC build.
3. Repeat build/test/package/verify on a separate physical clean machine, with
   the compiler toolchain absent from `PATH`.
4. If older Linux distributions are a support target, build and verify the
   archive on an older glibc host, or state a minimum glibc version.
5. Capture a real GUI game transcript (CuteChess or Arena) with options,
   time control, engine log and a normal game finish; add it to the release
   evidence.
6. Optional: run a paired-game experiment for configurable evaluation or
   forcing search only with a predeclared sample size and uncertainty interval.
   Do not present a few games as a strength result.

## Acceptance criteria

- [ ] Hosted Release CI is green for the candidate/tag and JUnit reports are attached.
- [x] MSVC Debug build/test result is recorded (merged `e6d6b07`, run 41582984398); candidate Release coverage remains pending.
- [ ] Separate physical clean-machine build/test/package result is recorded.
- [x] Local Linux ZIP support is explicitly limited to tested Ubuntu 26.04.1/WSL2 with glibc 2.43; GLIBC/GLIBCXX/CXXABI requirements are recorded in the candidate report. Hosted ZIPs require their own ABI evidence.
- [ ] A GUI transcript is archived with no illegal move, hang or forfeit.
- [x] No candidate strength claim: longer paired-game validation is explicitly left undone; the historical eight-game TSCP sample is not the 200-game S3 gate.

## Constraints

- Do not move or re-tag `v0.1.0`; publish a new patch version if source changes.
- Do not claim hosted CI, MSVC or clean-machine results before they exist.
- Do not change default engine behavior in this verification task.

## Progress report — 2026-11-03

- Created `feat/047`; reconciled completed task reports and task 058's status.
- Prepared CMake version 0.1.1, candidate notes and a current evidence matrix.
- Release workflow retains failure-time JUnit/logs and successful-package source
  and toolchain provenance; SMP remains disabled in release packages.
- Hosted baseline CI is green on Ubuntu, Windows/MSVC and Linux ASan/UBSan.
- Local candidate test/package results are recorded in `docs/RELEASE_CANDIDATE.md`.
- Windows Debug, Windows Release and Ubuntu/WSL2 Release pass **30/30 CTest
  targets each**. Both ZIPs pass SHA-256/extracted-package verification; optional
  Syzygy cases pass 9/9 with external data on both platforms. Local Linux
  support is explicitly limited to the tested Ubuntu/glibc/runtime environment.
- **Open:** candidate hosted Release workflow, tag/publication, separate physical
  clean machine, hosted-package Linux ABI evidence and GUI transcript. SSH push
  authentication is unavailable in this shell; dispatch needs an authenticated
  client. This task is not Done.
