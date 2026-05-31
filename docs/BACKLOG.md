# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| System | Spec | Features | Depends On |
|--------|------|----------|------------|
| DiaGraphics3D | [diagraphics3d.md](specs/systems/dia/diagraphics3d.md) ✅ | `graphics-3d-types` — Camera3D, lights, Mesh3DDrawCommand, Mesh3DFrameData, FrameData3D; new `Dia/DiaGraphics3D/` module; `Dia::Graphics3D::` namespace. Needs system specs for DiaMesh3D/Rig3D/Animation3D/Skinning3D/Scene3D before Phase 2 implements. | DiaMaths (Matrix44), DiaGeometry3D, DiaGraphics ✅ |
| DiaBgfx3D | [diabgfx3d.md](specs/systems/dia/diabgfx3d.md) ✅ | `3d-renderers` — Canvas3D, MeshRenderer, SkinnedMeshRenderer, ShadowRenderer, MaterialRegistry, MeshGpuCache, 6 shaders; `Dia::Bgfx3D::` namespace; Phase 2 ship gate. Blocked on Phase 1 + DiaScene3D chain. | DiaBgfx (Phase 1), DiaScene3D chain, DiaGraphics3D ✅ |
| DiaMesh3D | TBD — needs `/spec-system` | `mesh-asset-and-loader` feature already Approved (parent currently `render-backend`); needs own system spec. glTF 2.0 static+skinned mesh loading, `Mesh3DAsset`, `IAssetTypeHandler` plug-in. | DiaMaths, DiaGeometry3D, DiaAssetRuntime |
| DiaRig3D | TBD — needs `/spec-system` | `skeleton-and-pose` feature already Approved; needs own system spec. Bone3D, Skeleton3D, Pose3D, FK, `SkeletonComponent3D`. Mirrors DiaRig2D. | DiaMesh3D |
| DiaAnimation3D | TBD — needs `/spec-system` | `clip-and-player` feature already Approved; needs own system spec. AnimationClip3D, ClipPlayer3D, glTF loader, STEP/LINEAR/CUBICSPLINE, `AnimationComponent3D`. | DiaRig3D |
| DiaSkinning3D | TBD — needs `/spec-system` | `skinning-palette` feature already Approved; needs own system spec. SkinningManager, per-frame Matrix34 palettes, `skinningPaletteIndex` on draw commands. | DiaAnimation3D, DiaGraphics3D |
| DiaScene3D | TBD — needs `/spec-system` | `scene-graph` feature already Approved; needs own system spec. Flat-list scene, Transform3D parent chains, frustum culling, `Submit(scene, frameData3D)`. | DiaSkinning3D, DiaGraphics3D, DiaGeometry3D |
| DiaArchitecture | [diaarchitecture.md](specs/systems/dia/diaarchitecture.md) ✅ | C7 → C1 → C2 → C3a (Dia modules, additive) → C3b (Cluiche apps + retirement, deferrable). C3a spec needs update to Draft→Approved; C3b is a new Draft spec. | None |

---

### Standalone Features (system Done, feature Approved)

| Feature | Spec | System | Notes |
|---------|------|--------|-------|
| per-app-bin-layout | [per-app-bin-layout.md](specs/features/dia/diapipeline/per-app-bin-layout.md) | DiaPipeline ✅ | |
| ~~module-metadata~~ | **Done** (2026-05-28) — `PUAffinity` bitmask; `TypeRegistry` stores `kAllowedPUs`+`kDescription`; `AddModule()` asserts on mismatch; `IApplicationInspectable` extended; 24 modules annotated; 14 tests. | DiaApplicationFlow | |
| ~~shared-debug-console~~ | **Done** (2026-05-28) — `VisualDebuggerModule` + `VisualDebuggerConsoleModule` always-active; layers persist across transitions with stageTag; per-stage tab bar in console; 7 tests. | DiaVisualDebugger | |

---

## Ready to Build (cont.)

---

