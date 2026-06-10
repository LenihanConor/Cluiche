# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| System | Spec | Features | Depends On |
|--------|------|----------|------------|
| ~~DiaGraphics3D~~ | [diagraphics3d.md](specs/systems/dia/diagraphics3d.md) ✅ | **Done** — Camera3D, lights, Mesh3DDrawCommand, Mesh3DFrameData, FrameData3D; `Dia/DiaGraphics3D/` module; `Dia::Graphics3D::` namespace. 19 tests pass. | — |
| DiaBgfx3D | [diabgfx3d.md](specs/systems/dia/diabgfx3d.md) ✅ | `gpu-resources` — MaterialRegistry + MeshGpuCache (needs DiaMesh3D). `3d-renderers` — MeshRenderer, SkinnedMeshRenderer, ShadowRenderer, 6 shaders (needs DiaMesh3D + DiaSkinning3D). `canvas3d` — Canvas3D shell + MaterialRegistry **done** (tasks 1–3); CluicheTest demo blocked on DiaScene3D chain. | DiaBgfx (Phase 1) ✅, DiaGraphics3D ✅, DiaMesh3D, DiaScene3D chain |
| DiaMesh3D | [diamesh3d.md](specs/systems/dia/diamesh3d.md) ✅ | `mesh-asset-and-loader` — `Vertex3D` (52 bytes, no joint data), `Submesh`, `Mesh3DAsset`, `Mesh3DAssetHandler` (cooked `.mesh3d` flat-binary loader, type prefix `"mesh3d."`). glTF import is DiaAssetPipeline build-time only. | DiaMaths, DiaGeometry3D, DiaAssetRuntime |
| DiaRig3D | TBD — needs `/spec-system` | `skeleton-and-pose` feature already Approved; needs own system spec. Bone3D, Skeleton3D, Pose3D, FK, `SkeletonComponent3D`. Also owns `Rig3DAsset` (skeleton + per-vertex skin binding — joint indices + weights as parallel array to mesh vertices). Mirrors DiaRig2D. | DiaMesh3D |
| DiaAnimation3D | TBD — needs `/spec-system` | `clip-and-player` feature already Approved; needs own system spec. AnimationClip3D, ClipPlayer3D, STEP/LINEAR/CUBICSPLINE interpolation, `AnimationComponent3D`. glTF animation import is DiaAssetPipeline build-time only (parallel to DiaMesh3D's cooked binary approach). | DiaRig3D |
| DiaSkinning3D | TBD — needs `/spec-system` | `skinning-palette` feature already Approved; needs own system spec. SkinningManager, per-frame Matrix34 palettes, `skinningPaletteIndex` on draw commands. | DiaAnimation3D, DiaGraphics3D |
| ~~DiaCamera3D~~ | [diacamera3d.md](specs/systems/dia/diacamera3d.md) ✅ | `camera3d-types` — Camera3D (position + quaternion + perspective/ortho); `camera3d-registry` — CameraRegistry3D, ICameraBehaviour3D, factory; `camera3d-behaviours` — Follow3D, SmoothDamp3D, BoundsClamp3D, ScreenShake3D, Orbit, Flythrough. | DiaMaths, DiaGeometry3D ✅ |
| ~~DiaLighting3D~~ | [dialighting3d.md](specs/systems/dia/dialighting3d.md) ✅ | `lighting3d-types` — PointLight3D, DirectionalLight3D, SpotLight3D, AmbientLight3D, LightRegistry3D, ILightBehaviour3D, factory; `lighting3d-behaviours` — Flicker, Pulse, ColorCycle. | DiaMaths, DiaCore ✅ |
| ~~DiaScene3D~~ | [diascene3d.md](specs/systems/dia/diascene3d.md) ✅ | `scene3d-format` — Scene3D struct, SceneNode3D, SceneGraph3D; `scene3d-loader` — SceneLoader3D (camera/light/entity hydration, scene graph build); `scene3d-submit` — frustum derivation, transform resolution, culling, draw command emission. | DiaCamera3D, DiaLighting3D, DiaGraphics3D ✅, DiaGeometry3D ✅, diaentitytemplate ✅ |
| ~~DiaSceneEditor~~ | [diasceneeditor.md](specs/systems/dia/diasceneeditor.md) ✅ | **Done** — scene-hierarchy-panel, entity-placement-crud, change-blueprint, layer-authoring, camera-light-authoring, scene-validation. 22 tasks. | — |
| ~~DiaEntityInspector~~ | [diaentityinspector.md](specs/systems/dia/diaentityinspector.md) ✅ | **Done** — entity-inspector-panel, query-browser-tab, mailbox-traffic-monitor, entity-watch-list. 19 tasks. | — |
| ~~DiaEditorAPI~~ | [diaeditorapi.md](specs/systems/cluicheeditor/diaeditorapi.md) ✅ | Phase 1 done — action registry, Python `dia_editor` module, incremental regen on dynamic plugin load, JSON command capacity 256. Phase 2: MCP adapter deferred. **Migrations done:** `asset-catalogue-migration` ✅, `entity-template-migration` ✅, `scene-editor-scriptable` ✅, `plugin-browser-migration` ✅, `app-editor-actions` ✅. **Remaining:** `app-flow-editor-migration` ✅ Done. Inspector migrations dropped — observation-only surfaces. `pipeline-migration` (Deferred). All meaningful migrations complete. | DiaAPI, DiaEditor, DiaPython, DiaWebSocket (Ph2) |
| DiaChatPlugin | [diachatplugin.md](specs/systems/cluicheeditor/diachatplugin.md) ✅ | Dockable AI assistant panel — Ollama/Claude/Gemini via DiaPython orchestrator, direct `ExecuteAction()` tool dispatch, auto-gen knowledge context (`editor_actions.md`, `data_types.md`) + hand-authored files (`engine_overview.md`, `editor_workflows.md`, `asset_style_guide.md`), hybrid chat+detail UI, destructive action confirmation gate, context window indicator, empty state. Phase 2: multi-step agentic loop. Requires `project.list` action + DiaEditorAPI data-type-registry feature. | DiaEditorAPI Phase 1 + data-type-registry, DiaEditor, DiaPython, DiaUICEF |
| ~~GoogleTestSpeed~~ | [googletestspeed.md](specs/systems/googletests/googletestspeed.md) ✅ | **Done** — SLOW_* tagging + default filter, Release CI config, gtest.h PCH, --shards N parallel runner + XML merger, DiaPython fixture amortisation. | — |
| ~~DiaApplicationFlowInspector~~ | [diaapplicationflowinspector.md](specs/systems/dia/diaapplicationflowinspector.md) ✅ | **Done** — 4-tab Inspector (Modules/Streams/Timing/Log); framework telemetry; Editor split/renamed to DiaApplicationFlowEditor. 49 tasks complete (2026-06-07). | — |
| ~~DiaScene2D~~ | [diascene2d.md](specs/systems/dia/diascene2d.md) ✅ | **Done** — Scene2D struct, LayerTable, SceneLoader2D (camera/light/entity hydration, instanceData patching, validation). 18 tests pass. | — |
| ~~DiaArchitecture~~ | [diaarchitecture.md](specs/systems/dia/diaarchitecture.md) ✅ | **Done** — Layer fields, refactoring (R1–R6), audit tool (`dia check arch`), SLN sync (`dia check sln-sync`). 1975 violations baselined. | — |

---

### Standalone Features (system Done, feature Approved)

| Feature | Spec | System | Notes |
|---------|------|--------|-------|
| ~~per-app-bin-layout~~ | [per-app-bin-layout.md](specs/features/dia/diapipeline/per-app-bin-layout.md) | DiaPipeline ✅ | **Done** — `Directory.Build.props` per-app OutDir, path_resolver.py, package_stage.py, clean CluicheEditor layout. |
| ~~Editor Memory~~ | [editor-memory.md](specs/features/dia/diaeditor/editor-memory.md) | DiaEditor ✅ | **Done** — save/restore layout, plugins, per-plugin project-scoped state. |
| ~~Toast Notifications~~ | [toast-notifications.md](specs/features/dia/diaeditor/toast-notifications.md) | DiaEditor ✅ | **Done** — framework-level notification service; plugins push toasts, shell renders. |
| ~~Python Console~~ | [python-console.md](specs/features/dia/diaeditor/python-console.md) | DiaEditor ✅ | **Done** — Dockable REPL plugin; `python_console.execute` + `python_console.run_file` handlers; colour-coded React UI; dual-registered as DiaEditorAPI actions. |
| ~~app-flow-editor-migration~~ | [app-flow-editor-migration.md](specs/features/cluicheeditor/diaeditorapi/app-flow-editor-migration.md) | DiaEditorAPI ✅ | **Done** — 10 actions dual-registered: manifest load/save/applyCommand/getState, history undo/redo/getState, validation.run, types.get, risk.check. All callable from Python via `dia_editor.*`. |

---

## Ready to Build (cont.)

---

## In Progress

_Nothing here._

---

## Spec Work Needed (Draft or unset — review/approve before building)

| Item | Spec | What's needed |
|------|------|---------------|
| DiaStateMachineEditor system | TBD | Needs `/spec-system` — editor plugin for state machine visual debugging + design-time editing. Depends on DiaStateMachine ✅, DiaEditor |


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
| 9 | ~~DiaScreenCapture — frame grab for mock comparison~~ | **Done** — split into two features: [frame-capture-readback](specs/features/dia/diabgfx/frame-capture-readback.md) (ICanvas async readback, DiaBgfx ring buffer) + [captures](specs/features/dia/diaobservation/captures.md) (6th observation pillar, PNG write to session). Both Done. |
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

## DiaArchitecture — Done ✅

Spec: [diaarchitecture.md](specs/systems/dia/diaarchitecture.md). Plan: [diaarchitecture.plan.md](specs/systems/dia/diaarchitecture.plan.md). All phases complete (2026-06-04).

Numbered layers (1.0–3.1), 6 refactors (R1–R6), `dia check arch` audit tool, `dia check sln-sync`, 64/64 vcxprojs documented with `layer:` field. CI gate live: `dia check arch` runs in `dia pipeline --stage static-analysis`.

---

## Debug Infrastructure Improvements

Lessons from the entity-inspector debugging session (2026-06-06). Three bugs compounded silently — no single system reported failure. These items would have surfaced the problem in minutes instead of hours.

| Item | Priority | Notes |
|------|----------|-------|
| **WebSocket connection health metrics** | High | Editor should track messages received/dropped/dispatched per topic and surface in status bar. The 4000+ dropped messages were invisible until we added logging manually. A `ws.queue.dropped` counter + UI badge would have made this obvious. |
| ~~**ObservationBridge subscription gating**~~ | ~~High~~ | ~~Done (2026-06-07)~~ — bridge already gates on `mSubscriberQuery` per topic; `mClientTaps` populated by `HandleSubscribe`, cleaned up on disconnect. |
| **DebugServer per-topic send stats** | Medium | Track bytes/messages sent per topic per connection. When `entity.inspect: 0 delivered` but `observation.metric: 4000 sent` is visible in one glance, the flood diagnosis is instant. Surface via `get_app_state` query. |
| **WebSocket queue overflow → ERROR level + UI toast** | Medium | `"Incoming queue full"` was a WARNING buried in logs. Should be ERROR level, increment a metric, and fire a toast in the editor shell: "Connection degraded — messages being dropped." Backpressure signal to server (throttle request) is a stretch goal. |
| **Subscribe handshake verification** | Low | After sending `MESSAGE_TYPE_SUBSCRIBE`, editor should expect an ACK within N seconds. If no ACK and no data arrives, log ERROR. Would have caught the "subscribe never sent" bug immediately. |
| **End-to-end data flow smoke test** | Low | `dia orchestrate` scenario that launches game + editor, connects, enters EntityTestStage, and asserts `entity_inspector.inspect_data` arrives at the UI bridge within 5 seconds. Regression gate for future transport changes. |

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


| ~~Shared asset creation~~ | **Done** — `asset_catalogue.create_asset` unified handler; stage dropdown grey styling; stage↔scene association write-back; Blueprint Editor "+New Template" button. Tasks 1–9 complete. Task 10 (`create_from_template` removal) deferred pending test migration. |
| ~~Spatial cell inspector~~ | **Done** — `SpatialGridDrawer` + `HexGridDrawer` have selection state, highlight draw, and ImGui inspector. `Geometry2DTestStageModule` owns selection + wires click detection via `SetSelection()`. All ACs met. |
| ~~Arc/Sector cleanup~~ | **Done** (2026-05-31) — `Arc` deleted (duplicate sector-shaped class); `Sector` kept as the single canonical type. `SectorDrawHelper` rendering fix (duplicate close-back vertex removed). 249 geometry tests pass. |
