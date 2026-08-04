# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| System | Features | Depends On |
|--------|----------|------------|
| ~~DiaAIBudget~~ | IAIBudgetedSystem, AIBudgetScheduler, AIBudgetModule, Budget Metrics | DiaApplicationFlow ✅, DiaMetrics ✅, DiaCore ✅ |
| ~~DiaCondition~~ | IConditionContext, ConditionRegistry, ConditionExpr, ConditionGuardAdapter, Test Utilities | DiaCore ✅, DiaStateMachine ✅ |
| ~~DiaRules~~ | RuleActionRegistry, RuleSet, RuleSetComponent, Test Utilities | DiaCondition ✅ |
| ~~DiaUtilityAI~~ | ResponseCurve, ActionDef, UtilitySet, AsyncEvaluation, GroupConsideration, UtilitySetComponent, ScoreOverlay, TestUtilities | DiaCondition ✅, DiaRules ✅, DiaAIBudget ✅ |
| ~~DiaHTN~~ | OperatorRegistry, RuleActionBridge, HTNDomain, HTNPlan, SyncPlanner, AsyncPlanning, HTNPlannerComponent, TestUtilities | DiaCondition ✅, DiaRules ✅, DiaAIBudget ✅ |
| ~~DiaFlowField~~ | CFlowFieldGraph concept, SquareFlowAdapter + HexFlowAdapter, FlowField (per-cell direction array), ComputeFlowField (sync Dijkstra), FlowFieldCache (named dirty-flag store, Invalidate / InvalidateAll / InvalidateRegion), Test Utilities | DiaPathfinding ✅, DiaCore ✅, DiaMaths ✅ |
| ~~DiaScalarField~~ | CFieldTopology concept, SquareFieldTopology + HexFieldTopology, UniformDecayPolicy, RulesPropagationPolicy (header-only adaptor), blocked cell mask, static modifier map, double-buffering, value clamping, write shapes (point/radial/box), gradient query, spatial queries (FindLocalMaxima/FindCellsAboveThreshold), multi-field weighted combine, ScalarFieldOverlay (optional adaptor), Test Utilities | DiaCore ✅, DiaMaths ✅; optional: DiaRules ✅ (RulesPropagationPolicy adaptor), DiaVisualDebugger ✅ (ScalarFieldOverlay adaptor) |

---

### Standalone Features (system Done, feature Approved)

| Feature | System | Depends On |
|---------|--------|------------|
| ~~AI Personality~~ | DiaUtilityAI | DiaUtilityAI core built first |

---

## Spec Work Needed (Draft or unset — review/approve before building)

