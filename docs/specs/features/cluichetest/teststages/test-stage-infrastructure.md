# Feature Spec: Test Stage Infrastructure

## Parent System
@docs/specs/systems/cluichetest/teststages.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-004, PD-006, PD-007, PD-009, PD-010 |
| Application | @docs/specs/applications/cluichetest.md | AD-001, AD-004, AD-005 |
| System | @docs/specs/systems/cluichetest/teststages.md | SD-TS-001, SD-TS-002, SD-TS-003, SD-TS-004, SD-TS-005 |
| Cross-system | @docs/specs/systems/cluichetest/cluichetestscenarios.md | CTS-001, CTS-002 |
| Cross-system | @docs/specs/systems/dia/diaautomation.md | SD-AUT-002, SD-AUT-003, SD-AUT-005 |

## Problem Statement

CluicheTest needs infrastructure to support multiple dedicated test stages, each exercising a specific Dia engine feature. Today the Boot menu is hardcoded to navigate only to DummyStage, and the orchestrator has no way to selectively run a subset of scenarios. This feature delivers: (a) the shared pattern and ACs that every test stage must satisfy, (b) manifest-driven Boot menu navigation, and (c) CLI scenario filtering for selective test execution.

## Gap Analysis — Why E2E Test Stages?

CluicheTest already has ~5000 GoogleTests covering unit/integration for each Dia system. The shift-left philosophy prefers unit tests. E2E test stages fill gaps that unit/integration tests **cannot** reach:

| Gap Category | What unit tests miss | What e2e catches |
|--------------|---------------------|-----------------|
| Real PU timing | Manual `Step()` calls with explicit dt | Real TimeServer + fixed-timestep accumulator under load |
| Asset pipeline integration | Hardcoded test data | Load from disk → async readiness → use → unload lifecycle |
| Cross-stream data flow | Isolated module calls | Module A writes stream → Module B reads stream, per-frame |
| Scene lifecycle | Individual object create/destroy | Full stage enter → setup → run → teardown → exit, verify no leaks |
| Multi-system coupling | Each system tested in isolation | Physics + Animation + StateMachine interacting over real frames |
| Determinism under real conditions | Deterministic manual stepping | Fixed-timestep convergence when PU scheduling jitter exists |

### Recommended Test Stages (Priority Order)

| Priority | Stage | E2E Value | Justification |
|----------|-------|-----------|---------------|
| P1 | RigidBody2DStage | Multi-body sim under real PU timing, settle detection, broad-phase coherency | Biggest gap: unit tests never exercise real TimeServer integration |
| P1 | Animation2DStage | Asset-loaded clips, playback with real dt, pose verification, Rig2D integration | Asset pipeline + frame timing; includes skeleton evaluation |
| P1 | AssetRuntimeStage | Multi-asset concurrent load, stage-transition unload/reload, verify clean state | Resource lifecycle across stages; unit tests can't catch leaks |
| P2 | StateMachineAnimStage | StateMachine driving animation transitions, verify correct clip per state | Cross-system coupling; neither system tested in combination |
| P2 | SoftBody2DStage | Rope/cloth settle under real timing, rigid body coupling | Same timing gap as RigidBody2D; lower priority (solid golden tests) |
| P3 | IKAnimStage | IK override on animated rig, verify pose matches expected | Only if IK+Animation coupling proves fragile |

### Systems with NO e2e gap

| System | Why no dedicated stage |
|--------|----------------------|
| Geometry2D | Exercised implicitly by RigidBody2DStage (broad-phase) |
| Geometry3D | No runtime consumer yet (awaiting bgfx Phase 2) |
| Input | Covered by DummyStage; automation injection is orchestrator-level |
| ApplicationFlow | Tested implicitly by all stage navigation (Boot → Stage → Boot) |

## Acceptance Criteria

### A — Shared ACs (every test stage must satisfy these)

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-S1 | Stage has a manifest entry with `transitions: ["Boot"]` | Manifest review |
| AC-S2 | Stage module registers at least one checkpoint in DoStart via AutomationService | Code review + checkpoint trigger returns success |
| AC-S3 | Stage module emits at least one metric to MetricRegistry | Metric query from orchestrator returns value |
| AC-S4 | Stage module logs at DoStart entry, DoStop entry (DIA_LOG_INFO) | Session log review |
| AC-S5 | Stage module returns StartResult::kReady only after scene setup is complete | Code review |
| AC-S6 | Stage module cleans up all resources in DoStop, returns StopResult::kDone | No leaks; asset handles released |
| AC-S7 | Navigating Boot → Stage → Boot → Stage produces identical checkpoint results | Orchestrator scenario runs twice in sequence |
| AC-S8 | Checkpoint names follow convention: `<feature>.<checkpoint_name>` | Code review |
| AC-S9 | Stage has a matching pytest scenario file in `Tools/orchestrator/scenarios/cluichetest/<stage>/` | File exists |

