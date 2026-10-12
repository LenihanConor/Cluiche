# System Spec: F-2 Core Runtime

**Parent:** @docs/specs/applications/cow/cow.md  
**Design source:** `Output Docs/GDD/10_PrototypeStrategy/F2_CoreRuntime.md` (DesignDocs repo)

**Gameplay Domains:** none (testbed infrastructure)

## Summary

F-2 Core Runtime is a CoW testbed stage that proves the runtime foundation every other testbed stands on: deterministic RNG architecture (8 independent streams), WorldClock integration (speed control, pause, time bands), save/load round-trip, and band resolver behavior (DiaAttribute Feature 7). Unlike F-1, most of what F-2 touches is already designed elsewhere (WorldClock, E1 Save/Load are both Approved); F-2's new design surface is the testbed itself and the RNG stream architecture (8 named streams: WLD, A1, B4, B6, C1, C2, B5, C4).

This is a **dev-facing validation harness**, not player-facing gameplay. It runs 6 independent tests, each with pass/fail criteria, accessible from the debug menu like every other testbed. F-2 uses `DiaAutomation` for test orchestration (scripted action sequences, determinism proofs).

## Problem

CoW's 12 testbeds build on top of four foundation systems that must work correctly before any gameplay can be validated:

1. **RNG determinism** — CoW's 8 gameplay systems need independent, serializable random streams that never cross-contaminate. Dia's current `Random.h` is a single global Mersenne Twister per thread — no multi-stream support, no serialization.
2. **WorldClock wiring** — CoW's time system (speed tiers, pause, modal nesting, time bands) must work on a live stage before S-1 to S-7 can build on it.
3. **Save/load round-trip** — CoW's E1 integration (autosave, serialize/restore contract, atomic write) must prove RNG state + world state round-trips exactly.
4. **Band resolver timing** — DiaAttribute Feature 7 (band-based initialization) must run exactly once per dragon record (on load or hatch), never re-resolve on live reads.

Without F-2, every subsequent testbed would discover foundation bugs late (S-4 Dragon finds RNG desync, S-6 Dynasty finds save/load drift). F-2 proves these systems early in isolation before complex gameplay stacks on top.

## Goals

1. **RNG stream independence** — Prove 8 dedicated streams (WLD, A1, B4, B6, C1, C2, B5, C4) draw sequences independently with no shared state leaks (Test 1)
2. **Determinism proof** — Same seed + same script = identical simulation state, zero divergence over 1,000 ticks (Test 2)
3. **RNG save/load proof** — All 8 streams round-trip exactly through save/load; reload continues the exact sequence unsaved play would have produced (Test 3)
4. **WorldClock integration** — Prove speed tiers, pause, modal nesting, single-step work on a live CoW stage matching GDD A2 spec (Test 4)
5. **E1 integration** — Prove autosave cadence, atomic write, serialize/restore contract work on a live CoW stage matching GDD E1 spec (Test 5)
6. **Band resolver timing** — Prove resolver runs exactly once per dragon (on load or hatch), produces stable floats, never re-resolves on live reads (Test 6)

## Non-Goals

1. **Gameplay content** — No ecology, dragons, clans, or world generation (those are S-1 onward); F-2 uses minimal stub data only
2. **Dia unit tests** — GoogleTest validates individual Dia systems; F-2 proves CoW's integration of multiple systems working together
3. **Debug "force re-resolve" tool** — Rejected as scope creep (GDD F-2 decision 4); if wanted later, belongs in DragonDesign tooling (Phase 12)
4. **Replay UI** — F-2 proves determinism makes replay *possible*, but a replay viewer is out of scope
5. **RNG seed derivation algorithm** — How to derive 8 independent seeds from one master seed is a Cluiche-side implementation detail (GDD Open Question 1)

## Architecture

### Test Structure

F-2 is built as one `F2CoreRuntimeStage` with `F2TestRunnerModule` orchestrating 6 independent tests. Each test is a discrete scenario with pass/fail reporting. Tests can run individually or as a full suite. The stage UI shows:

