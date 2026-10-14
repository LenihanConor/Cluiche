# System Spec: F-2 Core Runtime

**Parent:** @docs/specs/applications/cow/cow.md  
**Design source:** `Output Docs/GDD/10_PrototypeStrategy/F2_CoreRuntime.md` (DesignDocs repo)

**Gameplay Domains:** none (testbed infrastructure)

## Summary

F-2 Core Runtime is a CoW testbed stage that proves the runtime foundation every other testbed stands on: deterministic RNG architecture (8 independent streams), GameSpeedController integration (speed control, pause, time bands), save/load round-trip, and band resolver behavior (DiaAttribute Feature 7). Unlike F-1, most of what F-2 touches is already designed elsewhere (GameSpeedController, E1 Save/Load); F-2's new design surface is the testbed itself and the RNG stream architecture (8 named streams: WLD, A1, B4, B6, C1, C2, B5, C4).

This is a **dev-facing validation harness**, not player-facing gameplay, accessible from the debug menu like every other testbed. It runs **one story**: *save, reload, replay*. A scripted session is saved mid-way, the reference future is recorded, the save is reloaded and the same remainder replayed, and the two futures must be bit-identical. The six GDD tests (Tests 1-6) are retained as named **checks** that report pass/fail inside that story. `F2TestRunnerModule` runs the script as an internal phase state machine; `DiaAutomation` can trigger `RunScenario` and read results from outside.

## Problem

CoW's 12 testbeds build on top of four foundation systems that must work correctly before any gameplay can be validated:

1. **RNG determinism** — CoW's 8 gameplay systems need independent, serializable random streams that never cross-contaminate. Dia's current `Random.h` is a single global Mersenne Twister per thread — no multi-stream support, no serialization.
2. **GameSpeedController wiring** — CoW's game speed system (speed presets, pause, modal nesting, time bands) must work on a live stage before S-1 to S-7 can build on it.
3. **Save/load round-trip** — CoW's E1 integration (autosave, serialize/restore contract, atomic write) must prove RNG state + world state round-trips exactly.
4. **Band resolver timing** — DiaAttribute Feature 7 (band-based initialization) must run exactly once per dragon record (on load or hatch), never re-resolve on live reads.

Without F-2, every subsequent testbed would discover foundation bugs late (S-4 Dragon finds RNG desync, S-6 Dynasty finds save/load drift). F-2 proves these systems early in isolation before complex gameplay stacks on top.

## Goals

1. **One cohesive proof** — Save mid-session, reload, replay: the reloaded future is bit-identical to the unsaved future, and a second run from the same seed is identical to the first
2. **Report six named checks** inside that proof (C1-C6, the GDD Tests 1-6), so a failure points at the broken system: streams, determinism, RNG save/load, clock, save/load, band resolver
3. **Own the speed/pause/step buttons** that drive GameSpeedController, used by the script and by hand
4. **Be the base the later testbeds extend** — each new system adds its actions to the script and its state to the fingerprint, getting determinism and save/load coverage for free

## Non-Goals

1. **Gameplay content** — No ecology, dragons, clans, or world generation (those are S-1 onward); F-2 uses minimal stub data only
2. **Dia unit tests** — GoogleTest validates individual Dia systems; F-2 proves CoW's integration of multiple systems working together
3. **Debug "force re-resolve" tool** — Rejected as scope creep (GDD F-2 decision 4); if wanted later, belongs in DragonDesign tooling (Phase 12)
4. **Replay UI** — F-2 proves determinism makes replay *possible*, but a replay viewer is out of scope
5. **RNG seed derivation algorithm** — How to derive 8 independent seeds from one master seed is a Cluiche-side implementation detail (GDD Open Question 1)

## Architecture

### Scenario Structure

F-2 is built as one `F2CoreRuntimeStage` with `F2TestRunnerModule` running one scenario, **Save, Reload, Replay**, as an internal phase state machine (`DiaAutomation` is an external-driver layer, so it only triggers `RunScenario` and reads check results). The question it answers: *if we save a game and reload it, do we get exactly the future we would have had?* A **fingerprint** is captured at each checkpoint: the 8 stream states (seed + draw count), the day counter, and the GameSpeedController state. Tests 1-6 from the GDD are **checks** raised at the point in the story where they naturally occur, each reporting Not Run / Running / Pass / Fail.

