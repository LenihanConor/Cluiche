# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| System | Features | Depends On |
|--------|----------|------------|
| ~~DiaEntitySpatial~~ | SpatialComponent, EntitySpatialIndex, EntitySpatialModule, Test Utilities | DiaGeometry2D ✅, DiaEntity ✅ |
| DiaEconomy | EconomySchema (JSON asset), EconomyInstance (per-participant runtime pools), Transaction API (Earn/Spend/Transfer/Tick), Observer events, Modifier stack (simple + conditional via DiaCondition), Derived resource hook, Test Utilities | DiaCore ✅; optional: DiaCondition ✅ (conditional modifiers) |
| ~~DiaSensor~~ | SensorResultsComponent, SightSensorComponent, ProximitySensorComponent, DamageSensorComponent, SoundSensorComponent, SensorBlackboardAdapter, SensorModule, Test Utilities | DiaEntitySpatial ✅, DiaBlackboard ✅, diaentitytemplate ✅, DiaCore ✅, DiaMaths ✅ |
| ~~DiaAICallout~~ | Callout (emit/query/claim/release), CalloutHandle, CalloutRegistry, TTL expiry, Test Utilities | DiaEntitySpatial, DiaGeometry2D ✅, DiaCore ✅ |

---

### Standalone Features (system Done, feature Approved)

| Feature | System | Depends On |
|---------|--------|------------|

---

## Spec Work Needed (Draft or unset — review/approve before building)