| Item | Spec | What's needed |
|------|------|---------------|
| ~~DiaAIDecisionInspector~~ | — | Needs `/spec-system` — CluicheEditor panel showing per-entity AI decision state: blackboard slots → condition results → fired rules → utility scores in one vertical read. Depends on DiaAIBudget, DiaCondition, DiaRules, DiaUtilityAI all built + at least one CluicheTest entity running all four systems. DiaEntityInspector (stub) is a soft prerequisite for entity selection. |
| DiaSpatialEntityIndex | — | Needs `/spec-feature` under DiaGeometry2D — bridge between `ISpatialStructure<Entity>` (SpatialGrid/Quadtree/BVH) and DiaEntity's Domain. Maintains a per-frame-updated spatial index of entities by reading a transform component; exposes `QueryCircle(point, radius)`, `QueryRegion(rect)`, `QueryKNearest(point, k)` returning `Entity` handles. Prerequisite for: DiaSteering, DiaGridVisibility, DiaTargeting, DiaSensor, DiaSignal, AoE resolution. |
| DiaGridVisibility | — | Needs `/spec-system` — per-cell fog-of-war on a grid. Each cell carries a per-faction state (unexplored / revealed / visible). Entities have a sight radius; cells within radius are marked visible each frame, fading to revealed when out of range. LOS blocking against terrain cells (walls, elevation). Shared vision within factions. Publishes visibility-change events via DiaStreams (unit spotted, unit lost). Prerequisite for: minimap data layer, cover/LOS combat modifiers. Depends on DiaGeometry2D ✅, DiaStreams ✅, DiaSpatialEntityIndex. |
| DiaBehaviourTree | — | Needs `/spec-system` — data-driven behaviour tree evaluator. Nodes: Sequence, Selector, Parallel, Decorator (inverter, repeater, cooldown, guard), Leaf (action/condition). Trees defined in JSON, loaded at runtime. Leaf nodes reference DiaBlackboard keys for conditions and DiaOrder for execution. Supports tree sharing (many entities, one tree definition, different blackboard instances). Time-sliced: trees pause mid-evaluation and resume next tick. Depends on DiaBlackboard ✅, DiaOrder ✅, DiaCore/Timer ✅, DiaStreams ✅. |
| DiaSensor | — | Needs `/spec-system` — standardised entity perception framework. Sensor types: SightSensor (LOS-based detection), ProximitySensor (radius check), DamageSensor (react to incoming damage), SoundSensor (react to events within range). Each sensor writes results to its entity's DiaBlackboard. Sensors tick at configurable rates (not every frame). Depends on DiaGeometry2D ✅, DiaBlackboard ✅, DiaSpatialEntityIndex. |
| DiaSignal | — | Needs `/spec-system` — spatial + faction-scoped signalling for AI coordination without squad membership. Signals (e.g. "help needed at (x,y)") have a broadcast radius, decay timer, and faction filter. Nearby friendly AI can read signals and react. Enables emergent coordination without explicit squad formation. Depends on DiaGeometry2D ✅, DiaStreams ✅, DiaSpatialEntityIndex. |
| DiaEconomy | — | Needs `/spec-system` — named resource pools (gold, wood, food, supply) per faction. Resources earned (harvest, production, time tick), spent (training, construction, ability costs), and capped (storage limits, population caps). Publishes rate-of-change metrics for AI budgeting and UI. Prerequisite for DiaTechTree, DiaSpawner (resource-gated spawning). Depends on DiaCore/Timer ✅, DiaStreams ✅. |
| DiaSpawner | — | Needs `/spec-system` — configurable entity spawning. Spawn points produce entities on timers, triggers, or wave schedules. Wave definitions: count, composition (unit type ratios), interval, difficulty scaling, spawn pattern. Data-driven wave tables (JSON). Publishes wave-start/wave-complete events. Depends on DiaCore/Timer ✅, DiaStreams ✅, DiaEntity ✅. |
| DiaObjective | — | Needs `/spec-system` — data-driven game goal tracking. Objectives have completion conditions (destroy target, hold area N seconds, collect N resources), progress tracking, and rewards. Conditions composable via DiaCondition. Supports primary/secondary/optional classification and chained objectives. Tracks per-faction objectives independently. Depends on DiaCondition ✅, DiaStreams ✅. |
| DiaTriggerScript | — | Needs `/spec-system` — data-driven scripted game events. Trigger types: spatial (entity enters region), temporal (time elapsed), state (blackboard condition met), count (N entities killed). Actions: spawn entities, give resources, change objective state, fire events. Per-map JSON definitions. One-shot and repeating triggers. Depends on DiaCondition ✅, DiaGeometry2D ✅, DiaStreams ✅, DiaObjective. |
| DiaSaveGame | — | Needs `/spec-system` — serialises complete game state (entity positions, components, resources, fog state, research progress, order queues, AI state) to JSON or binary. Handles versioning (older saves load in newer game with migration). Every gameplay system implements a save/load interface; DiaSaveGame orchestrates correct order (pause sim, serialise all, resume). Depends on DiaSerializer ✅, DiaEntity ✅. |
| RenderTestPlugin (CluicheEditor) | — | Needs `/spec-system` — visual debugger panel: wipe slider, region grid, expectation authoring, AI triage panel, render targets. DiaRenderTest CLI Pipeline ✅ unblocked. Mockup: [render_test_debugger_mockup.html](research/render_offline_test/render_test_debugger_mockup.html). Research: [render_offline_test/summary.md](research/render_offline_test/summary.md) |

---

## E2E Orchestration Stack

Architecture redesigned 2026-05-20. Source of truth: **[docs/research/e2e_testing/design-decisions.md](research/e2e_testing/design-decisions.md)**.

### Deferred

| # | Item | Notes |
|---|------|-------|
| 6b | Extract AutomationModuleBase into DiaAutomation | Evaluate from working code after 2+ apps use it |
| DiaRig3D system | feature spec exists (`skeleton-and-pose.md`) | Needs `/spec-system` — Bone3D, Skeleton3D, Pose3D, FK, `SkeletonComponent3D`, `Rig3DAsset`. Mirrors DiaRig2D. | 
| DiaAnimation3D system | feature spec exists (`clip-and-player.md`) | Needs `/spec-system` — AnimationClip3D, ClipPlayer3D, STEP/LINEAR/CUBICSPLINE, `AnimationComponent3D`. glTF animation import is build-time only. |
| DiaStateMachineEditor system | TBD | Needs `/spec-system` — editor plugin for state machine visual debugging + design-time editing. Depends on DiaStateMachine ✅, DiaEditor |
| DiaSkinning3D | TBD — needs `/spec-system` | `skinning-palette` feature already Approved; needs own system spec. SkinningManager, per-frame Matrix34 palettes, `skinningPaletteIndex` on draw commands. | DiaAnimation3D, DiaGraphics3D |
---