The stage UI shows:

- The scenario phases with the current phase highlighted
- The 6 checks (C1-C6) with status and last result summary (e.g. "800 / 800 draws match")
- Run Scenario button, plus the speed / pause / step buttons the scripted session uses (and a developer can also drive by hand)

Until dragon and decree systems exist, the script uses sim-only stub actions (speed changes, pause/resume, modal open/close, draws on named streams, advance N ticks); real actions replace them as systems land and extend the fingerprint.

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

### Dependency on GameSpeedController, E1, DiaAttribute Feature 7

- **GameSpeedController** — C4 depends on `GameSpeedControllerModule` + `SpeedChangeRequested` / `ManualAdvanceRequested` / `ModalOpened` / `ModalClosed` events + `SimulationTick` broadcasting (all defined in `gamespeedcontroller.md`, system Approved)
- **E1 Save/Load** — C5 depends on `SaveLoadModule`, the `cow.save` / `cow.load` commands and the `ISaveable` participants (GameSpeedController, RandomStream) registered through it (defined in `e1-saveload.md`; the `SaveRequested` / `LoadRequested` bus events are not built)
- **DiaAttribute Feature 7** — C6 depends on `InitializeFromBands()` API, band schema (NONE/LOW/BASELINE/MEDIUM/HIGH/APEX), `band_schema_version` support (Done; `diaattribute/band-based-initialization.md`)

F-2 exercises these systems as-is; no redesign or extension work in F-2's scope (GDD F-2 decision 5).

## Scenario: Save, Reload, Replay

**Setup:** Master seed S. 8 streams seeded from S. GameSpeedController, SaveLoadModule and stub dragon record (authored bands, `band_schema_version`) in place.

| Phase | Ticks | What happens | Checks raised |
|---|---|---|---|
| 0 Isolation | - | Draw interleaved from the 8 streams (A1, C4, A1, WLD, ...); compare each stream against drawing from it alone | **C1** |
| 1 Session | 0-500 | Scripted run: every speed preset, player pause, two nested modals (closing one must not resume), manual single-step while paused; each tick draws from the streams. Dragon record loaded via the band resolver | **C4** (inline transition assertions), **C6** (load path) |
| 2 Save | 500 | `cow.save quick`; capture fingerprint F500 | - |
| 3 Reference future | 500-1000 | Continue the session; capture fingerprint F1000 and the next 100 draws from each stream (800 values) | - |
| 4 Reload | 500 | `cow.load quick`; fingerprint must equal F500 | **C5** |
| 5 Replay | 500-1000 | Replay the same remainder; fingerprint must equal F1000; the 800 draws must match the reference | **C3** |
| 6 Rerun | 0-1000 | Reload the tick-0 save taken at scenario start (no in-process reset API exists), run the whole script again; fingerprint at 1000 must equal F1000 | **C2** |
| 7 Failed load | - | `cow.load` from a missing / corrupt slot | **C5** (part b) |

### Checks

**C1 Stream independence (GDD Test 1).** Pass: all 8 interleaved sequences equal their isolated baseline. Fail: any stream diverges when interleaved, i.e. shared state leaks.

**C2 Determinism (GDD Test 2).** Pass: second run's fingerprint at tick 1000 equals the first; divergence count 0 over 1,000 ticks. Fail: any stream position, day counter or GameSpeedController field differs.

**C3 Save/load RNG round trip (GDD Test 3).** Pass: the reloaded replay's 800 draws (100 x 8 streams) match the reference future exactly, 800 / 800. Fail: any value differs.

**C4 Clock proof (GDD Test 4).** Pass: every transition in phase 1 matches `gamespeedcontroller.md`: season rate per configured preset within tolerance, pause state, `modalDepth` bookkeeping, at most one `SimulationTick` per sim frame. Fail: wrong rate, modal nesting violation, unexpected resume, skipped or double season tick.

