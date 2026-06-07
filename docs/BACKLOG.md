# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| System | Spec | Features | Depends On |
|--------|------|----------|------------|
| ~~DiaGraphics3D~~ | [diagraphics3d.md](specs/systems/dia/diagraphics3d.md) ✅ | **Done** — Camera3D, lights, Mesh3DDrawCommand, Mesh3DFrameData, FrameData3D; `Dia/DiaGraphics3D/` module; `Dia::Graphics3D::` namespace. 19 tests pass. | — |
| DiaBgfx3D | [diabgfx3d.md](specs/systems/dia/diabgfx3d.md) ✅ | `3d-renderers` — Canvas3D, MeshRenderer, SkinnedMeshRenderer, ShadowRenderer, MaterialRegistry, MeshGpuCache, 6 shaders; `Dia::Bgfx3D::` namespace; Phase 2 ship gate. Blocked on Phase 1 + DiaScene3D chain. | DiaBgfx (Phase 1), DiaScene3D chain, DiaGraphics3D ✅ |
| DiaMesh3D | TBD — needs `/spec-system` | `mesh-asset-and-loader` feature already Approved (parent currently `render-backend`); needs own system spec. glTF 2.0 static+skinned mesh loading, `Mesh3DAsset`, `IAssetTypeHandler` plug-in. | DiaMaths, DiaGeometry3D, DiaAssetRuntime |
| DiaRig3D | TBD — needs `/spec-system` | `skeleton-and-pose` feature already Approved; needs own system spec. Bone3D, Skeleton3D, Pose3D, FK, `SkeletonComponent3D`. Mirrors DiaRig2D. | DiaMesh3D |
| DiaAnimation3D | TBD — needs `/spec-system` | `clip-and-player` feature already Approved; needs own system spec. AnimationClip3D, ClipPlayer3D, glTF loader, STEP/LINEAR/CUBICSPLINE, `AnimationComponent3D`. | DiaRig3D |
| DiaSkinning3D | TBD — needs `/spec-system` | `skinning-palette` feature already Approved; needs own system spec. SkinningManager, per-frame Matrix34 palettes, `skinningPaletteIndex` on draw commands. | DiaAnimation3D, DiaGraphics3D |
| DiaCamera3D | [diacamera3d.md](specs/systems/dia/diacamera3d.md) ✅ | `camera3d-types` — Camera3D (position + quaternion + perspective/ortho); `camera3d-registry` — CameraRegistry3D, ICameraBehaviour3D, factory; `camera3d-behaviours` — Follow3D, SmoothDamp3D, BoundsClamp3D, ScreenShake3D, Orbit, Flythrough. | DiaMaths, DiaGeometry3D ✅ |
| DiaLighting3D | [dialighting3d.md](specs/systems/dia/dialighting3d.md) ✅ | `lighting3d-types` — PointLight3D, DirectionalLight3D, SpotLight3D, AmbientLight3D, LightRegistry3D, ILightBehaviour3D, factory; `lighting3d-behaviours` — Flicker, Pulse, ColorCycle. | DiaMaths, DiaCore ✅ |
| DiaScene3D | [diascene3d.md](specs/systems/dia/diascene3d.md) ✅ | `scene3d-format` — Scene3D struct, SceneNode3D, SceneGraph3D; `scene3d-loader` — SceneLoader3D (camera/light/entity hydration, scene graph build); `scene3d-submit` — frustum derivation, transform resolution, culling, draw command emission. | DiaCamera3D, DiaLighting3D, DiaGraphics3D ✅, DiaGeometry3D ✅, diaentitytemplate ✅ |
| ~~DiaSceneEditor~~ | [diasceneeditor.md](specs/systems/dia/diasceneeditor.md) ✅ | **Done** — scene-hierarchy-panel, entity-placement-crud, change-blueprint, layer-authoring, camera-light-authoring, scene-validation. 22 tasks. | — |
| ~~DiaEntityInspector~~ | [diaentityinspector.md](specs/systems/dia/diaentityinspector.md) ✅ | **Done** — entity-inspector-panel, query-browser-tab, mailbox-traffic-monitor, entity-watch-list. 19 tasks. | — |
| DiaEditorAPI | [diaeditorapi.md](specs/systems/cluicheeditor/diaeditorapi.md) ✅ | Phase 1: C++ action registry + auto-generated Python `dia_editor` module (automation testing). Phase 2: MCP adapter for Ollama at-desk AI workflows. Run `/implement` to start. | DiaAPI, DiaEditor, DiaPython, DiaWebSocket (Ph2) |
| DiaChatPlugin | [diachatplugin.md](specs/systems/cluicheeditor/diachatplugin.md) ✅ | Dockable AI assistant panel — Ollama/Claude/Gemini via DiaPython orchestrator, direct `ExecuteAction()` tool dispatch, curated knowledge context system, hybrid chat+detail panel UI. Phase 2: multi-step agentic loop. | DiaEditorAPI Phase 1, DiaEditor, DiaPython, DiaUICEF |
| GoogleTestSpeed | [googletestspeed.md](specs/systems/googletests/googletestspeed.md) ✅ | 5 features in order: slow-suite-tagging → release-config-ci → precompiled-header → shard-runner → fixture-amortisation. Target: ~5 min → <90s. | DiaCLI, MSBuild, DiaPython |
| ~~DiaScene2D~~ | [diascene2d.md](specs/systems/dia/diascene2d.md) ✅ | **Done** — Scene2D struct, LayerTable, SceneLoader2D (camera/light/entity hydration, instanceData patching, validation). 18 tests pass. | — |
| ~~DiaArchitecture~~ | [diaarchitecture.md](specs/systems/dia/diaarchitecture.md) ✅ | **Done** — Layer fields, refactoring (R1–R6), audit tool (`dia check arch`), SLN sync (`dia check sln-sync`). 1975 violations baselined. | — |

