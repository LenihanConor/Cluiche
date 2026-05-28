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
| DiaArchitecture | [diaarchitecture.md](specs/systems/dia/diaarchitecture.md) ✅ | C7 (YAML layer formalisation) → C1 (`dia check --tool=arch`) → C2 (Foundation CMake pilot) → C3 (full layered CMake enforcement). Must be done in order — C1 gates C3. | None |

---

### Standalone Features (system Done, feature Approved)

| Feature | Spec | System | Notes |
|---------|------|--------|-------|
| per-app-bin-layout | [per-app-bin-layout.md](specs/features/dia/diapipeline/per-app-bin-layout.md) | DiaPipeline ✅ | |
| stage-scaffold-simplification | [stage-scaffold-simplification.plan.md](specs/features/cluichetest/teststages/stage-scaffold-simplification.plan.md) | TestStages | Cross-cutting DX pass — 8→4 touch points. FrameStream auto-flush, pipeline auto-derive stages from catalogue + .diagame, Boot auto-transitions, staleness fix. Unblocks TestGeometry2DStage + future stages. 8 tasks. |
| ~~service-channel~~ | **Done** (2026-05-27) — `ServiceStream<T>` replaces all `GetStatic*()` singletons; EventStream frame-batching; composite FrameStream per PU-pair; 8 consolidated streams; `channels[]` array replaces `reads`/`writes` in manifests; editor UI updated; 5310/5311 GoogleTests pass. | | |
| replace-diasfml-with-sdl3 | [replace-diasfml-with-sdl3.md](specs/features/dia/diasdl/replace-diasfml-with-sdl3.md) | DiaSDL (new) | Replace DiaSFML with SDL3 window+input backend; enables Windows/Linux/Android/iOS. [Plan](specs/features/dia/diasdl/replace-diasfml-with-sdl3.plan.md) ready — 10 tasks. Start with T-00 (SDL3 submodule) then T-01 (Win32WndProcChain → DiaBgfx/Imgui/ via PowerShell). Key: `ListenForInputSources` moves to `IInputSource` in T-03. |
| ~~diabgfx-imgui-backend~~ | **Done** (2026-05-25) — BgfxImGuiBackend wired; deferred-init on render thread; ImGui input via Win32WndProcChain; both SFML and `BGFX_BACKEND=dx11` paths pass. | | |
| ~~diasfml-render-removal~~ | **Done** (2026-05-26) — SFML render path deleted; DiaSFML = window+input only; TextureHandler moved to DiaAssetRuntime (stb_image decode via DiaBgfx); bgfx unconditional; imgui core sources moved to DiaBgfx; Phase 1 ship gate (RB-016) closed. | | |
| ~~stale-ui-overlay-fix~~ | **Done** (2026-05-26) — UIOverlayRenderer::Composite early-returns on empty buffer; fixes DummyStage UI persisting into RigidBody2DStage after transition. 5 regression tests added. | | |
| ~~ToolbarPanelSwitcher~~ | **Done** (2026-05-22) — full-name pills + `⋯ +N` overflow dropdown, `ProjectContextButton` moved right. | | |

---


---

## Ready to Build (cont.)


---

## Spec Work Needed (Draft or unset — review/approve before building)