**C5 Save/load proof (GDD Test 5).** (a) Pass: after `cow.load quick`, the restored fields equal F500: all 8 stream states, the day counter and preModalSpeedTier, and the speed state equals the post-restore default (Paused, pauseSource Player, modalDepth 0), since GameSpeedController (save v2: day, sub-day accumulator, preModalSpeedTier) always resumes paused (A2 EC-03). The session closes all modals before the save, and the remainder script (500-1000) starts with an explicit speed change so the reference future and the replay converge on the same speed state. (b) Pass: a load from a missing or corrupt slot reports an error and leaves the existing quick-save file and live state untouched. Fail: any restored field differs, or a failed load alters the file or live state. Autosave cadence and kill-mid-write fault injection are out of scope per `e1-saveload.md`.

**C6 Band resolver timing (GDD Test 6).** Load path: loading the dragon record runs the resolver exactly once and produces stable floats. Changing the record's bands in memory without bumping `band_schema_version` leaves the cached floats unchanged (no re-resolve on live read). Hatch path (egg -> Young via B6) must run the same single resolution at `on_hatch`; deferred until B6 exists. Pass: invocation count = 1 per dragon, floats stable. Fail: more than one invocation, or floats change without a version bump.

## Public Interface

### Stage

- **`F2CoreRuntimeStage`** — Testbed stage class (inherits from `DiaStage`)
  - Registered in CoW Boot stage transitions (per AD-005)
  - Initializes `F2TestRunnerModule` + `GameSpeedControllerModule` + `SaveLoadModule` + stub dragon data for C6

### Module

- **`F2TestRunnerModule`** — Scenario orchestration module
  - `RunScenario()` — Run Save, Reload, Replay end to end
  - `GetCheckStatus(CheckId) -> CheckStatus` — C1-C6 result (NotRun / Running / Pass / Fail)
  - `GetLastResult(CheckId) -> string` — Last result summary for UI display
  - `GetCurrentPhase() -> PhaseId` — For the phase strip in the UI

### Events (internal to F2)

- **`PhaseEntered{PhaseId}`** — Fired when a scenario phase begins
- **`CheckCompleted{CheckId, Pass/Fail, ResultSummary}`** — Fired when a check resolves
- **`ScenarioCompleted{PassCount, FailCount}`** — Fired when the scenario finishes

### UI

- **TestRunnerPanel** — Shows scenario phases, 6 check rows, status, Run Scenario and speed/pause/step buttons
  - Built with CoW's debug UI toolkit (same as F-1 Harness)
  - Displays pass/fail status, last result summary, elapsed time per phase

## Dependencies

### Systems (CoW)

- **GameSpeedController** — Approved; core, module and save participant built; F-2 check C4 exercises it. The speed/pause/step buttons are F-2's to build
- **E1 Save/Load** — Approved; `SaveLoadModule` shell, `cow.save`/`cow.load` and participants for GameSpeedController and RandomStream built or in progress; F-2 check C5 exercises it

### Features (Dia)

- **DiaAttribute Feature 7** (band-based initialization) — ✅ Done; F-2 check C6 exercises it
- **RandomStreamManager** (multi-stream RNG) — ✅ Done (commit d0bb72ed); scenario phases 0, 3, 5, 6 ready to build

### Modules (Dia)

- **DiaAutomation** — Exists (used by CluicheTest); optional external trigger for `RunScenario` and result readout, not the script runner
- **DiaSimTime** — Exists and Done; GameSpeedController drives its world time scale/pause
- **DiaSaveGame** — Exists and Done; E1 wraps it

## Inherited Binding Decisions

<!-- AI: Populated from parent Platform and Application specs (Binding=Yes decisions only) -->