### Blocked on Linux/CMake migration

| Item | Blocked by | Notes |
|------|-----------|-------|
| Clang-Tidy analysis | CMake migration (compile_commands.json) | Unblocked by C2 (Foundation CMake pilot) — see DiaArchitecture system below |
| TSan (ThreadSanitizer) | Linux target (WSL2 CI) | Only reliable race detector for Main/Render/Sim threading model; TSan doesn't run on Windows |

---

## E2E Test Coverage Gaps

### Quick wins (no new C++ work needed)

| Item | Notes |
|------|-------|
| ~~Add RigidBody2D + SoftBody2D scenarios to default.json~~ | ~~`scenarios/cluichetest/rigidbody2d/test_rigidbody2d_settle.py` and `softbody2d/test_softbody2d_settle.py` exist on disk but are missing from `plans/cluichetest/default.json`. Add both entries.~~ |
| Dedicated scenario: EntityTestStage | 6 checkpoints (spawn_complete, query_correct, hierarchy_valid, destroy_cascade, mailbox_received, lifecycle_complete) — richest stage, generic runner only. Write `scenarios/cluichetest/entity/test_entity_stage.py`. |
| Dedicated scenario: Scene2DTestStage | 5 checkpoints (scene.loaded, cameras_hydrated, lights_hydrated, entities_spawned, layers_resolved). Write `scenarios/cluichetest/scene2d/test_scene2d_stage.py`. |
| Dedicated scenario: Animation2DTestStage | 2 checkpoints (animation2d.clips_completed, pose_correct). Write `scenarios/cluichetest/animation2d/test_animation2d_stage.py`. |
| Dedicated scenario: IK2DTestStage | 3 checkpoints (ik2d.two_bone_converged, fabrik_converged, look_at_accurate). Write `scenarios/cluichetest/ik2d/test_ik2d_stage.py`. |
| Dedicated scenario: Geometry2DTestStage | 1 checkpoint. Write `scenarios/cluichetest/geometry2d/test_geometry2d_stage.py`. |
| Dedicated scenario: TestAssetRuntimeStage | 2 checkpoints (all_loaded, clean_reload). Write `scenarios/cluichetest/asset_runtime/test_asset_runtime_stage.py`. |

### Needs investigation

| Item | Notes |
|------|-------|
| ~~E2E suite: UIUltralightTestStage hard crashes app on generic runner~~ | ~~Fixed in commit `6d78c890` — UIModule no longer destroys UISystem on DoStop; Ultralight process-lifetime globals survive stage transitions.~~ |

### Needs new C++ stage

| Item | Notes |
|------|-------|
| ~~AIDecisionTestStage + scenario~~ | ~~Integration proof: DiaCondition + DiaRules + DiaUtilityAI + DiaAIBudget. Built 2026-07-31.~~ |
| ~~AIHTNTestStage + scenario~~ | ~~Integration proof: HTN sync + diverge/replan + async + RuleActionBridge + AIBudget. Built 2026-07-31.~~ |
| PathfindingTestStage + scenario | Spec drafted (pathfinding-test-stage.md). Full nav stack: A* + FlowField + Steering; 3 agents, dynamic obstacle re-route, gradient arrows, trails; 5 checkpoints + pytest scenario. |

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| ~~`dia diagnose --last-run` CLI command~~ | Automate crash triage: find latest session log, extract last module transition + all ERROR entries, show incomplete E2E report, check `%LocalAppData%\CrashDumps`. Saves the manual log-digging cycle after every crash. |
| RenderTechnique asset type | Layer-level rendering policy (blend mode, post-process like bloom/distortion). Layers reference a technique by name; renderer resolves at draw time. Needs `/spec-feature` under DiaGraphics or DiaBgfx once the scene system lands. |
| Camera2D controller (pan/zoom/reset) | Application-side input→Camera2D wiring for CluicheTest stages (keyboard pan, scroll zoom, home-key reset). Unblocked once coord2d-debug-overlay ships Camera2D + renderer integration. |
| ~~E2E suite: RigidBody2DTestStage / SoftBody2DTestStage checkpoint failures~~ | ~~Doubled frame budgets (900→1800, 600→1200) and pytest timeouts (15s→100s, 14s→70s) to match ~50ms Debug SimPU frame rate.~~ |
