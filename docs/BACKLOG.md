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
| DiaSceneEditor | [diasceneeditor.md](specs/systems/dia/diasceneeditor.md) ✅ | 6 features: scene-hierarchy-panel, entity-placement-crud, change-blueprint, layer-authoring, camera-light-authoring, scene-validation. Spatial authoring of `.diascene` files. [Plan](specs/systems/dia/diasceneeditor.plan.md) (22 tasks). | DiaScene2D, DiaEditor, DiaReflect, DiaGame, DiaAssetCatalogue, DiaBlueprintEditor (soft) |
| DiaEntityInspector | [diaentityinspector.md](specs/systems/dia/diaentityinspector.md) ✅ | 4 features: entity-inspector-panel, query-browser-tab, mailbox-traffic-monitor, entity-watch-list. Runtime debug via WebSocket. [Plan](specs/systems/dia/diaentityinspector.plan.md) (18 tasks). | DiaEntity, DiaDebugProtocol, DiaDebugServer, DiaEditor, DiaReflect Phase 3 |
| ~~DiaScene2D~~ | [diascene2d.md](specs/systems/dia/diascene2d.md) ✅ | **Done** — Scene2D struct, LayerTable, SceneLoader2D (camera/light/entity hydration, instanceData patching, validation). 18 tests pass. | — |
| DiaArchitecture | [diaarchitecture.md](specs/systems/dia/diaarchitecture.md) ✅ | C7 → C1 → C2 → C3a (Dia modules, additive) → C3b (Cluiche apps + retirement, deferrable). C3a spec needs update to Draft→Approved; C3b is a new Draft spec. | None |

---

### Standalone Features (system Done, feature Approved)

| Feature | Spec | System | Notes |
|---------|------|--------|-------|
| per-app-bin-layout | [per-app-bin-layout.md](specs/features/dia/diapipeline/per-app-bin-layout.md) | DiaPipeline ✅ | |
| Editor Memory | [editor-memory.md](specs/features/dia/diaeditor/editor-memory.md) | DiaEditor ✅ | Invisible save/restore of layout, plugins, per-plugin project-scoped state. Size S. |
| Toast Notifications | [toast-notifications.md](specs/features/dia/diaeditor/toast-notifications.md) | DiaEditor ✅ | Framework-level notification service — plugins push toasts, shell renders uniformly. [Plan](specs/features/dia/diaeditor/toast-notifications.plan.md) (11 tasks). Size S. |

---

## Ready to Build (cont.)

---

## In Progress

| System | Spec | What's happening |
|--------|------|-----------------|
| DiaApplicationFlowInspector | [diaapplicationflowinspector.md](specs/systems/dia/diaapplicationflowinspector.md) | Spec **Approved** (2026-06-04). Split from Editor — live runtime inspection (timeline, backpressure, frame budget, event log). [Plan](specs/systems/dia/diaapplicationflowinspector.plan.md) (34 tasks). Also renames DiaApplicationEditor → DiaApplicationFlowEditor. |

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

### TestStages — All Done ✅

System spec: [teststages.md](specs/systems/cluichetest/teststages.md) (Approved). Pattern: one manifest stage per engine feature, one Module per stage, checkpoints as validation contract. Star topology: Boot → Stage → Boot. All 8 stages completed as of 2026-06-01.

**Scaffolding:** `dia scaffold stage <Name> --modules <...>` creates all 9 touch points in one command. `/new-cluichetest-stage` skill infers domain, runs the script, adds domain-specific C++. Domain patterns: Physics, Entity, Asset, Animation, StateMachine, Geometry.

### Deferred

| # | Item | Notes |
|---|------|-------|
| 6b | Extract AutomationModuleBase into DiaAutomation | Evaluate from working code after 2+ apps use it |
| 7 | CluicheEditor EditorAutomationModule | After 6b |
| 9 | ~~DiaScreenCapture — frame grab for mock comparison~~ | **Specced** — split into two features: [frame-capture-readback](specs/features/dia/diabgfx/frame-capture-readback.md) (ICanvas async readback, DiaBgfx ring buffer) + [captures](specs/features/dia/diaobservation/captures.md) (6th observation pillar, PNG write to session). Both Approved with plans. |
| 10 | ~~**Eliminate remaining cross-PU statics**~~ | **Done** (2026-05-30) — all 4 statics removed. Policy: no new statics without sign-off. |
| 10a | ~~`JobSystemModule::GetStatic()`~~ | **Done** (2026-05-30) — replaced with `ModuleRef<JobSystemModule>` in `KernelModule`. Same PU (MainPU). |
| 10b | ~~`AutomationModule::GetStatic()` + `AssetServiceModule::GetStatic()`~~ | **Done** (2026-05-30) — `ServiceStream<AssetLoadStatus>` (Main→Sim), `ServiceStream<AutomationService>` (Main→Sim), `MainToRenderFrame.assetLoadStatus` (Main→Render). `TestStageModuleBase` + `TestAssetRuntimeStageModule` moved to SimPU. |
| 10c | ~~`VisualDebuggerModule::sLayerManager`~~ | **Done** (2026-05-30) — `ServiceStream<DebugLayerManager>` (Sim→Render). `Physics2DModule` moved MainPU→SimPU; uses `ModuleRef`. `Geometry2DTestStageModule` uses `ModuleRef`. `AssetRuntimeVisualDebuggerModule` + `VisualDebuggerConsoleModule` use `ServiceStreamReader`. Data race eliminated. |

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