| ID | Decision | Rationale | Scope | Impact on this system |
|----|----------|-----------|-------|----------------------|
| PD-001 | StringCRC for all entity/component IDs | Compile-time hashing, prevents typos | Platform-wide | F-2 check and phase IDs (C1-C6) use StringCRC for identification |
| PD-002 | ProcessingUnit/Phase/Module architecture | Multi-threaded execution, explicit lifecycle | Platform-wide | F2CoreRuntimeStage uses PU/Phase/Module pattern; F2TestRunnerModule orchestrates the scenario |
| PD-004 | No STL containers in public APIs | Consistent memory management | Platform-wide | F2TestRunnerModule returns Dia containers (DynamicArrayC) for test results |
| PD-007 | C++20 required | Modern C++ features | Platform-wide | F-2 code compiles under `/std:c++20` |
| PD-010 | `.diagame` is project root, `.diastage` declares stages | Typed imports, tooling discovery | Platform-wide | F-2 has a `.diastage` file (name + manifest path) listed in CoW Boot transitions |
| AD-001 | Permission guardrail for Dia/Baseline edits | Explicit sign-off required | CoW app | RandomManager was flagged in `dia-needs.md` (row 10) and built in Dia as `RandomStreamManager`; F-2 only consumes it |
| AD-002 | Four-tier model classification | Prevents leakage, reusable code at right layer | CoW app | F-2 classifies every capability: RandomManager → Dia (row 10), TestRunnerPanel → Feature-local |
| AD-003 | GDD owns design, spec owns implementation | One source of truth per concern | CoW app | F-2 spec defers to GDD F2_CoreRuntime.md for test scenarios / pass metrics; this spec owns class/module structure |
| AD-005 | One stage per testbed | Reuses Boot transition pattern | CoW app | F-2 is one stage with one scenario and 6 checks, not separate stages |

## Features

<!-- AI: Feature specs live as separate files; this table links to them. Mark status as Draft/Approved/In Progress/Done. -->

F-2 is a testbed stage, not a capability layer — it has no features of its own. It exercises systems with their own feature specs:

- GameSpeedController → 2 features (Approved, partly built) at `gamespeedcontroller.md`
- E1 Save/Load → 5 features (Approved, partly built) at `e1-saveload.md`
- DiaAttribute Feature 7 → 1 feature (Done) at `diaattribute/band-based-initialization.md`

## Open Design Questions

<!-- AI: Real uncertainties or risks surfaced during spec authoring. Skip if straightforward. -->

1. **Scenario stub fidelity** — Sim-only stub actions prove determinism of the clock and RNG, not of any gameplay system. Each system that lands should extend the script and the compared state; decide whether that is done in F-2 or in the owning testbed.

2. **C6 dragon stub data** — How much of a dragon record does C6 need? Minimal: `DragonID`, `band_schema_version`, authored bands (BASELINE health, HIGH strength, etc.), resolved floats storage. If B6 Dragon Lifecycle isn't built yet when F-2 starts, the C6 hatch path gets deferred until B6 exists, and C6 only proves the load path initially.

## Status

`Approved` — 2026-10-11 (amended 2026-10-14: single-scenario structure, Test 5 scoped to E1, Feature 7 Done, scenario driver)

Plan: @docs/specs/applications/cow/systems/f2-core-runtime/f2-core-runtime.plan.md

## GDD Cross-Reference

**Design source of truth:** `C:\GitHub\DesignDocs\COW\Output Docs\GDD\10_PrototypeStrategy\F2_CoreRuntime.md`

**Architecture Decisions from GDD:**
- Decision 1: One dedicated RNG stream per system (8 streams: WLD, A1, B4, B6, C1, C2, B5, C4)
- Decision 2: RandomManager Dia-need (multi-stream + enforcement)
- Decision 3: Band resolver runs once per dragon (load or hatch)
- Decision 4: No debug "force re-resolve" command (rejected, scope creep)
- Decision 5: A2 and E1 are proven, not redesigned
- Decision 6: scripted scenario (GDD: DiaAutomation drives Tests 2 and 3). Amended: the module runs the script itself; DiaAutomation is only the external trigger
- Decision 7: RandomManager enforces no bypass in debug builds

**Test Scenarios:** GDD Section 5's six tests are preserved as checks C1-C6 with their pass conditions. This spec restructures them into a single Save, Reload, Replay scenario; the GDD (Locked 2026-10-09) still describes six independent tests and needs a matching amendment.

**Metrics:** From GDD Section 6:
- Determinism: zero divergence over 1,000 ticks
- Save/load RNG: exact match on 800 values (100 draws × 8 streams)
- Band resolver: exactly 1 invocation per dragon record

**Failure Conditions:** From GDD Section 7.

**Challenge Pass:** Tier 2 complete (4 challenges raised, all resolved). See GDD for details.
