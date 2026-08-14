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
| DiaGridVisibility | VisibilityGroupId+VisibilityState, GridVisibilitySystem, Update pass (shadowcasting + dirty flags + chunkSize), CanSee, GetCellState, GetVisibleEntities, IVisibilityChangeObserver, Test Utilities | DiaPathfinding ✅, DiaEntitySpatial ✅, diaentitytemplate ✅, DiaCore ✅ |
| DiaGridVisibilityVisualDebugger | GridVisibilityDebugDomain (template), Cell State Drawer, Sight Radii Drawer, Shadowcast Boundary Drawer, GetJSONState + group selector. Plan: [diagridvisibilityvisualdebugger.plan.md](specs/applications/dia/systems/diagridvisibilityvisualdebugger/diagridvisibilityvisualdebugger.plan.md) | DiaGridVisibility ✅, DiaDebugDomain ✅, DiaVisualDebugger ✅, DiaEntitySpatial ✅, DiaCore ✅ |

---

### Standalone Features (system Done, feature Approved)

| Item | Notes | Depends On |
|------|-------|-----------|
| ~~Visual Debugger Panel Stats~~ | ~~Done. All 13 domains wired (asset domain task 12 blocked → moved to backlog). 338/338 tests green. Plan: [debugger-impl.plan.md](specs/applications/dia/systems/diadebugdomain/debugger-impl.plan.md)~~ | ~~DiaDebugDomain ✅~~ |
| ~~Visual Debugger Domain Stats Tests~~ | Add TDD RED stats-field assertions to all 13 domain test files (extend existing `*_JSONState` suites — do not create new files). Each test targets the specific `stats.xxx` fields each `debugger-impl.plan.md` task populates. Also add drawer-name assertions for tasks 13–15 (Scene2D split → 3 drawers, LightRangesDrawer, IK2D/Lighting3D label renames). Run gate: `dia run googletest --filter="*DebugDomain*_JSONState_Stats*"`. Add these after the impl work lands to avoid conflicts. Full plan: `.claude/plans/deep-stargazing-aurora.md`. | Visual Debugger Panel Stats impl done |

---

## Spec Work Needed (Draft or unset — review/approve before building)

### Visual Debugger Domains

These are new `DiaXxxVisualDebugger` system specs — each is its own module implementing `IDebugDomain` (Done ✅). Build order is flexible; highest-value ones first. Research + audit: `docs/research/visual_debugger_redesign/`.

| Item | Group | Value | Spec | Notes |
|------|-------|-------|------|-------|
| ~~DiaSteeringVisualDebugger~~ | Navigation | High | Approved ✅ | Prereq: `SteeringSystem::VisitAgents()` debug accessor. World-space: velocity arrows, separation radius, detection boxes. |
| ~~DiaPathfindingVisualDebugger~~ | Navigation | High | Approved ✅ | Prereq: confirm `PathGrid::VisitCells()`/`GetWidth()`/`GetHeight()`. World-space: path polyline, start/goal markers, grid passability. |
| ~~DiaFlowFieldVisualDebugger~~ | Navigation | High | Approved ✅ | Prereq: `FlowField::GetWidth()`/`GetHeight()`. World-space: per-cell direction arrows, reachability overlay. |
| ~~DiaStateMachineVisualDebugger~~ | AI / Behavior | High | Approved ✅ | No prereqs. Panel: states, transition history, guard pass/fail via `ITransitionListener`. |
| ~~DiaRulesVisualDebugger~~ | AI / Behavior | Medium | Approved ✅ | No prereqs outstanding. |
| ~~DiaHTNVisualDebugger~~ | AI / Behavior | Medium | Approved ✅ | Prereq: `HTNPlan::GetCurrentTaskIndex()`. |
| ~~DiaAIBudgetVisualDebugger~~ | AI / Behavior | Medium | Approved ✅ | Prereq: `AIBudgetResult.perSystem` timing array + `AIBudgetScheduler::GetLastBudgetMs()`. |
| ~~DiaBlackboardVisualDebugger~~ | AI / Behavior | Low–Med | Approved ✅ | No prereqs. Panel: slot table via `VisitSlots()`, hex fallback, optional per-type formatters. |
| DiaMailboxVisualDebugger | AI / Behavior | Low–Med | Approved ✅ | Prereq: `Mailbox::GetTypeStatsByIndex(int)`. Panel: per-type queue fill, drop counters. |
| DiaBehaviourTreeVisualDebugger | AI / Behavior | Medium | — | Needs `/spec-system`. Panel: active node highlight, per-node tick result (Running/Success/Failure), per-entity tree cursor, time-slice resume state. Prereq: DiaBehaviourTree ✅. |