## DiaArchitecture — Numbered Layer Architecture

Spec Approved: [diaarchitecture.md](specs/systems/dia/diaarchitecture.md). Plan: [diaarchitecture.plan.md](specs/systems/dia/diaarchitecture.plan.md).

**Target architecture:** Numbered layers (1.0–3.1) with strict dependency ordering. Sub-levels within Foundation (Core/Maths/Services/Platform/Application), Assets (core/tools), and Domains (Visual/Physics/Animation — core/tools per domain). CMake dropped — MSBuild remains. Enforcement via C1 audit tool.

### Phase 1 — Layer Fields ✅ Done (2026-06-03)

All 76 module docs updated with canonical `layer:` values (foundation/core, foundation/maths, foundation/services, foundation/platform, foundation/application, assets/core, assets/tools, domain/visual/*, domain/physics/*, domain/animation/*).

### Phase 2 + 3 — Refactoring ✅ Done (2026-06-03)

All 6 refactoring actions shipped:

| # | What | Outcome |
|---|------|---------|
| R1 | DiaFileIO split from DiaCore | FilePath/ compiles into DiaFileIO.lib |
| R2 | DiaJson split from DiaCore | jsoncpp compiles into DiaJson.lib |
| R3 | DiaStreams split from DiaApplicationFlow | Stream types in DiaStreams/, full header relocation |
| R4 | TextureHandler → DiaBgfx | DiaAssetRuntime drops DiaBgfx dep |
| R5 | DiaDebugServer dep inversion | IStreamTapTarget owned by DebugServer; DiaApplicationFlow dep removed |
| R6 | DiaDebugDraw split from DiaVisualDebugger | Abstract debug layer at foundation/services; cross-domain exception eliminated |

### Phase 4 — Architecture Audit Tool (C1) ✅ Done (2026-06-04)

`dia check arch [--module <id>] [--summary]` — reads all module docs, parses `#include` directives, detects forbidden deps and layer ordering violations. First run: 1975 violations found (expected — domain VDs still depend on `DiaVisualDebugger` rather than `DiaDebugDraw`). Output: `Cluiche/out/check/arch-violations.txt`. CI gate (task 18) deferred until violations are cleaned up.

### Phase 5 — SLN Layer Sync ✅ Done (2026-06-04)

`dia check sln-sync [--dry-run]` — rewrites `Cluiche.sln` solution folders to numbered names. 13 new folders created, 76 Dia projects assigned. Idempotent. Post-step wired into `dia scaffold module`. Reference: [diaarchitecture.folders.md](specs/systems/dia/diaarchitecture.folders.md).

Also: module docs created for all 18 previously undocumented Dia projects (DiaAPI, DiaAsset, DiaEditor, DiaGame, DiaImGui, DiaPicking, DiaProtobuf, DiaSDL, etc.) — 64/64 vcxprojs now have `layer:` set.

### Next — Violation Cleanup

1975 arch violations to clear (mostly domain VDs including `DiaVisualDebugger` instead of `DiaDebugDraw`). Once clean, add `dia check arch` to CI (task 18).

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| RenderTechnique asset type | Layer-level rendering policy (blend mode, post-process like bloom/distortion). Layers reference a technique by name; renderer resolves at draw time. Needs `/spec-feature` under DiaGraphics or DiaBgfx once the scene system lands. |
| Camera2D controller (pan/zoom/reset) | Application-side input→Camera2D wiring for CluicheTest stages (keyboard pan, scroll zoom, home-key reset). Unblocked once coord2d-debug-overlay ships Camera2D + renderer integration. |
| DiaAssetRuntime — Hot reload path | Asset Lifecycle Management adds `Failed → Loading` retry but no `Loaded → Loading → Loaded` path. Still need a `ReloadAsset(assetId)` for live iteration (future feature on top of lifecycle management). |
| `Dia::Core::Blackboard` — general-purpose key-value store | Identified during DiaStateMachine research; useful for AI, animation, gameplay. Needs `/spec-feature` under DiaCore. |
| ~~DiaSoftBody2D serializers~~ | **Done** (2026-05-28) — `DiaSoftBody2DSerializers.h` ships `WorldDef`, `RopeDef`, `ClothDef`. Follows `DiaRigidBody2DSerializers.h` pattern exactly. Non-owning anchor/world pointers skipped (same convention as RigidBody2D). |
| ~~Cross-PU data flow for debug UI~~ | **Folded into service-channel** — Shapes A+B+C all resolved in one feature (composite per PU-pair + ServiceStream). No follow-on features needed. |
| RigidBody2DStage test failure | Visual debug rendering fixed (circles now visible). Test itself fails — needs investigation. |
| ~~Spatial cell inspector~~ | **Done** — `SpatialGridDrawer` + `HexGridDrawer` have selection state, highlight draw, and ImGui inspector. `Geometry2DTestStageModule` owns selection + wires click detection via `SetSelection()`. All ACs met. |
| ~~Arc/Sector cleanup~~ | **Done** (2026-05-31) — `Arc` deleted (duplicate sector-shaped class); `Sector` kept as the single canonical type. `SectorDrawHelper` rendering fix (duplicate close-back vertex removed). 249 geometry tests pass. |
