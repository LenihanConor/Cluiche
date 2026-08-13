# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| System | Features | Depends On |
|--------|----------|------------|
| DiaMessageBus | core-bus, entity-router-registration, flush-adapters, frame-ledger, schema-browser, eventdispatcher-removal, module-and-build | DiaMailbox ✅, DiaStreams ✅, DiaApplicationFlow ✅, DiaObservation ✅ |
| DiaAICallout | Callout Emit, Callout Query, Claim/Release, TTL Expiry, Test Utilities | DiaEntitySpatial ✅, DiaGeometry2D ✅, DiaCore ✅ |

---

### Standalone Features (system Done, feature Approved)

_Nothing here._

---

## Spec Work Needed (Draft or unset — review/approve before building)

### Visual Debugger Domains

These are new `DiaXxxVisualDebugger` system specs — each is its own module implementing `IDebugDomain` (Done ✅). Build order is flexible; highest-value ones first. Research + audit: `docs/research/visual_debugger_redesign/`.

| Item | Group | Value | Notes |
|------|-------|-------|-------|
| DiaSteeringVisualDebugger | Navigation | High | World-space: per-agent velocity + desired-velocity arrows, separation radii, detection boxes. All data in `SteeringSystem::GetOutput()` — no API gaps. |
| DiaPathfindingVisualDebugger | Navigation | High | World-space: path polyline via `PathResult::ToWorldPositions()`, grid passability overlay, start/goal markers. Reuses hex/square draw patterns from EntitySpatial. |
| DiaFlowFieldVisualDebugger | Navigation | High | World-space: per-cell direction arrows via `FlowField::Sample()`. Direct analogue of existing `ScalarFieldGradientOverlay`. |
| DiaStateMachineVisualDebugger | AI / Behavior | High | Panel: `IStateMachineInspectable` was designed for this — all states, transitions, history, guard pass/fail. Low effort. |
| DiaRulesVisualDebugger | AI / Behavior | Medium | Panel: `RuleSet::GetLastFireReport()` already populated every frame. Ready to consume. |
| DiaHTNVisualDebugger | AI / Behavior | Medium | Panel: plan task sequence + cursor + diverged/pending state. Needs one `GetCurrentIndex()` accessor added to `HTNPlannerComponent`. |
| DiaAIBudgetVisualDebugger | AI / Behavior | Medium | Panel: budget bar + systems run/deferred. Needs per-system timing added to `AIBudgetResult` first. |
| DiaBlackboardVisualDebugger | AI / Behavior | Low–Med | Panel: slot table via `VisitSlots()`. Would benefit from per-type display format callbacks (not blocking, shows hex otherwise). |
| DiaMailboxVisualDebugger | AI / Behavior | Low–Med | Panel: per-type queue fill + drop counters. Needs type-erased descriptor list added to `Mailbox` internals first. |

---

### Other Spec Work

| Item | Spec | What's needed |
|------|------|---------------|
| DiaBehaviourTree | — | Needs `/spec-system` — data-driven behaviour tree evaluator. Nodes: Sequence, Selector, Parallel, Decorator (inverter, repeater, cooldown, guard), Leaf (action/condition). Trees defined in JSON, loaded at runtime. Leaf nodes reference DiaBlackboard keys for conditions and DiaOrder for execution. Supports tree sharing (many entities, one tree definition, different blackboard instances). Time-sliced: trees pause mid-evaluation and resume next tick. Depends on DiaBlackboard ✅, DiaOrder ✅, DiaCore/Timer ✅, DiaStreams ✅. |
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
| RenderTechnique asset type | Layer-level rendering policy (blend mode, post-process like bloom/distortion). Layers reference a technique by name; renderer resolves at draw time. Needs `/spec-feature` under DiaGraphics or DiaBgfx once the scene system lands. |
| Camera2D controller (pan/zoom/reset) | Application-side input→Camera2D wiring for CluicheTest stages (keyboard pan, scroll zoom, home-key reset). Unblocked once coord2d-debug-overlay ships Camera2D + renderer integration. |