---

### Extend DebugGalleryTestStage

| Item | Plan | What's Needed |
|------|------|---------------|
| Add 9 new domains to DebugGalleryTestStageModule | [debug-gallery-extension.plan.md](specs/applications/cluichetest/systems/teststages/debug-gallery-extension.plan.md) | All 9 debugger plans Done; then extend existing stage (.h + .cpp + vcxproj) — 6 tasks, no new spec needed. Panel goes from 14 → 23 domain cards. |

---

### Other Spec Work

| Item | Spec | What's needed |
|------|------|---------------|
| DiaBehaviourTree | Approved ✅ | All features Draft — ready to move to Ready to Build once features are individually Approved. Spec: [diabehaviourtree.md](specs/applications/dia/systems/diabehaviourtree/diabehaviourtree.md). Depends on DiaBlackboard ✅, DiaAIBudget ✅, DiaCore/Timer ✅. |
| ~~BehaviourTreeTestStage~~ | — | Needs `/spec-feature` — CluicheTest e2e stage for DiaBehaviourTree. Entities driven by a shared tree definition with different blackboard instances; demonstrates Sequence, Selector, Parallel, and Decorator nodes in-world. Conditions read blackboard keys; actions dispatch via DiaOrder. Paused/resumed trees visible via DiaHTNVisualDebugger-style panel. Prerequisite: DiaBehaviourTree ✅. |
| GridVisibilityTestStage | — | Needs `/spec-feature` — visually appealing CluicheTest e2e stage for DiaGridVisibility. Two groups of entities moving through a terrain grid with walls. Per-cell colour overlay: black=Unexplored, grey=Revealed, white=Visible (per group, togglable). Entities colour-coded by group; enemy entities hidden in non-Visible cells. Observer sight radii shown as debug circles. Demonstrates CanSee, GetCellState, shared group vision, and LOS blocking in real time. Prerequisite: DiaGridVisibility ✅, DiaGridVisibilityVisualDebugger ✅. |
| ~~DiaSensorInspector~~ | — | Needs `/spec-system` — dockable ImGui panel per entity; shows all four `SensorResultsComponent` arrays (sight, proximity, damage, sound) with per-entry distance/angle/timestamp, tick-countdown and stale/fresh state per sensor component, and the resulting blackboard slots (`ThreatBoard`, `AwarenessBoard`). Answers "why didn't this entity react?" for AI debugging. Depends on DiaSensor ✅, DiaEditor. |
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

### Blocked on Linux/CMake migration

| Item | Blocked by | Notes |
|------|-----------|-------|
| ~~Clang-Tidy analysis~~ | CMake migration (compile_commands.json) | Unblocked by C2 (Foundation CMake pilot) — see DiaArchitecture system below |
| ~~TSan (ThreadSanitizer)~~ | Linux target (WSL2 CI) | Only reliable race detector for Main/Render/Sim threading model; TSan doesn't run on Windows |

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| RenderTechnique asset type | Layer-level rendering policy (blend mode, post-process like bloom/distortion). Layers reference a technique by name; renderer resolves at draw time. Needs `/spec-feature` under DiaGraphics or DiaBgfx once the scene system lands. |
| Camera2D controller (pan/zoom/reset) | Application-side input→Camera2D wiring for CluicheTest stages (keyboard pan, scroll zoom, home-key reset). Unblocked once coord2d-debug-overlay ships Camera2D + renderer integration. |
