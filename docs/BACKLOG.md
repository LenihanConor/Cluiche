# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| System | Features | Depends On |
|--------|----------|------------|
| DiaEconomyInspector | EconomyInstancesSource, EconomyModifiersSource, EconomyEventsSource, EconomySchemaSource, dockable editor plugin | DiaEconomy ✅, DiaEditor |
| DiaScalarFieldInspector | Dockable editor panel for scalar field inspection | DiaScalarField ✅, DiaEditor |
| ~~DiaObjective~~ | Data-driven gameplay goal tracking — completion conditions, progress, per-faction objectives, chained objectives | DiaCondition ✅, DiaCore ✅, diaentitytemplate ✅ |
| ~~DiaEntitySpawner~~ | `SpawnRequest` API, `SpawnEmitterComponent` (rate/burst/cap/lifetime/radius), `EntitySpawnerModule` on SimPU, DiaObservation coverage, GoogleTest suite, CluicheTest E2E visual stage — **plan ready (10 tasks)** | diaentity ✅, DiaReflect ✅, DiaSerializer ✅ |
| ~~DiaSaveGame~~ | ISaveable contract, SaveRegistry, SaveConfig + slot management, SaveContext/LoadContext, SaveManifest, async I/O, versioning + migration, Observer events, test utilities | DiaSerializer ✅, DiaCore ✅ |
| ArenaTestStage (CluicheTest) | **In Progress (5/11 tasks done)** — scaffold + JSON assets + header + OnStart/LoadTriggerScript + SpawnWave/EnemyAgent done; remaining: DoUpdate frame loop (T6), metrics (T7), ImGui visuals (T8), DoStop (T9), pytest (T10), E2E verify (T11) | DiaTriggerScript ✅, DiaObjective ✅, DiaBlackboard ✅, DiaStateMachine ✅, DiaUtilityAI ✅, DiaRules ✅ |
| ~~DiaTriggerScript~~ | Data-driven level events — spatial/temporal/state/count triggers, four action types, ITriggerActionHandler extension point, TriggerFiredEvent on DiaStreams | DiaCondition ✅, DiaGeometry2D ✅, DiaEntitySpatial ✅, DiaStreams ✅, DiaObjective (ChangeObjectiveState action only) |

---

### Standalone Features (system Done, feature Approved)

_Nothing here._

---

## Spec Work Needed (Draft or unset — review/approve before building)

| Item | Spec | What's needed |
|------|------|---------------|
| DiaBehaviourTree | — | Needs `/spec-system` — data-driven behaviour tree evaluator. Nodes: Sequence, Selector, Parallel, Decorator (inverter, repeater, cooldown, guard), Leaf (action/condition). Trees defined in JSON, loaded at runtime. Leaf nodes reference DiaBlackboard keys for conditions and DiaOrder for execution. Supports tree sharing (many entities, one tree definition, different blackboard instances). Time-sliced: trees pause mid-evaluation and resume next tick. Depends on DiaBlackboard ✅, DiaOrder ✅, DiaCore/Timer ✅, DiaStreams ✅. |
| ~~DiaEntitySpawnerVisualDebugger~~ | — | Needs `/spec-system` — editor overlay for DiaEntitySpawner: spawn radius, spawn rate, live count, despawn reason per emitter. Follow-up to DiaEntitySpawner. Depends on DiaEntitySpawner, DiaEditor. |
| ~~DiaTriggerScript~~ | — | Needs `/spec-system` — data-driven scripted game events. Trigger types: spatial (entity enters region), temporal (time elapsed), state (blackboard condition met), count (N entities killed). Actions: spawn entities, give resources, change objective state, fire events. Per-map JSON definitions. One-shot and repeating triggers. Depends on DiaCondition ✅, DiaGeometry2D ✅, DiaStreams ✅, DiaObjective. |
| DiaGridVisibility | — | Needs `/spec-system` — per-cell fog-of-war on a grid. Each cell carries a per-faction state (unexplored / revealed / visible). Entities have a sight radius; cells within radius are marked visible each frame, fading to revealed when out of range. LOS blocking against terrain cells (walls, elevation). Shared vision within factions. Publishes visibility-change events via DiaStreams (unit spotted, unit lost). Prerequisite for: minimap data layer, cover/LOS combat modifiers. Depends on DiaGeometry2D ✅, DiaStreams ✅, DiaEntitySpatial ✅. |
| DiaSensorInspector | — | Needs `/spec-system` — dockable ImGui panel per entity; shows all four `SensorResultsComponent` arrays (sight, proximity, damage, sound) with per-entry distance/angle/timestamp, tick-countdown and stale/fresh state per sensor component, and the resulting blackboard slots (`ThreatBoard`, `AwarenessBoard`). Answers "why didn't this entity react?" for AI debugging. Depends on DiaSensor ✅, DiaEditor. |
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

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| ArenaTestStage spec → backlog | Promoted from loose end to Approved spec + backlog entry 2026-08-07. See Ready to Build above. |
| RenderTechnique asset type | Layer-level rendering policy (blend mode, post-process like bloom/distortion). Layers reference a technique by name; renderer resolves at draw time. Needs `/spec-feature` under DiaGraphics or DiaBgfx once the scene system lands. |
| Camera2D controller (pan/zoom/reset) | Application-side input→Camera2D wiring for CluicheTest stages (keyboard pan, scroll zoom, home-key reset). Unblocked once coord2d-debug-overlay ships Camera2D + renderer integration. |
