# Task 047 — Complete independent release verification

| Field | Value |
|---|---|
| ID | 047 |
| Stage | Post-release verification |
| Depends on | 037, 038 |
| Estimate | 0.5–1 day plus CI runtime |
| Priority | P2 |
| Status | Ready |

## Objective

Close verification gaps that could not be exercised during the local 0.1.0
release: hosted CI, MSVC, a separate physical clean machine, broader Linux ABI
coverage and a captured GUI transcript. This task does not add engine features
and does not retroactively change the 0.1.0 source or artifacts.

## Background

The 0.1.0 tag is local because the repository has no remote. The final local
matrix passed from the working tree and from a fresh local clone, including
Windows Debug/Release, Ubuntu Release, Linux ASan/UBSan, deep perft depth 5–6,
the 1,000-game protocol run and extracted-archive verification. Those results
are recorded in `docs/RELEASE_VALIDATION.md`.

## Scope

1. Configure a remote and run `.github/workflows/release.yml` on the `v0.1.0`
   tag. Confirm the sanitizer and both package matrix jobs pass and attach the
   generated JUnit reports.
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

- [ ] Hosted CI is green for the tag and the JUnit reports are attached.
- [ ] MSVC build/test result is recorded.
- [ ] Separate physical clean-machine build/test/package result is recorded.
- [ ] The supported Linux glibc baseline is stated and verified, or explicitly
      limited.
- [ ] A GUI transcript is archived with no illegal move, hang or forfeit.
- [ ] Any strength experiment has a recorded protocol and uncertainty, or is
      explicitly left undone.

## Constraints

- Do not move or re-tag `v0.1.0`; publish a new patch version if source changes.
- Do not claim hosted CI, MSVC or clean-machine results before they exist.
- Do not change default engine behavior in this verification task.
