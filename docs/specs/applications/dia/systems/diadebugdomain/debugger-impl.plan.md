**Spec:** @docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
**Status:** Done

---

## Context

Every IDebugDomain class and draw class already exists. Every `GetJSONState()` already runs but emits `stats: {}` (empty object). No draw class files are missing. CSS spacing in `debug-panel.html` already matches AC-16 — no HTML changes required.

**Audit findings (2026-08-13):**

| Finding | Impact |
|---------|--------|
| All 14 `IDebugDomain` wrappers exist and compile | GetJSONState()-only work |
| All draw class .h/.cpp files exist | No new draw files except LightRangesDrawer and Scene2D split |
| `stats: {}` is empty in every domain | All panel cards show no live data |
| `MeshStatsDrawer` caches 11 stat fields but has no public Get\* | Blocks task 4 — accessors needed first |
| `IKSolver`: GetChainCount/GetChainId/GetChainStart/EndBoneIndex/GetWorldTransforms exist | IK2D stats can be wired; check for solve-result accessors |
| `AnimationEvaluator`: GetSourceCount/GetSourceId/GetClipPlayer/GetSpringChain exist | Animation2D stats can be wired |
| `UtilitySet::GetLastFrameScores()` exists under DIA\_DEBUG | UtilityAI actions\[\] can be wired |
| `LightRangesDrawer`: does not exist anywhere | New file required (task 14) |
| `Scene2DDebugDomain` owns only `SceneOverviewDrawer` | Spec calls for 3 drawers — split required (task 13) |
| `UtilityAIDebugDomain::HasWorldDrawers()` returns `true` in code | Spec says `false` — fix in task 2 |
| CSS spacing (`debug-panel.html`) already correct | AC-16 is satisfied; no HTML changes |
| `RigidBody2DDebugDomain.cpp` line 143: "Phase 2 populates body counts here" | Intentional stub — clear to implement |
| `EntityDebugDomain.cpp` line 151: "Phase 2 moves Stats here" | Intentional stub — clear to implement |

**Drawer label mismatches** — spec names vs. current `kDrawerLabels[]`:

| Domain | Code label | Spec label |
|--------|-----------|-----------|
| IK2D | `"Bones"` | `"ChainBones"` |
| IK2D | `"Joints"` | `"ChainJoints"` |
| IK2D | `"Arrows"` | `"ChainArrows"` |
| IK2D | `"Reach"` | `"ReachCircles"` |
| Geometry2D | `"Labels"` | `"Labels"` ✓ |
| Geometry2D | `"AABB"` | `"AABB"` ✓ |
| Lighting3D | `"Widgets"` | `"PositionWidgets"` |
| Lighting3D | `"Paths"` | `"PathArcs"` |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **MeshStatsDrawer: add public Get\* accessors** | Compile only | Done | haiku | — |
| 2 | **UtilityAIDebugDomain::GetJSONState()** + fix `HasWorldDrawers()` → `false` | Panel shows per-action score bars | Done | haiku | Code done. Test `UtilityAIDebugDomain_Identity.IdsAndGroupAreCanonical` still expects `true` — needs `EXPECT_FALSE` update in TestUtilityAIDebugDomain.cpp:75. |
| 3 | **RigidBody2DDebugDomain::GetJSONState()** | Panel shows body counts | Done | haiku | Used `Size()` not `GetCount()` for DynamicArray. |
| 4 | **Mesh3DDebugDomain::GetJSONState()** | Panel shows mesh draw stats | Done | haiku | — |
| 5 | **IK2DDebugDomain::GetJSONState()** | Panel shows chain table | Done | sonnet | Added `lastSolved`/`lastIterationCount`/`lastEndEffectorError` fields to `IKSolver`. Also fixed `TwoBoneSolver.DegenerateZeroDistance` crash (d=0 NaN path — early-out guard added in `SolveTwoBone`). |
| 6 | **Rig2DDebugDomain::GetJSONState()** | Panel shows "Bones: N" | Done | haiku | — |
| 7 | **Animation2DDebugDomain::GetJSONState()** | Panel shows "Active: clip @ T" | Done | sonnet | — |
| 8 | **SoftBody2DDebugDomain::GetJSONState()** | Panel shows body counts | Done | haiku | — |
| 9 | **Coord3DDebugDomain::GetJSONState()** | Panel shows camera stats | Done | haiku | Added `eye`, `forward`, `fovYDeg`, `nearZ`, `farZ` cached fields to `Camera3D` (DiaGraphics3D). |
| 10 | **Coord2DDebugDomain::GetJSONState()** | Panel shows "Cursor: (X, Y)" | Done | haiku | Added `mCursorWorld` to `DebugLayerManager`; `Coord2DCursorDrawer` writes it each frame via non-const manager ref. |
| 11 | **EntityDebugDomain::GetJSONState()** | Panel shows entity counts | Done | haiku | — |
| 12 | **AssetRuntimeDebugDomain::GetJSONState()** | Panel shows asset load stats | Blocked | haiku | Domain constructor has no asset service refs. Needs asset service injected before this can be wired. |
| 13 | **Scene2DDebugDomain: split into 3 drawers** (CamerasDrawer, LightsDrawer, LayerBoundsDrawer) | Panel shows cam/light/layer breakdown | Done | sonnet | Code done + vcxproj updated. **8 tests failing** — all reference removed `kScene2DOverview` layer. `TestScene2DDebugDomain.cpp` needs rewriting to use `kScene2DCameras`, `kScene2DLights`, `kScene2DLayerBounds` and assert 3 drawers instead of 1. |
| 14 | **LightRangesDrawer: new draw class** + update Lighting3DDebugDomain | LightRanges drawer + panel light counts | Done | sonnet | Code done + vcxproj updated. **6 tests failing** — see task 16 notes. |
| 15 | **Fix drawer label names** (IK2D + Lighting3D) | Panel labels match spec | Done | haiku | `"Widgets"`→`"PositionWidgets"`, `"Paths"`→`"PathArcs"` in Lighting3D; IK2D bones/joints/arrows/reach renamed. |
| 16 | **Fix 15 failing unit tests** | All `*DebugDomain*` tests green | Done | haiku | Scene2D: all kScene2DOverview refs → 3-drawer structure. Lighting3D: drawer count 2→3, labels PositionWidgets/PathArcs, isolation tests disable kLightRanges. UtilityAI: EXPECT_FALSE(HasWorldDrawers()). 338/338 tests green. |
| 17 | **HTML drawer compliance** — update `debug-panel.html` Alpine.js cards to render new `stats` fields for each domain | Panel cards show live data | Done | sonnet | Added formatStatValue, renderArrayTable, upgraded renderStats to handle scalars/arrays/nested objects. IK2D chains and Animation2D layers render as .st-table mini-tables. |
| 18 | **Commit all work** (one commit per task, per implement protocol) | `git log` shows 15+ task commits | Done | haiku | 15 task commits + docs/specs commit. All code tasks committed in order. |