| Item | Spec | What's needed |
|------|------|---------------|
| DiaStateMachineEditor system | TBD | Needs `/spec-system` — editor plugin for state machine visual debugging + design-time editing. Depends on DiaStateMachine ✅, DiaEditor |
| Migrate manifest inner arrays to heap (`DynamicArray`) | TBD | `ApplicationManifestV3` is ~344KB on the stack because `DynamicArrayC` uses fixed inline storage all the way down (`processingUnits[4]` → `modules[32]` → inner arrays). Switching the large inner containers to heap-allocated `DynamicArray` would make the manifest a small value type safe to create anywhere. Needs spec — touches serialization, validator, loader, editor. |

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
| RigidBody2DStage | DiaRigidBody2D | Circle drop + settle detection, collision response | `rigid_body.circle_settled`, `rigid_body.collision_detected` | `/spec-feature` |
| StateMachineStage | DiaStateMachine | State transitions, guard evaluation, event firing | `state_machine.reached_target`, `state_machine.guard_blocked` | `/spec-feature` |
| Animation2DStage | DiaAnimation2D + DiaRig2D | Clip playback, pose validation, blend weights | `animation.clip_complete`, `animation.pose_matches` | `/spec-feature` |
| SoftBody2DStage | DiaSoftBody2D | Rope/cloth stabilization, spring convergence | `soft_body.rope_settled` | `/spec-feature` |
| TestGeometry2DStage | DiaGeometry2D + DiaGeometry2DVisualDebugger | Gallery of all shape primitives, 6 intersection pair colour-coding, 4 spatial structure overlays, 5 IVisualDebugger drawers | `geometry2d.passed` | [Spec Approved](specs/features/cluichetest/teststages/geometry2d-stage.md). [Plan ready](specs/features/cluichetest/teststages/geometry2d-stage.plan.md) — 9 tasks. Blocked on scaffold simplification (Task 2). [Mockup](specs/features/cluichetest/teststages/geometry2d-stage.mockup.html) approved. |
| EntityTestStage | DiaEntity | Spawn/destroy/hierarchy/query/mailbox/lifecycle; `TransformComponent` + `VisualTestRenderComponent`; 6 checkpoints | `entity.spawn_complete`, `entity.query_correct`, `entity.hierarchy_valid`, `entity.destroy_cascade`, `entity.mailbox_received`, `entity.lifecycle_complete` | [Spec Approved](specs/features/cluichetest/teststages/entity-test-stage.md). [Plan ready](specs/features/cluichetest/teststages/entity-test-stage.plan.md) — 9 tasks (T-00 done). Uses `/new-cluichetest-stage` skill. Two-module split: EntityModule (reusable) + EntityTestModule (MainPU, checkpoints). |
| UIUltralightStage | DiaUIUltralight | Page load, JS↔C++ bridge (4 bound methods inc. round-trip), pixel buffer non-empty, mouse injection, deterministic reload; Alpine.js panel | `ui.page_loaded`, `ui.js_to_cpp_callback_fired`, `ui.round_trip_value_correct`, `ui.pixel_buffer_non_empty`, `ui.mouse_click_handled`, `ui.deterministic_reload` | [Spec Approved](specs/features/cluichetest/teststages/ui-ultralight-stage.md). Mockup done. 10-task plan at implementation start. |

**First stage to build:** RigidBody2DStage — physics is already stable, no other system dependencies, clear pass/fail checkpoint (body settles). Proves the test-stage pattern works.

### Deferred

| # | Item | Notes |
|---|------|-------|
| 6b | Extract AutomationModuleBase into DiaAutomation | Evaluate from working code after 2+ apps use it |
| 7 | CluicheEditor EditorAutomationModule | After 6b |

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

Spec Approved: [diaarchitecture.md](specs/systems/dia/diaarchitecture.md). All 4 features Approved — ready to implement.

**Target architecture:** 6-sub-layer Core + 4 domain vertical slices. Each domain owns its core modules and its visual debuggers/editor plugins. CMake `target_link_libraries` enforces the dependency rules currently only documented in YAML.

### Implementation sequence (build in order — each depends on previous)

| Step | Item | What's needed | Notes |
|------|------|--------------|-------|
| C7 | YAML layer formalisation | Add `layer:` field to all 55+ module docs | Documents the architecture; prerequisite for C1 |
| C1 | `dia check --tool=arch` | Python `#include` graph checker vs YAML `dependencies.forbidden` | Surfaces current silent violations before any CMake work |
| C2 | Foundation CMake pilot | `CMakeLists.txt` for Foundation sub-layer (DiaCore, DiaMaths, DiaGeometry2D/3D, DiaSerializer, DiaObservation) | `.vcxproj` stays; CMake additive; unlocks `compile_commands.json` |
| C3 | Layered CMake INTERFACE model | Full enforcement — all 55 modules, `cmake --build` replaces msbuild in DiaCLI, PD-006 updated | Requires C1 violations fixed first |

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| DiaAssetRuntime — Hot reload path | Asset Lifecycle Management adds `Failed → Loading` retry but no `Loaded → Loading → Loaded` path. Still need a `ReloadAsset(assetId)` for live iteration (future feature on top of lifecycle management). |
| `Dia::Core::Blackboard` — general-purpose key-value store | Identified during DiaStateMachine research; useful for AI, animation, gameplay. Needs `/spec-feature` under DiaCore. |
| DiaSoftBody2D serializers | Body definition types not yet covered — `DiaReflect` ships Phase 4g (`DiaRigidBody2DSerializers.h`) but Phase 4 plan explicitly excludes DiaSoftBody2D. Needs `/spec-feature` or addition to type-coverage plan when SoftBody work resumes. |
| ~~Cross-PU data flow for debug UI~~ | **Folded into service-channel** — Shapes A+B+C all resolved in one feature (composite per PU-pair + ServiceStream). No follow-on features needed. |
| RigidBody2DStage visual debug — circles not visible | Stage runs and times out but falling circles are not rendering on screen. Ground (huge circle) draws correctly. Coordinate system (Y-UP renderer, pixel-scale physics) and gravity direction are set but circles still don't appear. Possible issues: circles too small relative to viewport, draw order, or drawer not picking up dynamic bodies. Needs investigation with logging or breakpoints. |