## Spec Work Needed (Draft or unset — review/approve before building)

| Item | Spec | What's needed |
|------|------|---------------|
| DiaStateMachineEditor system | TBD | Needs `/spec-system` — editor plugin for state machine visual debugging + design-time editing. Depends on DiaStateMachine ✅, DiaEditor |
| Manifest heap modules | [manifest-heap-modules.md](specs/features/dia/diaapplicationflow/manifest-heap-modules.md) | `ApplicationManifestV3` is ~60 KB on the stack due to `DynamicArrayC<ModuleDeclaration,32>` inline storage. Spec written + plan ready. Blocked on `DynamicArrayC`/`DynamicArray` both using `memcpy` — nesting heap-owning containers is unsafe without fixing copy semantics. Options: reduce cap (32→16, 1-line, safe), or allocate the whole manifest on the heap at the call site. Deferred — low urgency. |

---

## E2E Orchestration Stack (build in dependency order)

Architecture redesigned 2026-05-20. Source of truth: **[docs/research/e2e_testing/design-decisions.md](research/e2e_testing/design-decisions.md)**.

### Core Stack (Done)

| # | Item | Status |
|---|------|--------|
| 0 | DiaAPI — JSON command path | **Done** (folded into #2) |
| 1 | DiaApplicationFlow — [transition-guards](specs/features/dia/diaapplicationflow/transition-guards.md) | **Done** |
| 2 | DiaApplicationFlow — [baseline-commands](specs/features/dia/diaapplicationflow/baseline-commands.md) | **Done** |
| 3 | [DiaAutomation](specs/systems/dia/diaautomation.md) system + AutomationModule | **Done** |
| 4 | [dia orchestrate](specs/features/dia/diacli/dia-orchestrate.md) CLI + pytest plugin | **Done** |
| 5 | CluicheTest [smoke scenario](specs/features/cluichetest/cluichetestscenarios/smoke-scenario.md) | **Done** — `dia orchestrate` passes end-to-end |
| 8 | [Metric threshold assertions](specs/features/dia/diaautomation/metric-assertions.md) — `dia.automation.get_metric` + pytest `assert_metric` fixture | **Done** (2026-05-24) |

### TestStages — Needs `/spec-feature` per stage

System spec: [teststages.md](specs/systems/cluichetest/teststages.md) (Approved). Pattern: one manifest stage per engine feature, one Module per stage, checkpoints as validation contract. Star topology: Boot → Stage → Boot.

**Candidate stages** (build in any order — independent once pattern established):

| Stage | Engine System | Validates | Checkpoint examples | Needs |
|-------|--------------|-----------|---------------------|-------|
| ~~RigidBody2DTestStage~~ | **Done** (2026-05-28) — `TestStageModuleBase` extracted; stage renamed; 10 circles settle; pytest scenario passes. | | |
| ~~AssetRuntimeTestStage~~ | **Done** (2026-05-28) — Migrated to `TestStageModuleBase`; stage renamed; multi-entry reload validation. | | |
| StateMachineTestStage | DiaStateMachine | State transitions, guard evaluation, event firing | `state_machine.reached_target`, `state_machine.guard_blocked` | `/spec-feature` |
| Animation2DTestStage | DiaAnimation2D + DiaRig2D | Clip playback, pose validation, blend weights | `animation.clip_complete`, `animation.pose_matches` | `/spec-feature` |
| SoftBody2DTestStage | DiaSoftBody2D | Rope/cloth stabilization, spring convergence | `soft_body.rope_settled` | `/spec-feature` |
| Geometry2DTestStage | DiaGeometry2D + DiaGeometry2DVisualDebugger | Gallery of all shape primitives, 6 intersection pair colour-coding, 4 spatial structure overlays, 5 IVisualDebugger drawers | `geometry2d.passed` | [Spec Approved](specs/features/cluichetest/teststages/geometry2d-stage.md). [Plan ready](specs/features/cluichetest/teststages/geometry2d-stage.plan.md) — 9 tasks. [Mockup](specs/features/cluichetest/teststages/geometry2d-stage.mockup.html) approved. |
| EntityTestStage | DiaEntity | Spawn/destroy/hierarchy/query/mailbox/lifecycle; `TransformComponent` + `VisualTestRenderComponent`; 6 checkpoints | `entity.spawn_complete`, `entity.query_correct`, `entity.hierarchy_valid`, `entity.destroy_cascade`, `entity.mailbox_received`, `entity.lifecycle_complete` | [Spec Approved](specs/features/cluichetest/teststages/entity-test-stage.md). [Plan ready](specs/features/cluichetest/teststages/entity-test-stage.plan.md) — 9 tasks (T-00 done). Uses `/new-cluichetest-stage` skill. Two-module split: EntityModule (reusable) + EntityTestModule (MainPU, checkpoints). |
| UIUltralightTestStage | DiaUIUltralight | Page load, JS↔C++ bridge (4 bound methods inc. round-trip), pixel buffer non-empty, mouse injection, deterministic reload; Alpine.js panel | `ui.page_loaded`, `ui.js_to_cpp_callback_fired`, `ui.round_trip_value_correct`, `ui.pixel_buffer_non_empty`, `ui.mouse_click_handled`, `ui.deterministic_reload` | [Spec Approved](specs/features/cluichetest/teststages/ui-ultralight-stage.md). [Plan ready](specs/features/cluichetest/teststages/ui-ultralight-stage.plan.md) — 10 tasks. |

**Scaffolding:** `dia scaffold stage <Name> --modules <...>` creates all 9 touch points in one command. `/new-cluichetest-stage` skill infers domain, runs the script, adds domain-specific C++. Domain patterns: Physics, Entity, Asset, Animation, StateMachine, Geometry.

### Deferred

| # | Item | Notes |
|---|------|-------|
| 6b | Extract AutomationModuleBase into DiaAutomation | Evaluate from working code after 2+ apps use it |
| 7 | CluicheEditor EditorAutomationModule | After 6b |
| 9 | ~~DiaScreenCapture — frame grab for mock comparison~~ | **Specced** — split into two features: [frame-capture-readback](specs/features/dia/diabgfx/frame-capture-readback.md) (ICanvas async readback, DiaBgfx ring buffer) + [captures](specs/features/dia/diaobservation/captures.md) (6th observation pillar, PNG write to session). Both Approved with plans. |
| 10 | **Eliminate remaining cross-PU statics** | `AutomationModule::GetStatic()`, `AssetServiceModule` static, `JobSystemModule::GetStatic()`, `VisualDebuggerModule::sLayerManager`. Migrate each to `ModuleRef` (move consumer to same PU) or `ServiceStream` (expose via stream). Policy: no new statics without sign-off. |

---

## Static Bug Detection Stack ✅ Done

Research complete: [docs/research/static_cpp_bug/](research/static_cpp_bug/). Bundle A chosen.

| Item | Status | Notes |
|------|--------|-------|
| `sanitizer-configs` | **Done** | `Debug-Asan` + `Debug-Ubsan` configs; `dia run googletest --config Asan\|Ubsan`; `dia check --tool=sanitizer` |
| `cppcheck-integration` | **Done** | `dia check`; SARIF output; `.cppcheck-suppressions.xml`; Cppcheck in winget.json |
| `ci-gate` | **Done** | `dia pipeline --stage static-analysis`; baseline diff; `delta.sarif`; 57 findings baselined |
| `dia-diagnose-loop` | **Dropped** | Agentic Claude fix loop — not needed; manual review of `delta.sarif` is sufficient |

### Blocked on Linux/CMake migration

| Item | Blocked by | Notes |
|------|-----------|-------|
| Clang-Tidy analysis | CMake migration (compile_commands.json) | Unblocked by C2 (Foundation CMake pilot) — see DiaArchitecture system below |
| TSan (ThreadSanitizer) | Linux target (WSL2 CI) | Only reliable race detector for Main/Render/Sim threading model; TSan doesn't run on Windows |

---

## DiaArchitecture — Domain-Oriented Module Structure + CMake Enforcement

Spec Approved: [diaarchitecture.md](specs/systems/dia/diaarchitecture.md). C3 is split into C3a (Dia-only, additive) and C3b (Cluiche apps + retirement). C3b can be deferred — C3a delivers architecture enforcement and full Clang-Tidy coverage.

**Target architecture:** 6-sub-layer Core + 4 domain vertical slices. Each domain owns its core modules and its visual debuggers/editor plugins. CMake `target_link_libraries` enforces the dependency rules currently only documented in YAML.

### Implementation sequence (build in order — each depends on previous)

| Step | Item | What's needed | Notes |
|------|------|--------------|-------|
| C7 | YAML layer formalisation | Add `layer:` field to all 55+ module docs | Documents the architecture; prerequisite for C1 |
| C1 | `dia check --tool=arch` | Python `#include` graph checker vs YAML `dependencies.forbidden` | Surfaces current silent violations before any CMake work |
| C2 | Foundation CMake pilot | `CMakeLists.txt` for Foundation sub-layer (DiaCore, DiaMaths, DiaGeometry2D/3D, DiaSerializer, DiaObservation) | `.vcxproj` stays; CMake additive; unlocks `compile_commands.json` |
| C3a | Dia CMake full | `CMakeLists.txt` for all 55 Dia modules; full INTERFACE aggregates; architecture enforcement live | **Additive** — `.vcxproj` kept, `dia run` unchanged, Cluiche.sln still works |
| C3b | Cluiche CMake + retirement | `CMakeLists.txt` for 4 Cluiche app projects; `Find*.cmake` for binary SDK externals; DiaCLI switches to `cmake --build`; atomic `.vcxproj` deletion | **Hard part** — external dep wiring (bgfx, CEF, Ultralight, SDL3). Can sit on backlog after C3a. |

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| Camera2D controller (pan/zoom/reset) | Application-side input→Camera2D wiring for CluicheTest stages (keyboard pan, scroll zoom, home-key reset). Unblocked once coord2d-debug-overlay ships Camera2D + renderer integration. |
| DiaAssetRuntime — Hot reload path | Asset Lifecycle Management adds `Failed → Loading` retry but no `Loaded → Loading → Loaded` path. Still need a `ReloadAsset(assetId)` for live iteration (future feature on top of lifecycle management). |
| `Dia::Core::Blackboard` — general-purpose key-value store | Identified during DiaStateMachine research; useful for AI, animation, gameplay. Needs `/spec-feature` under DiaCore. |
| ~~DiaSoftBody2D serializers~~ | **Done** (2026-05-28) — `DiaSoftBody2DSerializers.h` ships `WorldDef`, `RopeDef`, `ClothDef`. Follows `DiaRigidBody2DSerializers.h` pattern exactly. Non-owning anchor/world pointers skipped (same convention as RigidBody2D). |
| ~~Cross-PU data flow for debug UI~~ | **Folded into service-channel** — Shapes A+B+C all resolved in one feature (composite per PU-pair + ServiceStream). No follow-on features needed. |
| RigidBody2DStage visual debug — circles not visible | Stage runs and times out but falling circles are not rendering on screen. Ground (huge circle) draws correctly. Coordinate system (Y-UP renderer, pixel-scale physics) and gravity direction are set but circles still don't appear. Possible issues: circles too small relative to viewport, draw order, or drawer not picking up dynamic bodies. Needs investigation with logging or breakpoints. |
| Spatial cell inspector | [Spec Approved](specs/features/dia/diavisualdebugger/spatial-cell-inspector.md). SpatialGrid + HexGrid only. Click cell → highlight → ImGui inspector panel. Uses `ImGui::IsMouseClicked` in `DrawImGui()`. Duplicated per-drawer, no shared base. |
