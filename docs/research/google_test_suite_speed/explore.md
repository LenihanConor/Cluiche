# Research: Explore — Google Test Suite Speed

**Session date:** 2026-06-06
**Folder:** docs/research/google_test_suite_speed/

## Problem Space Overview

The GoogleTests suite has grown to 404 .cpp files (~18,000 lines across 59 folders) covering the full
Dia engine — Core, Maths, Physics, Animation, Entity, Editor, Observation, and more. The single-binary
model compiles everything into one `GoogleTests.exe` and runs all tests sequentially. With 5+ minutes of
wall-clock time per run, the feedback loop is long enough to discourage running the suite before commits.

The root cause is compound: a large number of tests, some of which involve real-time physics integration,
heavy fixture setup, Python embedding, and WebSocket round-trips. These slow tests are co-scheduled with
fast unit tests and there is no mechanism to skip them selectively. The CLI supports `--filter` but
requires the developer to know which pattern to use.

The binary itself is also expensive to link: 60+ Dia libraries, googletest, SDL3, bgfx, Python 3.11,
protobuf, imgui, websocketpp, and ASIO are all pulled in regardless of which tests are actually run.
Every re-link touches the whole world. No sharding, no parallelism, no incremental test selection.

## Existing Approaches

- **Test sharding** — Google Test supports `--gtest_shard_index=N --gtest_total_shards=M`; splits
  the test list into M buckets and runs one. CI systems use this to parallelise across machines/processes.
- **Parallel test execution within a binary** — third-party runners (gtest-parallel, pytest-xdist
  analogue for C++) can spawn the binary multiple times with different shard indices.
- **Separate test binaries per module** — each Dia module gets its own `DiaCore.Tests.exe`;
  each binary links only what its module needs; binaries run in parallel.
- **Test labelling / categorisation** — `--gtest_filter` with a convention like `UNIT_*`, `SLOW_*`,
  `INTEGRATION_*` lets developers run fast subsets without knowing individual test names.
- **Build-time dirty tracking** — only rebuild and re-run test binaries whose source files changed
  since the last passing run (analogous to Bazel's test caching or Buck's dep-graph awareness).
- **Release config for tests** — O2/O3 optimisation dramatically shrinks physics and math test times
  without affecting correctness in most cases.
- **Mock / stub slow subsystems** — tests that exercise Python embedding, WebSocket, or file I/O can
  be replaced by fakes that run in microseconds.
- **TEST_F SetUpTestSuite vs SetUp** — expensive fixture initialisation amortised across a suite
  rather than repeated per test case.
- **Link-time improvements** — incremental linking, /GL whole-program-optimisation, or splitting
  one giant .vcxproj into smaller ones reduces per-change link time.
- **Precompiled headers (PCH)** — one `#include <gtest/gtest.h>` PCH across all 404 files eliminates
  repeated header parsing in MSVC.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| Scope of change | One binary → many binaries vs. stay single binary | Many binaries = more vcxproj work, more CI complexity |
| Parallelism model | Sharding (process-level) vs. threading (within process) | Threading in gtest is fragile; sharding is safer |
| Dirty tracking | DiaCLI-level vs. IDE-level (incremental build) vs. none | DiaCLI already has pipeline; could add test-dirty tracking |
| Test categorisation | Tag/filter convention vs. separate binaries vs. test suites | Convention is cheapest to adopt |
| Build speed | PCH / incremental link / Release config vs. current Debug | Quick wins, no restructuring |
| Fixture cost | Amortise setup vs. mock slow subsystems | Per-test vs. per-suite setup is a code change |

## Known Tradeoffs

- **Many small binaries** — faster feedback per module, but more vcxproj files to maintain, more
  DiaCLI commands to learn, and CI job count multiplies.
- **Sharding** — nearly free to add (two env vars), but only helps on multi-core machines / CI;
  does nothing for a single developer on a local serial run.
- **Release config** — hides debug symbols; sanitiser configs (ASAN, UBSan) are already in the
  vcxproj which implies correctness checking is a goal, not just speed.
- **Tag conventions** — easy to adopt going forward, but existing 404 files need auditing or
  slow tests remain mixed in.
- **PCH** — meaningful compile-time win (gtest.h is large), essentially zero correctness risk.
- **Dirty tracking in DiaCLI** — complex to implement correctly; must handle transitive dep changes.

## Known Pitfalls (C++ / game engine context)

- Physics integration tests are inherently time-bound; can't stub the integrator without defeating the test.
- Python embedding startup (`Py_Initialize`) is expensive (~200ms); tests that spin up a Python
  interpreter per TEST_F hit this repeatedly.
- WebSocket round-trip tests have real I/O waits; timeouts set too low cause flakiness; too high wastes time.
- ASAN/UBSan configs already exist — any solution must not break those configurations.
- The post-build `pipeline deploy` step runs on every build, even if only one file changed; this adds
  latency before a test run can start.
- Splitting into many binaries increases total link time on a clean build, even if incremental builds
  are faster.
- `--gtest_parallel` is not natively supported; the gtest-parallel Python script is an external dependency.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaCLI | Already the runner; `dia run googletest --filter` is the user-facing entry point; new flags live here |
| GoogleTests.vcxproj | Single .vcxproj covering all 404 files; `MultiProcessorCompilation` already enabled |
| Directory.Build.props | Owns OutDir/IntDir/toolchain; PCH config could live here for test projects |
| DiaObservation | Test suite covers 18 files of observation code; could instrument test timing itself |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-006 Visual Studio project files are source of truth | Any multi-binary split needs new .vcxproj files maintained by hand; significant ongoing cost |
| PD-008 Directory.Build.props owns OutDir/IntDir | PCH and incremental-link settings should go in Directory.Build.props, not per-project |
| PD-007 C++20 required | No compiler constraints on speed options; PCH, /GL, and LTO are all legal |
| PD-005 x64 only | Sharding via env vars works; no cross-platform complications |
| PD-009 out/ for generated output | Test result caches and dirty-tracking state files belong under `Cluiche/out/GoogleTests/` |

## Open Questions for Ideation

- Is 5 minutes mostly **compile time**, **link time**, or **runtime**? (The answer shapes which fixes help most.)
- Which test folders are the slowest at runtime? Physics and Python are suspects; need profiling data.
- Is the 5-minute pain felt mostly on **local dev** (every change) or on **CI** (every PR)?
- How many developers are on this project? A multi-binary split has a maintenance cost that scales with team size.
- Is the `pipeline deploy` post-build step running even when no test binary content changes? Can it be made conditional?
- Are there any tests that could be promoted to a nightly / slow suite with no loss to daily confidence?
