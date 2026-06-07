# System Spec: GoogleTestSpeed

**Parent:** @docs/specs/applications/googletests.md
**Research:** @docs/research/google_test_suite_speed/summary.md

## Purpose

GoogleTestSpeed owns the tooling, build configuration, and test-code conventions that keep the GoogleTests suite fast. It covers every layer of the test execution stack — compile time (PCH), runner configuration (Release config, DiaCLI flags), test categorisation (slow-suite tagging), parallelism (shard runner), and test-code hygiene (fixture amortisation) — with the goal of driving the full-suite wall-clock time from ~5 minutes to under 90 seconds.

## Responsibilities

- Define and enforce the `SLOW_` suite-prefix convention and the DiaCLI default filter that excludes those suites during development
- Own the PCH configuration for `gtest.h` and a stable set of Dia headers in `GoogleTests.vcxproj`
- Own the `--shards N` DiaCLI flag that splits the suite across N parallel processes
- Define the CI full-suite configuration default (Release)
- Own the fixture-amortisation pattern guide and apply it to `Py_Initialize` and other expensive per-suite setup calls

## Public Interfaces

### DiaCLI surface

| Flag / Command | Behaviour |
|---|---|
| `dia run googletest` | Builds + runs; excludes `SLOW_*` suites by default |
| `dia run googletest --all` | Runs including `SLOW_*` suites |
| `dia run googletest --shards N` | Splits suite into N shards, runs in parallel, merges results |
| `dia run googletest --config Release` | Runs in Release config (becomes CI default for full suite) |

### Conventions (data contract with test authors)

- Suites whose median wall-clock time exceeds **500 ms** must be prefixed `SLOW_`
- Expensive one-time setup (interpreter init, large file loads) must be in `SetUpTestSuite` / `TearDownTestSuite`, not in per-test `SetUp`

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| slow-suite-tagging | `SLOW_` prefix convention + DiaCLI default filter; developers see fast feedback, CI runs all | TBD | Draft |
| release-config-ci | Switch CI full-suite default to `--config Release`; 3–5× runtime reduction for math/physics tests at zero code cost | TBD | Draft |
| precompiled-header | PCH for `gtest.h` + stable Dia headers in `GoogleTests.vcxproj`; scope limited to headers that never change | TBD | Draft |
| shard-runner | `dia run googletest --shards N` splits filter set across N `--gtest_filter` invocations, runs in parallel, merges XML results | TBD | Draft |
| fixture-amortisation | Move `Py_Initialize` / `Py_Finalize` and other expensive calls from `SetUp` to `SetUpTestSuite`; apply across Python/ suite first | TBD | Draft |

## Platform Primitives Used

- DiaCLI (flag parsing, process invocation, output merging)
- MSBuild / `GoogleTests.vcxproj` (PCH, Release config)
- Google Test framework (filter flags, XML output, `SetUpTestSuite`)
- DiaPython (fixture amortisation target)

## Dependencies on Other Systems

- **DiaCLI** — all runner changes live in the `dia run googletest` command path
- **DiaPipeline** — CI full-suite run triggers via `dia pipeline`; Release config must be a recognised pipeline config

## Out of Scope

- Per-module test binaries (C8) — deferred; `dia scaffold` cannot yet generate test projects automatically
- Dirty-module tracking (C7) — deferred; requires `SLOW_` labelling to be in place first
- Google Benchmark / performance regression tests — separate concern outside unit testing
- ASAN / UBSan configs — must be left untouched; all five features must leave sanitiser pipelines unaffected
- Test parallelism within a single process (thread-level) — Google Test does not support this; sharding is the correct mechanism

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | Implement features in dependency order: slow-suite-tagging → release-config-ci → precompiled-header → shard-runner → fixture-amortisation | C2 (tagging) is the keystone; sharding works better with categorised suites; later features assume earlier ones are live | All features | Accepted | Yes |
| SD-002 | PCH must include only headers that are stable across the whole tree | A PCH including unstable headers causes all 404 files to recompile on any header touch — net negative | precompiled-header | Accepted | Yes |
| SD-003 | `SLOW_` threshold is 500 ms median wall-clock per suite | Gives developers fast round-trips; keeps CI reliable without arbitrary exclusions | slow-suite-tagging | Accepted | Yes |
| SD-004 | `--shards N` defaults to logical core count minus 1 when N is omitted | Saturates available parallelism without starving the OS; user can override | shard-runner | Accepted | No |

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|-----------------------------|
| PD-004 | Platform | No STL containers in public APIs | Any new C++ helper code (e.g. shard merger) must use DiaCore containers, not STL |
| PD-005 | Platform | x64 is the only supported build target | PCH and Release config changes apply to x64 only — no 32-bit variants needed |
| PD-006 | Platform | Visual Studio project files are source of truth | PCH changes must be made in `GoogleTests.vcxproj`; no external build scripts may override |
| PD-007 | Platform | C++20 required | PCH must compile cleanly under `/std:c++20` |
| PD-008 | Platform | `Directory.Build.props` owns OutDir / IntDir | Release config output will resolve automatically; no `.vcxproj` overrides permitted |
| PD-009 | Platform | Generated output under `Cluiche/out/<AppName>/` | Shard XML results and merged reports must land under `Cluiche/out/GoogleTests/` |
| AD-001 | GoogleTests | Google Test framework exclusively | Shard runner must still invoke GTest binaries with `--gtest_filter`; no alternative test runners |
| AD-003 | GoogleTests | Dirty tracking is opt-in | `--all` and `--shards` flags must not disable dirty tracking; they compose with it |
| AD-004 | GoogleTests | Tests link against `.lib` files | PCH change must not alter the link model |
| AD-005 | GoogleTests | Exit code 0 = pass | Shard runner must aggregate exit codes; any shard failure → non-zero overall exit |
| AD-006 | GoogleTests | No fixtures requiring external resources | Fixture amortisation must reuse an in-process interpreter, not a network or file-system resource |

## Open Design Questions

1. **Shard result merging** — RESOLVED: thin Python script (~30 lines) in DiaCLI alongside the runner. Avoids coupling unit-test runner to DiaAutomation (E2E concern); JUnit XML format is the same either way so CI tooling stays compatible.
2. **Slow-suite enforcement** — RESOLVED: `dia run googletest` prints a WARNING at the end of any run listing suites that exceeded 500 ms but lack the `SLOW_` prefix. No external timing data required — GTest already prints per-suite duration in its XML. No `dia check` gate; the warning surfaces at exactly the right moment.

## Status

`In Progress` — plan: @docs/specs/systems/googletests/googletestspeed.plan.md
