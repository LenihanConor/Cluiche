# Research: Ideate — Google Test Suite Speed

**Input:** docs/research/google_test_suite_speed/explore.md

## Candidates

### Candidate 1: Precompiled Header for gtest.h
**Home module/system:** GoogleTests.vcxproj / Directory.Build.props
**Size:** S
**Description:** Add a precompiled header (`pch.h`) that includes `<gtest/gtest.h>` and the most
commonly used Dia headers across test files. MSVC will parse these headers once and cache the result.
With 404 .cpp files all including gtest/gtest.h, the repeated parse cost is significant — gtest.h
pulls in a large header graph. The PCH config belongs in Directory.Build.props per PD-008, or
in a `GoogleTests`-specific props import to avoid polluting non-test projects.

**Primary value:** Reduces incremental compile time for any test change; single-developer daily
loop becomes noticeably faster after any source edit.

---

### Candidate 2: Fast/Slow Tag Convention + DiaCLI Default Filter
**Home module/system:** DiaCLI (test runner) + test file naming convention
**Size:** S
**Description:** Establish a naming convention where slow tests (physics integration, Python embedding,
WebSocket round-trips) use a suite prefix like `Slow_` or live in suites ending in `_Integration`.
Update `dia run googletest` to pass `--gtest_filter=-*Slow_*:-*_Integration*` by default, running
only fast tests (~1–2 min). A `--full` flag drops the filter for the complete 5-minute run. No test
files need to move; only the slow suites in the worst offender folders (RigidBody2D, SoftBody2D,
Python, WebSocket, Animation2D) need their class names prefixed. The DiaCLI change is a two-line
filter string addition.

**Primary value:** Developers run the fast suite on every commit and the full suite before pushing
to PR — immediate 60–70% wall-clock reduction for the daily loop.

---

### Candidate 3: Google Test Sharding via DiaCLI Parallel Runner
**Home module/system:** DiaCLI (test runner)
**Size:** M
**Description:** Enhance `dia run googletest` with a `--shards N` flag (default: cpu_count/2).
The runner spawns N child processes, each with `--gtest_shard_index=i --gtest_total_shards=N`
set as env vars, then merges the per-shard XML results via `--gtest_output=xml:shard_N.xml` and
reports a unified pass/fail. Google Test's shard support is built-in; the DiaCLI work is Python
multiprocessing + XML merge. On a typical 8-core dev machine, 4 shards could cut the 5-minute run
to ~90 seconds. Works independently of Candidate 2 and compounds with it.

**Primary value:** Runtime parallelism with zero changes to test code or .vcxproj files; scales
with machine core count; immediately available on CI multi-core runners.

---

### Candidate 4: Release-with-Debug-Info Config for Tests
**Home module/system:** Directory.Build.props / GoogleTests.vcxproj
**Size:** S
**Description:** Add a `RelWithDebInfo|x64` build configuration (or promote use of the existing
`Release|x64`) for the test binary. Physics integration, math, and geometry tests run timed
simulation loops that are dominated by floating-point work — O2 optimisation can cut their
runtime by 3–5x. Debug symbols (`/Zi`) are retained for readable crash stacks. ASAN/UBSan
configs are unaffected (they have their own configurations). DiaCLI's `--config` flag already
passes through, so `dia run googletest --config Release` works today with no new code.
Documentation and a note in CLAUDE.md are the main deliverables.

**Primary value:** Cuts physics/math/geometry test runtime by 3–5x with a one-line CLI flag;
no code changes required; easy to make the CI default.

---

### Candidate 5: Slow-Suite Quarantine Binary
**Home module/system:** New `GoogleTests.Slow.vcxproj` + DiaCLI
**Size:** M
**Description:** Extract the known slow test categories (Python/, WebSocket/, RigidBody2D
integration tests, SoftBody2D) into a separate `GoogleTests.Slow.vcxproj` that links only the
relevant subset of Dia libraries. The main `GoogleTests.vcxproj` drops those files and links.
`dia run googletest` runs only the fast binary; `dia run googletest --slow` or a separate
`dia run googletest-slow` runs the quarantine binary. This is a lighter version of full
per-module splitting — two binaries instead of 60.

**Primary value:** Link time for the main binary drops (fewer libs); slow tests are still
maintained and gated on CI but don't block the daily dev loop; only two .vcxproj files to maintain.

---

### Candidate 6: Fixture Amortisation Audit (SetUpTestSuite)
**Home module/system:** Test source files — Python/, WebSocket/, Editor/
**Size:** M
**Description:** Audit every TEST_F fixture class in the Python/, WebSocket/, and DiaEditor/
folders for expensive `SetUp()` work repeated per test case. Migrate to `SetUpTestSuite()` /
`TearDownTestSuite()` (static, called once per suite) where the fixture state is safe to share.
Python's `Py_Initialize` / `Py_Finalize` cycle is the biggest candidate — a single interpreter
shared across all Python test cases in a suite eliminates hundreds of 200ms init calls.
Similarly, any WebSocket server that spins up per test-case can be promoted to per-suite.

**Primary value:** Directly cuts the Python and WebSocket test runtime, which are likely the
single largest contributors to the 5-minute total; no infrastructure change required.

---

### Candidate 7: DiaCLI Dirty-Module Test Tracking
**Home module/system:** DiaCLI (test runner + build pipeline)
**Size:** L
**Description:** DiaCLI tracks a `out/GoogleTests/last_pass.json` file recording which test
suites passed and which source files were current at that time. On the next run it computes
which Dia modules have changed (via git diff or mtime scan), maps them to affected test folders
using the existing module dependency graph (`Tools/dia_modules.py`), and only re-runs those
suites via `--gtest_filter`. Unaffected suites are assumed green. A `--force-full` flag bypasses
the cache. This is analogous to Bazel test caching at the DiaCLI layer.

**Primary value:** On focused changes (e.g. a DiaCore fix), only DiaCore tests rerun — 30
seconds instead of 5 minutes; scales to CI if the cache is committed as a CI artifact.

---

### Candidate 8: Per-Module Test Binaries
**Home module/system:** New set of per-module .vcxproj files (one per Dia module)
**Size:** XL
**Description:** Split GoogleTests.vcxproj into ~20 per-module test binaries
(DiaCore.Tests.exe, DiaMaths.Tests.exe, DiaRigidBody2D.Tests.exe, etc.). Each links only the
libraries its module needs. Binaries are run in parallel by DiaCLI. On a clean build, total link
time is lower because no single link step pulls in all 60+ libs; incremental builds only relink
the affected module's binary. DiaCLI's `dia run googletest` becomes an orchestrator over all
module binaries.

**Primary value:** Maximum parallelism and minimum per-change link/run time; mirrors the
module architecture of the engine itself. However, requires creating and maintaining ~20 new
.vcxproj and .vcxproj.filters files by hand (PD-006), which is significant ongoing overhead.

---

## Coverage Map

The eight candidates span all three root causes identified in explore.md:

| Root Cause | Candidates |
|------------|-----------|
| Slow test runtime (physics, Python, WebSocket) | C2 (tag+filter), C4 (Release config), C6 (fixture amortise), C5 (quarantine binary) |
| No parallelism | C3 (shard runner), C8 (per-module binaries) |
| Compile / link time | C1 (PCH), C4 (Release), C8 (per-module) |
| Developer workflow | C2 (fast default), C7 (dirty tracking), C3 (shards) |

Scope range: S (C1, C2, C4) through M (C3, C5, C6) to L (C7) and XL (C8) — full spectrum represented.