- Test list (6 rows: Test 1 to Test 6)
- Status per test (Not Run / Running / Pass / Fail)
- Last result summary (e.g., "Zero divergence over 1,000 ticks")
- Run buttons (individual test, run all)

Tests 2 and 3 use `DiaAutomation` for scripted action sequences (spawn dragon, issue decree, advance N ticks). Tests 1, 4, 5, 6 are direct API exercise.

### RNG Stream Architecture

**Built:** Dia's `RandomStreamManager` (DiaMaths/Core/RandomStreamManager.h) + CoW's `RandomStreamModule` (commit d0bb72ed). Manager owns multiple named `RandomStream` instances, each thread-safe with per-stream mutex. CoW module creates 8 streams (WLD, A1, B4, B6, C1, C2, B5, C4) with distinct seeds on startup.

**8 streams required:**
- **WLD** — World generation (S-1)
- **A1** — Ecology system
- **B4** — Dragon movement/pathfinding
- **B6** — Dragon lifecycle (hatch, aging, death)
- **C1** — Lineage/hearth reproduction
- **C2** — Genetics drift
- **B5** — Challenge (rival RNG swing)
- **C4** — Dynasty/clan politics

Each stream is independently seeded at `WorldGenerationComplete` from the master world seed (GDD F-2 decision 1). All 8 streams serialize/restore with every save/load cycle (GDD F-2 decision 3).

### Dependency on WorldClock, E1, DiaAttribute Feature 7

- **WorldClock** — Tests 4 depends on `WorldClockModule` + `SpeedChangeRequested` / `ModalOpened` events + `SimulationTick` broadcasting (all defined in `worldclock.md`, system Approved)
- **E1 Save/Load** — Test 5 depends on `SaveLoadModule` + `SaveRequested` / `LoadRequested` events + `serialize_state()` / `restore_state()` contract (all defined in `e1-saveload.md`, system Approved)
- **DiaAttribute Feature 7** — Test 6 depends on `InitializeFromBands()` API, band schema (NONE/LOW/BASELINE/MEDIUM/HIGH/APEX), `band_schema_version` support (spec Draft at `diaattribute/band-based-initialization.md`)

F-2 exercises these systems as-is; no redesign or extension work in F-2's scope (GDD F-2 decision 5).

## Test Scenarios

### Test 1: Stream Independence

**Setup:** 8 dedicated streams initialized with arbitrary seeds.

**Action:** Draw a sequence from each stream in an interleaved order (e.g., draw from A1, then C4, then A1 again, then WLD). Record each stream's output sequence.

**Expected:** Each stream's output sequence matches drawing from it alone, uninterrupted by the others. No shared state leaks between streams.

**Pass condition:** All 8 sequences match their isolated-draw baseline.

**Failure condition:** Any stream's sequence diverges when interleaved vs. isolated.

### Test 2: Determinism Round Trip

**Setup:** `DiaAutomation` script with a fixed action sequence (e.g., spawn dragon, issue Hunt decree, advance 1,000 ticks).

**Action:** Run script twice from the same master seed. Record all 8 streams' final positions and resulting world state.

**Expected:** Both runs produce bit-identical stream positions and world state.

**Pass condition:** Zero divergence across all 8 streams and world state over 1,000 ticks.

**Failure condition:** Any stream position or world state field differs between the two runs.

**Metrics:** Divergence count (must be 0), tick count (1,000).

### Test 3: Save/Load RNG Round Trip

**Setup:** Same `DiaAutomation` script as Test 2.

**Action:** Run seeded session, save at tick 500, continue unsaved to tick 600 recording each stream's next 100 draws (800 values total). Separately, reload the tick-500 save and draw the same 100 values from each stream.

**Expected:** Both sets (saved-and-reloaded vs. unsaved-continuation) match exactly.

**Pass condition:** Exact match on 800 values (100 draws × 8 streams).

**Failure condition:** Any RNG value differs between reload and unsaved continuation.

**Metrics:** Match count (must be 800 / 800).

### Test 4: Clock Proof

**Setup:** WorldClock initialized on a live CoW stage.

