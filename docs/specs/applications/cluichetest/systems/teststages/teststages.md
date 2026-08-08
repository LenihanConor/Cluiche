# System Spec: TestStages

## Parent Application
@docs/specs/applications/cluichetest/cluichetest.md

## Research
@docs/research/e2e_testing/design-decisions.md (Section 7, 8, item #6)

## Purpose

TestStages provides dedicated game stages within CluicheTest, each exercising a specific engine feature (RigidBody2D, Entity system, animation, etc.) and registering checkpoints for E2E validation via DiaAutomation. Each test stage sets up a controlled scene, runs the engine feature under test, and registers named checkpoints that the pytest orchestrator can trigger to validate correctness.

The system defines the **pattern** — one stage per feature, one module per stage, checkpoints as the validation contract. Individual test stages are added as feature specs over time.

## Responsibilities

- **Define test stages** — each stage exercises one engine feature in a controlled environment
- **Register checkpoints** — each stage module registers named checkpoints with DiaAutomation for orchestrator-triggered validation
- **Emit observability signals** — test stages emit metrics and traces (via DiaObservation) that pytest scenarios can assert on via metric threshold checks
- **Manage test scene lifecycle** — DoStart sets up the scene (entities, physics bodies, etc.), DoUpdate runs the simulation, DoStop tears down
- **Extend the CluicheTest manifest** — each new test stage adds a stage entry to the v3 manifest with its navigation transitions

## Non-Responsibilities

- **Orchestration logic** — owned by pytest scenarios in `Tools/orchestrator/`
- **Automation infrastructure** — owned by DiaAutomation
- **Engine feature implementation** — owned by the respective Dia systems (DiaRigidBody2D, diaentitytemplate, etc.)
- **General gameplay** — DummyStage handles general testbed behaviour; TestStages are targeted validation
- **Shared test utilities / base classes** — if a pattern emerges across multiple stages, extract at that point (not up front)

## Public Interfaces

### Pattern: TestStage Module

Each test stage follows this pattern:

```cpp
// Cluiche/CluicheTest/Modules/TestStages/<Feature>StageModule.h
namespace CluicheTest {

    class RigidBody2DTestModule : public Dia::ApplicationFlow::Module
    {
    public:
        static constexpr Dia::Core::StringCRC kTypeId{"RigidBody2DTestModule"};

        RigidBody2DTestModule(const Dia::Core::StringCRC& instanceId);

    protected:
        StartResult DoStart() override;
        void DoUpdate(float deltaTime) override;
        StopResult DoStop() override;

    private:
        void SetupScene();
        void RegisterCheckpoints(Dia::Automation::AutomationService& automation);
    };
}
DIA_MODULE(RigidBody2DTestModule);
```

### Checkpoint Contract

Each stage registers checkpoints in `DoStart()` via the AutomationService (obtained through `ModuleRef<AutomationModule>`):

```cpp
StartResult RigidBody2DTestModule::DoStart()
{
    SetupScene();

    auto* automation = mAutomationRef->GetService();
    automation->RegisterCheckpoint(this, StringCRC("rigid_body.circle_settled"),
        [this]() -> CheckpointResult {
            bool settled = mCircleBody->IsAsleep();
            return { settled, settled ? "circle at rest" : "circle still moving", 0.0f };
        });

    return StartResult::kReady;
}
```

Checkpoints auto-clear when the module stops (DiaAutomation SD-AUT-002).

### Manifest Integration

Each test stage adds entries to the v3 manifest:

```json
{
    "name": "RigidBody2DStage",
    "transitions": ["Boot"],
    "auto_advance": false
}
```

Navigation: Boot can transition to any test stage, and each test stage can return to Boot. The orchestrator drives all navigation via `dia.automation.navigate_to`.

### Observability

Test stage modules emit:
- **Metrics** — registered with `DiaMetrics::MetricRegistry` (e.g., `cluichetest.rigidbody.step_count`, `cluichetest.rigidbody.settle_time_ms`)
- **Traces** — `DIA_TRACE_ZONE` on per-frame physics steps for timing validation
- **Logs** — standard `DIA_LOG_*` for state transitions and errors

Pytest scenarios can assert on metrics via the metric threshold fixture (item #8).

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Test Stage Infrastructure | Shared ACs, Boot menu manifest-driven nav, CLI scenario filtering, child spec template | @docs/specs/applications/cluichetest/systems/teststages/test-stage-infrastructure.md | Approved |
| RigidBody2D Stage | Physics sim under real PU timing, settle detection, determinism | @docs/specs/applications/cluichetest/systems/teststages/rigidbody2d-stage.md | Approved |
| Animation2D Stage | Sequential clip playback on Rig2D, asset-loaded, golden pose verification | @docs/specs/applications/cluichetest/systems/teststages/animation2d-stage.md | Approved |
| AssetRuntime Stage | Multi-type concurrent load, two-entry reload validation, handle lifecycle | @docs/specs/applications/cluichetest/systems/teststages/assetruntime-stage.md | Approved |
| StateMachineAnim Stage | StateMachine driving animation transitions, multi-entity, asset-loaded clips | @docs/specs/applications/cluichetest/systems/teststages/statemachineanim-stage.md | Approved |
| SoftBody2D Stage | Rope + cloth settle under real timing, RB anchor coupling | @docs/specs/applications/cluichetest/systems/teststages/softbody2d-stage.md | Approved |
| Visual Feedback | In-stage HUD bottom bar + Boot menu pass/fail badges | @docs/specs/applications/cluichetest/systems/teststages/visual-feedback.md | Approved |
| Visual Debugger Module | VisualDebuggerModule + VisualDebuggerConsoleModule pattern; IVisualDebugger::DrawImGui(); RigidBody2D drawers wired | @docs/specs/applications/cluichetest/systems/teststages/visual-debugger-module.md | Approved |
| Geometry2D Stage | Gallery of all shape primitives, intersection pair colour-coding, spatial structure overlays (BVH/Quadtree/SpatialGrid) | @docs/specs/applications/cluichetest/systems/teststages/geometry2d-stage.md | Approved |
| EntityTest Stage | Entity spawn/destroy/hierarchy/query/mailbox/component lifecycle under real PU timing; `TransformComponent` + `VisualTestRenderComponent`; 6 checkpoints | @docs/specs/applications/cluichetest/systems/teststages/entity-test-stage.md | Approved |
| UIUltralight Stage | Page load, JS↔C++ bridge (4 bound methods), pixel buffer non-empty, mouse injection, round-trip value, deterministic reload; 6 checkpoints | @docs/specs/applications/cluichetest/systems/teststages/ui-ultralight-stage.md | Approved |
| Scene2D Stage | Scene load pipeline — .diascene parse, camera/light registry hydration, entity spawn with instance_data, LayerTable resolution; 5 checkpoints | @docs/specs/applications/cluichetest/systems/teststages/scene2d-stage.md | Approved |
| IK2D Stage | Three IK solvers (two-bone right wing, FABRIK left wing, look-at head) on dragon skeleton; 3 convergence checkpoints; IK2DVisualDebugger drawers wired | @docs/specs/applications/cluichetest/systems/teststages/ik2d-stage.md | Done |
| Mesh3DRenderSystem Stage | Multiple draw commands per frame (3 cubes + 1 glTF), unknown mesh ID silently skipped, 60-frame pass checkpoint | @docs/specs/applications/cluichetest/systems/teststages/mesh3d-render-system-stage.md | Approved |
| PathfindingTestStage | Full navigation stack integration: A* + FlowField + Steering; 3 agents, dynamic obstacle re-route, gradient flow arrows, agent trails, goal pulse; 5 checkpoints | @docs/specs/applications/cluichetest/systems/teststages/pathfinding-test-stage.md | Approved |
| ScalarFieldTestStage | Tactical influence maps: Blue + Red faction fields (UniformDecayPolicy + RulesPropagationPolicy), combined front-line field, WriteRadial/WriteBox/WritePoint, walls, swamp terrain, burst events, FindLocalMaxima, FindCellsAboveThreshold, GetGradient, heatmap + gradient overlays; 8 checkpoints | @docs/specs/applications/cluichetest/systems/teststages/scalarfield-test-stage.md | Approved |
| EntitySpatialTestStage | 32 moving agents (4 layer categories), live dirty-flag re-index at 30 Hz, all 5 query shapes active per frame, layer-mask toggle at frame 60, entity destruction sweep at frame 90, query shape overlays + agent trails; 8 checkpoints | @docs/specs/applications/cluichetest/systems/teststages/entityspatial-test-stage.md | Approved |
| ArenaTestStage | Multi-system AI/progression E2E: all 4 DiaTriggerScript trigger types, DiaObjective 3-wave prerequisite chain, DiaBlackboard shared state, per-enemy StateMachine + UtilityAI + Rules; 6 checkpoints | @docs/specs/applications/cluichetest/systems/teststages/arena-test-stage.md | Approved |

## Platform Primitives Used

- **DiaApplicationFlow** — Module base class, stages, ModuleRef, DIA_MODULE macro
- **DiaAutomation** — AutomationService for checkpoint registration
- **DiaObservation** — DIA_LOG_*, DIA_TRACE_ZONE, DIA_PROFILE_SCOPE
- **DiaMetrics** — MetricRegistry for quantitative signals
- **DiaCore** — StringCRC, containers, time

## Dependencies on Other Systems

**Required:**
- **DiaApplicationFlow** — Module lifecycle, stage membership, manifest integration
- **DiaAutomation** — Checkpoint registration (via AutomationModule's service)
- **DiaObservation** — Logging, tracing for observability

**Per-stage (varies):**
- **DiaRigidBody2D** — for RigidBody2DStage
- **diaentitytemplate** — for EntityTestStage
- **DiaAnimation2D** — for Animation2DStage
- **DiaSoftBody2D** — for SoftBody2DStage
- (etc.)

**Consumers:**
- **Tools/orchestrator/scenarios/cluichetest/** — pytest scenarios that navigate to stages and validate checkpoints
- **GoogleTests** — may unit-test individual checkpoint functions in isolation

## Out of Scope

- **Orchestrator logic** — pytest owns scenario flow
- **AutomationModule itself** — lives in CluicheGameBaseline, not here
- **Engine feature bugs** — test stages validate, they don't fix
- **Visual regression** — no screenshot comparison
- **Performance benchmarking** — metric thresholds are for correctness gates, not perf CI
- **Shared base class** — premature; extract if a pattern emerges after 3+ stages

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-TS-001 | One manifest stage per test feature | Each stage is independently navigable by the orchestrator. Keeps stages isolated — one engine feature per stage. Matches design-decisions §8 (module lifetime = checkpoint scope). | All features | Accepted | Yes |
| SD-TS-002 | Checkpoints registered in DoStart, auto-cleared on stop | Module-scoped lifetime per DiaAutomation SD-AUT-002. No manual cleanup. Stage entering = checkpoints available; stage leaving = checkpoints gone. | All features | Accepted | Yes |
| SD-TS-003 | Test stages emit metrics for threshold assertions | Enables pytest metric fixture (item #8) to assert on quantitative signals without needing C++ checkpoint logic for simple numeric checks. | All features | Accepted | Yes |
| SD-TS-004 | Each test stage returns to Boot | Simple star topology: Boot is the hub, test stages are leaves. Orchestrator navigates Boot → Stage → Boot → next Stage. Avoids complex transition graphs. | All features | Accepted | Yes |
| SD-TS-005 | Individual stages are feature specs | System defines the pattern; each concrete stage gets its own `/spec-feature` with specific ACs, checkpoint names, and scene setup. | All features | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for IDs | Stage names, module type IDs, checkpoint names all StringCRC. |
| PD-004 | Platform | No STL in public APIs | Module interfaces use Dia containers. Checkpoint callbacks use std::function (callable, not container). |
| PD-005 | Platform | x64 only | No platform-specific code. |
| PD-006 | Platform | VS project files source of truth | Test stage modules added to CluicheTest.vcxproj. |
| PD-007 | Platform | C++20 required | Standard C++ features used. |
| AD-001 (CT) | CluicheTest | Three PUs (Main/Render/Sim) | Test stage modules live on MainPU or SimPU depending on what they exercise. Physics stages may need SimPU modules. |
| AD-004 (CT) | CluicheTest | Test levels included | Test stages ARE test levels — directly aligned with this decision. |
| AD-005 (CT) | CluicheTest | App is testbed not product | Test stages are the primary expression of the testbed role. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Manifest | Adding many stages to the manifest — does this bloat Boot's `transitions[]` array? | Boot transitions to all test stages. This is fine — the v3 manifest supports arbitrary transition lists. If it gets unwieldy (20+ stages), consider a hub stage. Not a concern for the first 5-10 stages. |
| 2 | Module placement | Should test stage modules live on MainPU or SimPU? | Depends on the feature. Physics test stages need SimPU (where physics runs). Most others use MainPU. The feature spec for each stage decides. System-level: no constraint. |
| 3 | Checkpoint naming | Should checkpoint names be globally unique or scoped by stage? | Globally unique (DiaAutomation rejects duplicates). Convention: `<feature>.<checkpoint_name>` (e.g., `rigid_body.circle_settled`). Since only one test stage is active at a time, collisions are unlikely anyway. |
| 4 | Scene determinism | How do test stages ensure deterministic results for checkpoint validation? | Fixed timestep (SimPU already runs at fixed 30Hz). Deterministic initial conditions (hardcoded positions, not random). No external input during test. Checkpoint validates after N frames or a convergence condition. |
| 5 | Transition graph | With SD-TS-004 (all stages return to Boot), can the orchestrator skip Boot and go directly stage-to-stage? | No — the manifest's `transitions[]` only allows Stage → Boot. The orchestrator must navigate Stage → Boot → next Stage. This is intentional: Boot is the clean slate between tests (retained modules get a fresh start). |

## Status

`Done` (2026-05-22) — All 6 child feature specs Approved.