### B — Boot Menu (manifest-driven navigation)

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-B1 | Boot menu displays stages read from the manifest (no hardcoded stage list) | Visual + code review |
| AC-B2 | Only stages with `transitions` from Boot appear in the menu | Manifest has non-Boot stage without Boot transition → not shown |
| AC-B3 | Selecting a stage in the menu triggers `TransitionTo(stageName)` | Navigation succeeds |
| AC-B4 | DummyStage removed from Boot menu if it has no manifest entry (or kept if it does) | Menu matches manifest exactly |
| AC-B5 | New DiaAPI command `dia.manifest.stages` returns available stage names | DiaAPI query returns JSON array |

### C — CLI Scenario Filtering

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-C1 | `dia orchestrate --suite=cluichetest/default --scenario=<pattern>` runs only matching scenarios | Only filtered scenarios execute |
| AC-C2 | Pattern supports glob wildcards (e.g., `rigidbody*`, `*stage*`) | Glob match verified |
| AC-C3 | `dia orchestrate --suite=cluichetest/default --list` lists available scenarios without running | Scenario names printed, exit 0, no app launched |
| AC-C4 | No filter runs all scenarios in the plan (existing behaviour preserved) | Full suite runs |

## Template — Child Stage Spec Questionnaire

Each concrete test stage spec (RigidBody2DStage, Animation2DStage, etc.) must answer:

| # | Question | Example (RigidBody2D) |
|---|----------|----------------------|
| T1 | What engine feature is exercised? | DiaRigidBody2D: multi-body dynamics |
| T2 | What scene does DoStart set up? (entities, bodies, initial state) | 10 circles at known positions, gravity enabled, no input |
| T3 | What checkpoint(s) and their success conditions? | `rigid_body.all_settled` → true when all bodies IsAsleep() |
| T4 | What metric(s) are emitted? | `cluichetest.rigidbody.settle_frame_count`, `cluichetest.rigidbody.step_count` |
| T5 | Which PU does the module live on? | SimPU (physics runs there) |
| T6 | What assets need loading? (or is setup code-only?) | None — bodies created programmatically |
| T7 | What is the gap vs unit tests? (specific, not generic) | Unit tests call Step() manually; this validates fixed-timestep accumulator under real PU timing |
| T8 | What determinism constraints exist? | Fixed seed, no randomness, fixed timestep, N frames to converge |
| T9 | What is the expected frame budget for the test? | ~200 frames (6.6s at 30Hz) to settle 10 circles |
| T10 | Dependencies on other modules? | TimeServer (for dt), AutomationModule (for checkpoints) |

## Design

### Boot Menu — Manifest-Driven Navigation

**Data flow:** `dia.manifest.stages` (DiaAPI) → `BootUIPageModule::DoStart` (C++ queries command at module start) → exposes stage list to JS via Ultralight binding → `bootscreen.html` renders buttons dynamically.

The HTML cannot call DiaAPI directly — Ultralight pages communicate through C++ bindings only. `BootUIPageModule` acts as the bridge: it queries the manifest data in C++ and passes the stage name array to JS.

```
Command: dia.manifest.stages
Response: { "success": true, "data": { "stages": ["DummyStage", "RigidBody2DStage", "Animation2DStage", ...] } }
```

The command reads the Application's manifest stage list, filters to stages reachable from Boot (those whose `transitions` include "Boot"), and returns their names.

`BootUIPageModule::RequestLaunchLevel` currently hardcodes `TransitionTo(StringCRC("DummyStage"))`. The fix: change that single line to `TransitionTo(StringCRC(levelName))`. The `levelName` parameter already arrives from the JS binding via `LaunchUIPageExternalInterface` — it's just ignored today. Stage names in JS must match manifest StringCRC names exactly.

### CLI Scenario Filtering

The `dia orchestrate` DiaCLI command wraps `run_orchestration()`. Filtering is implemented by pruning the plan's `scenarios` array before invoking pytest — not via pytest's `-k` flag (which filters test functions, not scenario files):

- `--scenario <glob>` — `fnmatch` against each scenario path in the plan (relative to orchestrator root). Only matched entries are passed to pytest.
- `--list` — print matched scenario paths from the plan and exit (no app launch, no pytest invocation).

The plan JSON's `scenarios` array is the single source of truth. `--list` shows what the plan contains (optionally filtered by `--scenario`); it does not discover un-listed `.py` files.