**Action:** Exercise all speed tiers (PAUSED/NORMAL/FAST/FASTEST), player pause, nested modal auto-pause (open two modals, confirm closing one doesn't resume), manual single-step while paused.

**Expected:** Every transition matches `A2_TechnicalSpec.md` / `worldclock.md` exactly (Hz values, pause state, modal_depth bookkeeping).

**Pass condition:** All transitions match spec behavior.

**Failure condition:** Any speed tier Hz mismatch, modal nesting violation, or unexpected resume.

### Test 5: Save/Load Proof

**Setup:** E1 SaveLoadModule initialized on a live CoW stage.

**Action:** Exercise autosave firing on season-count schedule, manual save request, atomic write (fault injection if feasible: kill process mid-write, confirm prior save intact), failed load leaves existing file untouched.

**Expected:** Every behavior matches `e1-saveload.md` exactly (autosave cadence, atomic write guarantees, error handling).

**Pass condition:** All E1 behaviors match spec.

**Failure condition:** Autosave skipped, corrupt save written, failed load overwrites good file.

### Test 6: Band Resolver Timing

**Setup:** Dragon record with authored bands (BASELINE for health, HIGH for strength, etc.).

**Action:** 
1. Load dragon, confirm resolver runs exactly once, produces stable floats
2. Change dragon's bands in memory without bumping `band_schema_version`
3. Confirm cached floats do not change (no re-resolve on live read)
4. Separately, hatch a new dragon mid-session (egg → Young via B6)
5. Confirm identical resolution step runs once at `on_hatch`, produces stable floats same way as load path

**Expected:** Resolver invoked exactly once per dragon record (across both triggers: load and hatch), never re-invoked on live reads.

**Pass condition:** Invocation count = 1 per dragon, cached floats stable across live reads.

**Failure condition:** Resolver invoked >1 times per dragon, or cached floats change without `band_schema_version` bump.

## Public Interface

### Stage

- **`F2CoreRuntimeStage`** — Testbed stage class (inherits from `DiaStage`)
  - Registered in CoW Boot stage transitions (per AD-005)
  - Initializes `F2TestRunnerModule` + `WorldClockModule` + `SaveLoadModule` + stub dragon data for Test 6

### Module

- **`F2TestRunnerModule`** — Test orchestration module
  - `RunTest(TestId)` — Run one of the 6 tests
  - `RunAllTests()` — Run full suite sequentially
  - `GetTestStatus(TestId) -> TestStatus` — Query test result (NotRun / Running / Pass / Fail)
  - `GetLastResult(TestId) -> string` — Last result summary for UI display

### Events (internal to F2)

- **`TestStarted{TestId}`** — Fired when a test begins
- **`TestCompleted{TestId, Pass/Fail, ResultSummary}`** — Fired when a test finishes
- **`AllTestsCompleted{PassCount, FailCount}`** — Fired when suite finishes

### UI

- **TestRunnerPanel** — Shows 6 test rows, status, run buttons
  - Built with CoW's debug UI toolkit (same as F-1 Harness)
  - Displays pass/fail status, last result summary, elapsed time per test

## Dependencies

### Systems (CoW)

- **WorldClock** — Approved (4 features Approved), not built yet; F-2 Test 4 exercises it
- **E1 Save/Load** — Approved (5 features Approved), not built yet; F-2 Test 5 exercises it

### Features (Dia)

- **DiaAttribute Feature 7** (band-based initialization) — Draft spec exists; F-2 Test 6 exercises it
- **RandomStreamManager** (multi-stream RNG) — ✅ Done (commit d0bb72ed); F-2 Tests 1-3 ready to build

### Modules (Dia)

- **DiaAutomation** — Exists (used by CluicheTest); Tests 2 and 3 drive scripted action sequences through it
- **DiaSimTime** — Exists and Done; WorldClock wraps it
- **DiaSaveGame** — Exists and Done; E1 wraps it

## Inherited Binding Decisions

<!-- AI: Populated from parent Platform and Application specs (Binding=Yes decisions only) -->

| ID | Decision | Rationale | Scope | Impact on this system |
|----|----------|-----------|-------|----------------------|
| PD-001 | StringCRC for all entity/component IDs | Compile-time hashing, prevents typos | Platform-wide | F-2 test IDs (Test1-Test6) use StringCRC for identification |
| PD-002 | ProcessingUnit/Phase/Module architecture | Multi-threaded execution, explicit lifecycle | Platform-wide | F2CoreRuntimeStage uses PU/Phase/Module pattern; F2TestRunnerModule orchestrates tests |
| PD-004 | No STL containers in public APIs | Consistent memory management | Platform-wide | F2TestRunnerModule returns Dia containers (DynamicArrayC) for test results |
| PD-007 | C++20 required | Modern C++ features | Platform-wide | F-2 code compiles under `/std:c++20` |
| PD-010 | `.diagame` is project root, `.diastage` declares stages | Typed imports, tooling discovery | Platform-wide | F-2 has a `.diastage` file (name + manifest path) listed in CoW Boot transitions |
| AD-001 | Permission guardrail for Dia/Baseline edits | Explicit sign-off required | CoW app | F-2 flags RandomManager as Dia-need in `dia-needs.md`, does not implement it locally |
| AD-002 | Four-tier model classification | Prevents leakage, reusable code at right layer | CoW app | F-2 classifies every capability: RandomManager → Dia (row 10), TestRunnerPanel → Feature-local |
| AD-003 | GDD owns design, spec owns implementation | One source of truth per concern | CoW app | F-2 spec defers to GDD F2_CoreRuntime.md for test scenarios / pass metrics; this spec owns class/module structure |
| AD-005 | One stage per testbed | Reuses Boot transition pattern | CoW app | F-2 is one stage with 6 tests inside, not 6 separate stages |

## Features

<!-- AI: Feature specs live as separate files; this table links to them. Mark status as Draft/Approved/In Progress/Done. -->

F-2 is a testbed stage, not a capability layer — it has no features of its own. It exercises systems with their own feature specs:

- WorldClock → 4 features (Approved, not built) at `worldclock.md`
- E1 Save/Load → 5 features (Approved, not built) at `e1-saveload.md`
- DiaAttribute Feature 7 → 1 feature (Draft) at `diaattribute/band-based-initialization.md`

## Open Design Questions

<!-- AI: Real uncertainties or risks surfaced during spec authoring. Skip if straightforward. -->

1. **Test 5 atomic write fault injection** — "Kill process mid-write, confirm prior save intact" is the ideal test, but requires fault injection tooling Dia may not have. Fallback: test atomic write guarantees via file system state inspection (temp file cleanup, no partial writes). Decide during implementation.

3. **Test 6 dragon stub data** — How much of a dragon record does Test 6 need? Minimal: `DragonID`, `band_schema_version`, authored bands (BASELINE health, HIGH strength, etc.), resolved floats storage. If B6 Dragon Lifecycle isn't built yet when F-2 starts, Test 6 step 4 (hatch trigger) gets deferred until B6 exists, and Test 6 only proves the load path initially.

## Status

`Approved` — 2026-10-11

## GDD Cross-Reference

**Design source of truth:** `C:\GitHub\DesignDocs\COW\Output Docs\GDD\10_PrototypeStrategy\F2_CoreRuntime.md`

**Architecture Decisions from GDD:**
- Decision 1: One dedicated RNG stream per system (8 streams: WLD, A1, B4, B6, C1, C2, B5, C4)
- Decision 2: RandomManager Dia-need (multi-stream + enforcement)
- Decision 3: Band resolver runs once per dragon (load or hatch)
- Decision 4: No debug "force re-resolve" command (rejected, scope creep)
- Decision 5: A2 and E1 are proven, not redesigned
- Decision 6: DiaAutomation drives Tests 2 and 3
- Decision 7: RandomManager enforces no bypass in debug builds

**Test Scenarios:** Lifted verbatim from GDD Section 5.

**Metrics:** From GDD Section 6:
- Determinism: zero divergence over 1,000 ticks
- Save/load RNG: exact match on 800 values (100 draws × 8 streams)
- Band resolver: exactly 1 invocation per dragon record

**Failure Conditions:** From GDD Section 7.

**Challenge Pass:** Tier 2 complete (4 challenges raised, all resolved). See GDD for details.