---

### Standalone Features (system Done, feature Approved)

| Feature | Spec | System | Notes |
|---------|------|--------|-------|
| ~~per-app-bin-layout~~ | [per-app-bin-layout.md](specs/features/dia/diapipeline/per-app-bin-layout.md) | DiaPipeline ✅ | **Done** — `Directory.Build.props` per-app OutDir, path_resolver.py, package_stage.py, clean CluicheEditor layout. |
| ~~Editor Memory~~ | [editor-memory.md](specs/features/dia/diaeditor/editor-memory.md) | DiaEditor ✅ | **Done** — save/restore layout, plugins, per-plugin project-scoped state. |
| ~~Toast Notifications~~ | [toast-notifications.md](specs/features/dia/diaeditor/toast-notifications.md) | DiaEditor ✅ | **Done** — framework-level notification service; plugins push toasts, shell renders. |

---

## Ready to Build (cont.)

---

## In Progress

| System | Spec | What's happening |
|--------|------|-----------------|
| DiaApplicationFlowInspector | [diaapplicationflowinspector.md](specs/systems/dia/diaapplicationflowinspector.md) | Spec **Approved** (2026-06-04). Split from Editor — live runtime inspection (timeline, backpressure, frame budget, event log). [Plan](specs/systems/dia/diaapplicationflowinspector.plan.md) (49 tasks). Also renames DiaApplicationEditor → DiaApplicationFlowEditor. Includes new Editor-side ACs: connection-status-indicator, live-state-overlay (topic model), risky-change-warnings (no Inspector dep). |

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

## DiaArchitecture — Done ✅

Spec: [diaarchitecture.md](specs/systems/dia/diaarchitecture.md). Plan: [diaarchitecture.plan.md](specs/systems/dia/diaarchitecture.plan.md). All phases complete (2026-06-04).

Numbered layers (1.0–3.1), 6 refactors (R1–R6), `dia check arch` audit tool, `dia check sln-sync`, 64/64 vcxprojs documented with `layer:` field. CI gate live: `dia check arch` runs in `dia pipeline --stage static-analysis`.

---

## Debug Infrastructure Improvements

Lessons from the entity-inspector debugging session (2026-06-06). Three bugs compounded silently — no single system reported failure. These items would have surfaced the problem in minutes instead of hours.

| Item | Priority | Notes |
|------|----------|-------|
| **WebSocket connection health metrics** | High | Editor should track messages received/dropped/dispatched per topic and surface in status bar. The 4000+ dropped messages were invisible until we added logging manually. A `ws.queue.dropped` counter + UI badge would have made this obvious. |
| **ObservationBridge subscription gating** | High | Currently broadcasts observation.metric/log/trace to ALL connected clients unconditionally (disabled as of 2026-06-06). Must respect the subscribe protocol — only send to clients that have sent `MESSAGE_TYPE_SUBSCRIBE` for the relevant topic. Re-enable once gated. |
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
| RigidBody2DStage test failure | Visual debug rendering fixed (circles now visible). Test itself fails — needs investigation. |
| Stale deploy test mock | `test_force_removes_deploy_directory` uses `.run.return_value` but code now calls `.run_with_result()` — mock never intercepts, sentinel file survives. Fix mock setup in `test_cli_asset_commands.py`. |
| ~~Shared asset creation~~ | **Done** — `asset_catalogue.create_asset` unified handler; stage dropdown grey styling; stage↔scene association write-back; Blueprint Editor "+New Template" button. Tasks 1–9 complete. Task 10 (`create_from_template` removal) deferred pending test migration. |
| ~~Spatial cell inspector~~ | **Done** — `SpatialGridDrawer` + `HexGridDrawer` have selection state, highlight draw, and ImGui inspector. `Geometry2DTestStageModule` owns selection + wires click detection via `SetSelection()`. All ACs met. |
| ~~Arc/Sector cleanup~~ | **Done** (2026-05-31) — `Arc` deleted (duplicate sector-shaped class); `Sector` kept as the single canonical type. `SectorDrawHelper` rendering fix (duplicate close-back vertex removed). 249 geometry tests pass. |