```bash
dia orchestrate --suite=cluichetest/default --scenario="*rigidbody*"
dia orchestrate --suite=cluichetest/default --scenario="boot/*"
dia orchestrate --suite=cluichetest/default --list
dia orchestrate --suite=cluichetest/default --list --scenario="*rigidbody*"
```

### Test Stage Module Pattern

```cpp
// Cluiche/CluicheTest/Modules/TestStages/<Feature>StageModule.h
namespace CluicheTest {

class <Feature>StageModule : public Dia::ApplicationFlow::Module
{
public:
    static constexpr Dia::Core::StringCRC kTypeId{"<Feature>StageModule"};
    explicit <Feature>StageModule(const Dia::Core::StringCRC& instanceId);

protected:
    StartResult DoStart() override;
    void DoUpdate(float deltaTime) override;
    StopResult DoStop() override;

private:
    void SetupScene();
    void RegisterCheckpoints(Dia::Automation::AutomationService& automation);
    void RegisterMetrics(Dia::Metrics::MetricRegistry& metrics);

    Dia::ApplicationFlow::ModuleRef<AutomationModule> mAutomation{this};
    // Feature-specific members...
};

} // namespace CluicheTest
DIA_MODULE(<Feature>StageModule);
```

### Manifest Entry Pattern

