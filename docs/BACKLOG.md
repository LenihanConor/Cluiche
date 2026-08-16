# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| System | Features | Depends On |
|--------|----------|------------|
| DiaSimTime | Foundation, SimTimeDomain, SimTimeScheduler, DiaSimTime umbrella (all Approved). Phase 5 SimTimeAnalytical is a deferred follow-on feature spec (Planned). Plan: [diasimtime.plan.md](specs/applications/dia/systems/diasimtime/diasimtime.plan.md) — 19 tasks, 4 phases, 3 blocking open Qs resolved. **Pre-dispatch:** confirm Sim/Render/Main affinity for 7 undeclared modules (Task 1.5a). Hard-cutover migration (~70 files) touches all Module subclasses. | DiaApplicationFlow ✅, DiaCore ✅, DiaAIBudget ✅ (absorbed), DiaStreams ✅, DiaObservation ✅ |
| DiaMessageBus | core-bus, entity-router-registration, flush-adapters, frame-ledger, schema-browser, eventdispatcher-removal, module-and-build | DiaMailbox ✅, DiaStreams ✅, DiaApplicationFlow ✅, DiaObservation ✅ |
| DiaGridVisibility | VisibilityGroupId+VisibilityState, GridVisibilitySystem, Update pass (shadowcasting + dirty flags + chunkSize), CanSee, GetCellState, GetVisibleEntities, IVisibilityChangeObserver, Test Utilities | DiaPathfinding ✅, DiaEntitySpatial ✅, diaentitytemplate ✅, DiaCore ✅ |
| DiaGridVisibilityVisualDebugger | GridVisibilityDebugDomain (template), Cell State Drawer, Sight Radii Drawer, Shadowcast Boundary Drawer, GetJSONState + group selector. Plan: [diagridvisibilityvisualdebugger.plan.md](specs/applications/dia/systems/diagridvisibilityvisualdebugger/diagridvisibilityvisualdebugger.plan.md) | DiaGridVisibility ✅, DiaDebugDomain ✅, DiaVisualDebugger ✅, DiaEntitySpatial ✅, DiaCore ✅ |

---

## Spec Work Needed (Draft or unset — review/approve before building)

### Visual Debugger Domains

These are new `DiaXxxVisualDebugger` system specs — each is its own module implementing `IDebugDomain` (Done ✅). Build order is flexible; highest-value ones first. Research + audit: `docs/research/visual_debugger_redesign/`.

| Item | Group | Value | Spec | Notes |
|------|-------|-------|------|-------|
| ~~DiaBehaviourTreeVisualDebugger~~ | AI / Behavior | Medium | Approved ✅ | Panel: active node highlight, per-node tick result (Running/Success/Failure), per-entity tree cursor, resume state. Uses `IBehaviourTreeEventListener` to track node visits in real time. Prereq: DiaBehaviourTree ✅. Spec: [diabehaviourtreevisualdebugger.md](specs/applications/dia/systems/diabehaviourtreevisualdebugger/diabehaviourtreevisualdebugger.md). |

---

### Other Spec Work

| Item | Spec | What's needed |
|------|------|---------------|
| ~~DiaBehaviourTree~~ | Approved ✅ | In Progress — **Next: Task 7 — Decorator execution** (Tasks 1–6 done). Plan: [diabehaviourtree.plan.md](specs/applications/dia/systems/diabehaviourtree/diabehaviourtree.plan.md). |
| ~~BehaviourTreeTestStage~~ | — | Approved ✅ — 3-guard patrol/alert/chase loop; shared BT asset + independent blackboards; all 5 node types; DiaOrder (`GuardMoveOrder`); debugger panel auto-shown; 5 checkpoints; 10 tasks. Spec: [behaviourtree-test-stage.md](specs/applications/cluichetest/systems/teststages/behaviourtree-test-stage.md). |
| AICalloutTestStage | [plan](specs/applications/cluichetest/systems/teststages/aicallout-test-stage.plan.md) | Approved ✅ — 2-faction stage: 6 wandering emitters (3 blue + 3 red), 6 responders (3 blue + 3 red); emit/claim/release/TTL lifecycle; faction-coloured ring overlay; cross-faction violations metric. Plan ready, ODQs resolved. 9 tasks. |
| GridVisibilityTestStage | — | Needs `/spec-feature` — visually appealing CluicheTest e2e stage for DiaGridVisibility. Two groups of entities moving through a terrain grid with walls. Per-cell colour overlay: black=Unexplored, grey=Revealed, white=Visible (per group, togglable). Entities colour-coded by group; enemy entities hidden in non-Visible cells. Observer sight radii shown as debug circles. Demonstrates CanSee, GetCellState, shared group vision, and LOS blocking in real time. Prerequisite: DiaGridVisibility ✅, DiaGridVisibilityVisualDebugger ✅. |
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

### Blocked on asset service injection

| Item | Blocked by | Notes |
|------|-----------|-------|
| AssetRuntimeDebugDomain stats | `DiaAssetRuntime` service not injected into domain constructor | `GetJSONState()` emits `stats: {}`. Wire a service ref into `AssetRuntimeDebugDomain` then add `assetCount`, `loadedCount`, `pendingCount` stats fields. |

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| RenderTechnique asset type | Layer-level rendering policy (blend mode, post-process like bloom/distortion). Layers reference a technique by name; renderer resolves at draw time. Needs `/spec-feature` under DiaGraphics or DiaBgfx once the scene system lands. |
| Camera2D controller (pan/zoom/reset) | Application-side input→Camera2D wiring for CluicheTest stages (keyboard pan, scroll zoom, home-key reset). Unblocked once coord2d-debug-overlay ships Camera2D + renderer integration. |