| Item | Spec | What's needed |
|------|------|---------------|
| DiaScalarFieldVisualDebugger | `diascalarfieldvisualdebugger.md` | Gradient arrow overlay needs arrowhead triangle + hex axial-to-world fix. Heatmap + layer names already implemented. |
| DiaScalarFieldInspector | `diascalarfieldinspector.md` | Spec Approved. Ready to build. Depends on DiaScalarField ✅, DiaEditor. |
| ~~DiaGridVisibility~~ | — | Needs `/spec-system` — per-cell fog-of-war on a grid. Each cell carries a per-faction state (unexplored / revealed / visible). Entities have a sight radius; cells within radius are marked visible each frame, fading to revealed when out of range. LOS blocking against terrain cells (walls, elevation). Shared vision within factions. Publishes visibility-change events via DiaStreams (unit spotted, unit lost). Prerequisite for: minimap data layer, cover/LOS combat modifiers. Depends on DiaGeometry2D ✅, DiaStreams ✅, DiaEntitySpatial. |
| DiaBehaviourTree | — | Needs `/spec-system` — data-driven behaviour tree evaluator. Nodes: Sequence, Selector, Parallel, Decorator (inverter, repeater, cooldown, guard), Leaf (action/condition). Trees defined in JSON, loaded at runtime. Leaf nodes reference DiaBlackboard keys for conditions and DiaOrder for execution. Supports tree sharing (many entities, one tree definition, different blackboard instances). Time-sliced: trees pause mid-evaluation and resume next tick. Depends on DiaBlackboard ✅, DiaOrder ✅, DiaCore/Timer ✅, DiaStreams ✅. |
| ~~DiaSensor~~ | — | Needs `/spec-system` — standardised entity perception framework. Sensor types: SightSensor (LOS-based detection), ProximitySensor (radius check), DamageSensor (react to incoming damage), SoundSensor (react to events within range). Each sensor writes results to its entity's DiaBlackboard. Sensors tick at configurable rates (not every frame). Depends on DiaGeometry2D ✅, DiaBlackboard ✅, DiaEntitySpatial. |
| DiaSensorVisualDebugger | — | Needs `/spec-system` — world-space overlay drawing sight cones (range + half-angle arc) and proximity circles per entity. Implements `IVisualDebugger`, reads `SensorResultsComponent` directly; highlights entities currently in sight/proximity results. `#ifdef DIA_DEBUG` guarded. Follows `DiaGeometry2DVisualDebugger` / `DiaRigidBody2DVisualDebugger` pattern. Prerequisite: DiaSensor ✅. |
| DiaSensorInspector | — | Needs `/spec-system` — dockable ImGui panel per entity; shows all four `SensorResultsComponent` arrays (sight, proximity, damage, sound) with per-entry distance/angle/timestamp, tick-countdown and stale/fresh state per sensor component, and the resulting blackboard slots (`ThreatBoard`, `AwarenessBoard`). Answers "why didn't this entity react?" for AI debugging. Prerequisite: DiaSensor ✅, DiaEditor. |
| DiaEconomy | — | Needs `/spec-system` — named resource pools (gold, wood, food, supply) per faction. Resources earned (harvest, production, time tick), spent (training, construction, ability costs), and capped (storage limits, population caps). Publishes rate-of-change metrics for AI budgeting and UI. Prerequisite for DiaTechTree, DiaSpawner (resource-gated spawning). Depends on DiaCore/Timer ✅, DiaStreams ✅. |
| DiaSpawner | — | Needs `/spec-system` — configurable entity spawning. Spawn points produce entities on timers, triggers, or wave schedules. Wave definitions: count, composition (unit type ratios), interval, difficulty scaling, spawn pattern. Data-driven wave tables (JSON). Publishes wave-start/wave-complete events. Depends on DiaCore/Timer ✅, DiaStreams ✅, DiaEntity ✅. |
| DiaObjective | — | Needs `/spec-system` — data-driven game goal tracking. Objectives have completion conditions (destroy target, hold area N seconds, collect N resources), progress tracking, and rewards. Conditions composable via DiaCondition. Supports primary/secondary/optional classification and chained objectives. Tracks per-faction objectives independently. Depends on DiaCondition ✅, DiaStreams ✅. |
| DiaTriggerScript | — | Needs `/spec-system` — data-driven scripted game events. Trigger types: spatial (entity enters region), temporal (time elapsed), state (blackboard condition met), count (N entities killed). Actions: spawn entities, give resources, change objective state, fire events. Per-map JSON definitions. One-shot and repeating triggers. Depends on DiaCondition ✅, DiaGeometry2D ✅, DiaStreams ✅, DiaObjective. |
| DiaSaveGame | — | Needs `/spec-system` — serialises complete game state (entity positions, components, resources, fog state, research progress, order queues, AI state) to JSON or binary. Handles versioning (older saves load in newer game with migration). Every gameplay system implements a save/load interface; DiaSaveGame orchestrates correct order (pause sim, serialise all, resume). Depends on DiaSerializer ✅, DiaEntity ✅. |
| RenderTestPlugin (CluicheEditor) | — | Needs `/spec-system` — visual debugger panel: wipe slider, region grid, expectation authoring, AI triage panel, render targets. DiaRenderTest CLI Pipeline ✅ unblocked. Mockup: [render_test_debugger_mockup.html](research/render_offline_test/render_test_debugger_mockup.html). Research: [render_offline_test/summary.md](research/render_offline_test/summary.md) |
| DiaEconomyInspector (CluicheEditor) | — | Needs `/spec-system` — dockable editor panel for runtime economy inspection. Shows active `EconomyInstance` list; per-instance resource bars, current pool values, income rates, active modifier stack; rolling event log (Earn/Spend/Clamped/Transfer); read-only cost table browser. Prerequisite: DiaEconomy ✅, DiaEditor. Spec after DiaEconomy ships. |

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

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| RenderTechnique asset type | Layer-level rendering policy (blend mode, post-process like bloom/distortion). Layers reference a technique by name; renderer resolves at draw time. Needs `/spec-feature` under DiaGraphics or DiaBgfx once the scene system lands. |
| Camera2D controller (pan/zoom/reset) | Application-side input→Camera2D wiring for CluicheTest stages (keyboard pan, scroll zoom, home-key reset). Unblocked once coord2d-debug-overlay ships Camera2D + renderer integration. |