```json
{
    "name": "<Feature>Stage",
    "modules": [
        { "type": "<Feature>StageModule", "instance": "<feature>_stage" }
    ],
    "transitions": ["Boot"],
    "auto_advance": false
}
```

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaApplicationFlow/Manifest/ApplicationManifestLoader.cpp` | Parse `transitions[]` and `auto_advance` from `.diastage` JSON into StageDeclaration |
| `Cluiche/Assets/Stages/DummyStage/dummy_stage.diastage` | Add `"transitions": ["Boot"]` field |
| `Dia/DiaApplicationFlow/Application.cpp` | Register `dia.manifest.stages` JSON command in `RegisterBaselineCommands()` |
| `Cluiche/CluicheTest/Modules/BootUIPageModule.h` | Add stage list storage + JS binding method |
| `Cluiche/CluicheTest/Modules/BootUIPageModule.cpp` | Query manifest stages at DoStart, expose to JS, fix `RequestLaunchLevel` to use `levelName` |
| Boot UI HTML (deployed asset) | Receive stage list from C++ binding, render dynamic menu |
| `Tools/orchestrator/cli.py` | Add `--scenario` and `--list` flags (filter plan's scenario array) |
| `Tools/orchestrator/plugin.py` | Accept pre-filtered scenario list from cli |
| Manifest (`.diastage` files) | Add `transitions` field as stages are implemented |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Add test stage module files (per child spec) |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Update `.diastage` loader to parse `transitions[]` and `auto_advance` | Load DummyStage manifest → StageDeclaration has transitions=["Boot"] | Todo | sonnet | Prerequisite for all menu/filtering work |
| 2 | Add `"transitions": ["Boot"]` to `dummy_stage.diastage` | Manifest loads with transitions populated | Todo | haiku | First consumer of Task 1 |
| 3 | Implement `dia.manifest.stages` DiaAPI command | DiaAPI query returns filtered stage list | Todo | sonnet | In `Application::RegisterBaselineCommands()`, filters by "Boot" in transitions |
| 4 | Update `BootUIPageModule` to query stages + expose to JS | BootUIPageModule DoStart populates stage list, JS binding returns names | Todo | sonnet | Bridge pattern: C++ queries DiaAPI, exposes array to Ultralight JS |
| 5 | Update Boot UI HTML to render dynamic menu from binding | Menu shows manifest stages | Todo | sonnet | Receives stage list from C++, no direct DiaAPI access |
| 6 | Fix `RequestLaunchLevel` to use `levelName` param | Navigate to arbitrary stage works | Todo | haiku | One-line change: `TransitionTo(StringCRC(levelName))` |
| 7 | Add `--scenario` glob filter to orchestrator CLI | Filtered run executes subset | Todo | sonnet | fnmatch on plan's scenario array before pytest |
| 8 | Add `--list` flag to orchestrator CLI | Prints scenario names, exits 0 | Todo | haiku | Prints plan entries (optionally filtered) |
| 9 | Verify: Boot → DummyStage still works after refactor | Smoke scenario passes | Todo | sonnet | Regression gate |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | Stage names, module type IDs, checkpoint names all use StringCRC. Boot menu converts JS string → StringCRC for TransitionTo. |
| PD-004 | No STL in public APIs | DiaAPI command returns Dia JSON types. Module interfaces use Dia containers. |
| PD-006 | VS project files source of truth | New module files added to CluicheTest.vcxproj per child spec. |
| PD-007 | C++20 required | constexpr StringCRC, standard features used. |
| PD-009 | Output under Cluiche/out/ | Session logs, metrics in `Cluiche/out/CluicheTest/sessions/`. |
| PD-010 | .diagame root, .diastage for stages | Test stages added to manifest as `.diastage` entries. Loader updated to parse `transitions[]` from `.diastage` JSON. `dia.manifest.stages` reads from the in-memory manifest (already loaded by framework). |
| AD-001 (CT) | Three PUs | Test stage modules live on appropriate PU (per child spec; physics=SimPU, others=MainPU). |
| AD-004 (CT) | Test levels included | Test stages ARE test levels — direct expression of this decision. |
| AD-005 (CT) | App is testbed not product | Test stages are purely for engine validation. |
| SD-TS-001 | One manifest stage per feature | Each child spec adds exactly one stage. |
| SD-TS-002 | Checkpoints in DoStart, auto-clear on stop | AC-S2 enforces this. Pattern code shows RegisterCheckpoints in DoStart. |
| SD-TS-003 | Metrics for threshold assertions | AC-S3 enforces this. Pattern code shows RegisterMetrics in DoStart. |
| SD-TS-004 | All stages return to Boot | AC-S1 enforces `transitions: ["Boot"]`. Star topology maintained. |
| SD-TS-005 | Individual stages are feature specs | Each concrete stage gets its own spec inheriting this template. |
| CTS-001 | Scenarios describe CluicheTest behavior | Scenarios navigate CluicheTest stages, validate CluicheTest checkpoints. |
| CTS-002 | Smoke is gate for all other scenarios | Test stage scenarios depend on smoke passing first (plan ordering). |
| SD-AUT-002 | Module-scoped checkpoint lifetime | Checkpoints auto-clear when stage module stops. |
| SD-AUT-003 | Navigation hold re-arms after transition | Orchestrator waits for hold after each navigate_to. |
| SD-AUT-005 | Consistent {success, data/error} envelope | All DiaAPI responses including `dia.manifest.stages` use envelope. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Boot Menu | Should the DiaAPI command filter stages to only those navigable FROM Boot, or return all stages? | Only stages with Boot in their `transitions` array. Boot can reach them, they can reach Boot (SD-TS-004). Avoids showing stages unreachable from the current position. |
| 2 | CLI Filter | Should `--scenario` match on filename only or full relative path? | Full relative path from plan root (e.g., `scenarios/cluichetest/rigidbody2d/test_*.py`). Allows filtering by directory (feature area) or by filename. Plan JSON is single source of truth — `--list` shows plan contents, not filesystem discovery. |
| 3 | Determinism | AC-S7 requires identical results on repeated navigation. What if a stage uses randomness? | Stages must NOT use randomness (or must use a fixed seed reset in DoStart). Determinism is required for checkpoint validation. Add to shared ACs as a constraint. |
| 4 | Ordering | Should the plan enforce scenario execution order (smoke first, then test stages)? | Yes. Plan JSON lists scenarios in order. Orchestrator runs sequentially. Smoke failure aborts the run (CTS-002). |
| 5 | Boot Menu UX | Should the Boot menu show stage status (passed/failed from last run)? | Out of scope for this spec. Future enhancement. Menu is navigation-only. |
| 6 | DiaAPI Location | Where does the `dia.manifest.stages` command handler live? | In `Application::RegisterBaselineCommands()` (DiaApplicationFlow). Reads the Application's loaded manifest stage list. Registered alongside `dia.app.report`, `dia.app.quit`. |
| 7 | Asset Hot-Reload | If a test stage is added to manifest but its module isn't compiled, what happens? | Module registration fails at startup. DiaApplicationFlow logs an error for unknown module types. The stage still appears in the menu but navigation would fail. This is acceptable — it's a development-time issue, not a runtime correctness concern. |
| 8 | Manifest Loader | `.diastage` JSON currently has no `transitions` field — what's the migration path? | Add `transitions` as optional array (defaults to empty). Update loader to parse it into `StageDeclaration::transitions`. Existing `.diastage` files without the field continue to load (just won't appear in Boot menu until field is added). No version bump needed — additive change. |
| 9 | Checkpoint Validation | Does `dia.automation.validate` exist server-side, or is it only in the Python DiaClient wrapper? | Exists in AutomationService: `RunCheckpoint(name)` is the C++ implementation. The DiaAPI JSON command routes to it. Confirmed present — no gap. |
| 10 | Metrics Access | How does a test stage module acquire a MetricRegistry reference? | Via `ModuleRef<AutomationModule>` — AutomationModule exposes `GetMetricRegistry()`. Stages don't interact with MetricRegistry directly; they call `automation.EmitMetric(name, value)` which handles registration and emission. |

## Status

`Approved` — 2026-05-22
