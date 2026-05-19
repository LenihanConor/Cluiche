# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

---

## In Progress

| Item | Spec | What's next |
|------|------|-------------|
| DiaApplicationFlow — Stream Tap & Debug Iteration | [stream-tap.md](specs/features/dia/diaapplicationflow/stream-tap.md) | 7 of 8 live features Done (validated 2026-05-18). One remaining: Stream Tap DebugServer migration. Plan: [stream-tap.plan.md](specs/features/dia/diaapplicationflow/stream-tap.plan.md). Next: Task 1 (SerializeToJson) + Task 2 (StreamInfo extension) + Task 3 (FindStream on inspectable) in parallel. |
| CluicheTest Application Flow | [applicationflow.md](specs/systems/cluichetest/applicationflow.md) | All 5 feature specs Approved. Unblocks once DiaApplicationFlow Stream Tap lands (system will be Done). |

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| System | Spec | Features | Depends On |
|--------|------|----------|------------|
| DiaApplicationFlowEditor | [diaapplicationfloweditor.md](specs/systems/dia/diaapplicationfloweditor.md) | 15 features (Draft) — blocked on DiaApplicationFlow implementation | DiaEditor ✅, DiaWebSocket ✅, DiaApplicationFlow (in progress) |
| DiaGraphics3D | [diagraphics3d.md](specs/systems/dia/diagraphics3d.md) ✅ | `graphics-3d-types` — Camera3D, lights, Mesh3DDrawCommand, Mesh3DFrameData, FrameData3D; new `Dia/DiaGraphics3D/` module; `Dia::Graphics3D::` namespace. Needs system specs for DiaMesh3D/Rig3D/Animation3D/Skinning3D/Scene3D before Phase 2 implements. | DiaMaths (Matrix44), DiaGeometry3D, DiaGraphics ✅ |
| DiaBgfx3D | [diabgfx3d.md](specs/systems/dia/diabgfx3d.md) ✅ | `3d-renderers` — Canvas3D, MeshRenderer, SkinnedMeshRenderer, ShadowRenderer, MaterialRegistry, MeshGpuCache, 6 shaders; `Dia::Bgfx3D::` namespace; Phase 2 ship gate. Blocked on Phase 1 + DiaScene3D chain. | DiaBgfx (Phase 1), DiaScene3D chain, DiaGraphics3D ✅ |
| DiaMesh3D | TBD — needs `/spec-system` | `mesh-asset-and-loader` feature already Approved (parent currently `render-backend`); needs own system spec. glTF 2.0 static+skinned mesh loading, `Mesh3DAsset`, `IAssetTypeHandler` plug-in. | DiaMaths, DiaGeometry3D, DiaAssetRuntime |
| DiaRig3D | TBD — needs `/spec-system` | `skeleton-and-pose` feature already Approved; needs own system spec. Bone3D, Skeleton3D, Pose3D, FK, `SkeletonComponent3D`. Mirrors DiaRig2D. | DiaMesh3D |
| DiaAnimation3D | TBD — needs `/spec-system` | `clip-and-player` feature already Approved; needs own system spec. AnimationClip3D, ClipPlayer3D, glTF loader, STEP/LINEAR/CUBICSPLINE, `AnimationComponent3D`. | DiaRig3D |
| DiaSkinning3D | TBD — needs `/spec-system` | `skinning-palette` feature already Approved; needs own system spec. SkinningManager, per-frame Matrix34 palettes, `skinningPaletteIndex` on draw commands. | DiaAnimation3D, DiaGraphics3D |
| DiaScene3D | TBD — needs `/spec-system` | `scene-graph` feature already Approved; needs own system spec. Flat-list scene, Transform3D parent chains, frustum culling, `Submit(scene, frameData3D)`. | DiaSkinning3D, DiaGraphics3D, DiaGeometry3D |

---

### Standalone Features (system Done, feature Approved)

| Feature | Spec | System |
|---------|------|--------|
| per-app-bin-layout | [per-app-bin-layout.md](specs/features/dia/diapipeline/per-app-bin-layout.md) | DiaPipeline ✅ |
| Harness Core | [harness-core.md](specs/features/dia/diatestharness/harness-core.md) | DiaTestHarness |
| Smoke Test Scenario | [smoke-test-scenario.md](specs/features/cluichetest/cluichetestscenarios/smoke-test-scenario.md) | CluicheTestScenarios (depends on Harness Core) |
| HandlePool\<T\> | [handle-pool.md](specs/features/dia/diacore/handle-pool.md) | DiaCore ✅ — foundation for DiaMailbox + DiaEntity |

---

### DiaObservation (7 features Approved — implement in order)

DiaObservation features are serial: #1 → #2 → #3 → #4/#6 (parallel) → #5 (DiaMetrics wiring) → #7 (last).

| Feature | Spec | Notes |
|---------|------|-------|
| DiaObservation #2 — Foundation | [foundation.md](specs/features/dia/diaobservation/foundation.md) | #1 Done (2026-05-18); `SessionManager`, session directory, `ObservationFileSink`, retention ring, crash dump, `session.json` + `log.jsonl` schemas frozen v1.0 |
| DiaObservation #3 — Config | [config.md](specs/features/dia/diaobservation/config.md) | Blocked on #2; `ObservationConfigLoader`, `.diagame` block, per-channel log levels, all sinks configurable |
| DiaObservation #4 — DiaTrace Spans | [trace-spans.md](specs/features/dia/diaobservation/trace-spans.md) | Blocked on #2; `DIA_TRACE_ZONE` macros, `Tracer` singleton + drain thread, `trace.jsonl` |
| DiaObservation #5 — DiaMetrics wiring | [metrics-registry.md](specs/features/dia/diaobservation/metrics-registry.md) | Blocked on #2; `MetricsFileSink` in DiaObservation, `SessionManager::Tick` snapshot timer. `MetricsCollectorModule` rewrite **already done** (2026-05-18). Only Tasks 11–13 remain. |
| DiaObservation #6 — DiaHealth Reporting | [health-reporting.md](specs/features/dia/diaobservation/health-reporting.md) | Blocked on #2; `HealthRegistry`, `IHealthReporter`/`HealthReporterBase`, `health.json`, `DIA_OBSERVATION_ASSERT`/`FAIL` macros |
| DiaObservation #7 — DebugServer Bridge | [debugserver-bridge.md](specs/features/dia/diaobservation/debugserver-bridge.md) | Blocked on #2/#4/#5/#6; `ObservationBridge` in `DiaDebugServer`, 4 sink interfaces, `observation.*` WebSocket topics, deletes `BroadcastCoreMetrics` |

---

### Entity System Stack (build in dependency order)

System specs and the foundation feature are all `Approved`. Each system's child feature specs are still `Planned/TBD` and need `/spec-feature` before implementation. Strict order: HandlePool → remove old IComponent → DiaMailbox → DiaEntity. Research: `docs/research/entity_system/summary.md`.

| # | Item | Spec | What's next |
|---|------|------|-------------|
| 1 | HandlePool\<T\> implementation | [handle-pool.md](specs/features/dia/diacore/handle-pool.md) | Approved feature — implement (plan + tasks). Foundation for everything below. |
| 2 | Remove old IComponent infrastructure | TBD | Needs `/spec-feature` (likely under DiaCore or DiaEntity). Per SD-ENT-020: delete `Dia/DiaCore/Architecture/Components/`, migrate the two consumers (`SkeletonComponent`, `StateMachineComponent`), remove their tests. Must land before DiaEntity is built. |
| 3 | DiaMailbox features (5 features) | [diamailbox.md](specs/systems/dia/diamailbox.md) | System Approved. Need `/spec-feature` for: address-and-types, typed-queue, subscriptions, routers, module-and-build. |
| 4 | DiaEntity features (11 features) | [diaentity.md](specs/systems/dia/diaentity.md) | System Approved. Need `/spec-feature` in implementation order: foundation, reflection, blueprint-loader, component-deps-and-refs, hierarchy, mailbox-router, query-system, editor-inspection, update-loop, module-and-build (#2 remove-old-icomponent is item #2 above). |
| 5 | EntityModule adapter (CluicheTest) | TBD | Needs `/spec-feature` under CluicheTest — application-level adapter that owns a Realm and plugs into DiaApplicationFlow v2 stage lifecycle (DoStart loads blueprints, returns kLoading until assets resolve, kReady; stage transition destroys Realm). Lives in CluicheTest, not in DiaEntity. |
| 6 | PD-003 / AD-005 Supersede amendment | TBD | After DiaEntity ships, amend platform decision PD-003 and app decision AD-005 (both reference the old IComponent model) to Superseded, pointing to DiaEntity as the new authority. Per SD-ENT-021. Housekeeping. |

---

## Ready to Build (cont.)

### DiaEditor Prerequisites (must land before ApplicationFlowEditor feature specs)

| Feature | Spec | Notes |
|---------|------|-------|
| Project Context Bar | [project-context-bar.md](specs/features/dia/diaeditor/project-context-bar.md) | **Approved** — implement before any ApplicationFlowEditor feature specs. Adds `IEditorContext::LoadProject`, `OnProjectChanged`, toolbar project button, `--project` CLI arg, live auto-load on connect. Migrates all 4 existing plugins (`DiaApplicationFlowEditor`, `DiaApplicationEditor`, `DiaAssetCatalogueEditor`, `DiaAssetRuntimeEditor`) to shared project context. Side tasks: add `diagame_path` to DiaDebugServer `get_app_state`; add `config.assetCatalogue` to `DiaGameConfig` + serializer. |

### DiaApplicationFlowEditor mockup

| Item | Notes |
|------|-------|
| Mockup v4 | `docs/research/diapp_simplif/editor_mockup_v4.html` — Approved design. Key decisions: full-tab Module-Presence grid (virtual scroll, PU group collapse, filter), traffic light dots everywhere (single `.tl` CSS primitive), Live button 3-state (grey/amber/green), stream click navigates to Streams tab (no dual surface), Add PU ghost node on graph, Dependency Order collapsed by default, Validate button removed. Offline vs Live presence modes. Before writing feature specs: update system spec decisions to capture mockup v4 choices as binding (ED-00x). |

---

## Spec Work Needed (Draft or unset — review/approve before building)

| Item | Spec | What's needed |
|------|------|---------------|
| Asset Lifecycle Management | [asset-lifecycle-management.md](specs/features/dia/diaassetruntime/asset-lifecycle-management.md) | Draft — needs `/spec-review` then Approval. Extends state machine with `IAssetTypeHandler` dispatch, `Loading`/`Failed` states, progress query. |
| DiaAPI quit command | TBD | Needed for DiaTestHarness graceful shutdown. No quit command exists today (exit is UI-driven). Needs `/spec-feature` under DiaAPI |
| CluicheTest TestStages system | TBD | Needs `/spec-system` under CluicheTest — multi-stage test stages for deep engine validation (DiaRigidBody2D first). Open questions: phase vs level vs own PU; reporting mechanism. Research: `docs/research/e2e_testing/summary.md` |
| DiaStateMachineEditor system | TBD | Needs `/spec-system` — editor plugin for state machine visual debugging + design-time editing. Depends on DiaStateMachine ✅, DiaEditor |
| Future DiaE2E system | TBD | Needs `/spec-system` AFTER DiaObservation features #1–#6 are Done. The actual reason DiaObservation exists. Sibling `Cluiche/out/<App>/suites/<id>/` directory, `suite.json` + per-scenario summary + JUnit XML emitter, `dia e2e --suite=<name>` CLI command, scenario subprocess spawning. Estimated M because DiaObservation did the schema work; orchestrator is ~200 lines of file reading. Research: `docs/research/observ_telemetry/summary.md` "Future: Multi-Scenario E2E Suites". |

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| DiaAssetRuntime — Hot reload path | Asset Lifecycle Management adds `Failed → Loading` retry but no `Loaded → Loading → Loaded` path. Still need a `ReloadAsset(assetId)` for live iteration (future feature on top of lifecycle management). |
| DiaApplicationFlow — Feature 6: Compile-Time Dependency Validation | Deferred by user ("let's come back and talk about 6") |
| DiaApplicationFlow — v1 source removal | v1 types (ApplicationPhase, ApplicationProcessingUnit, ApplicationModule, MessageBus, MetricsCollector, ApplicationManifest v1, HotReloadManager, ManifestComposer v1, ManifestValidator v1, JsonApplicationManifestSerializer v1, Introspection/ApplicationIntrospector v1, Loader/ApplicationLoader v1, TypeRegistry/ApplicationTypeRegistry v1) still live in `Dia/DiaApplicationFlow/`. **Progress:** v1 unit tests deleted (f0fbee4), dead consumers removed (f11e8cc), DiaDebugServer ported and decoupled from ApplicationFlow entirely (5653c01), 4 v1 integration tests deleted. **Remaining consumers (editor-side, feature-sized work):** `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` (~1700 lines — loads, edits, composes, saves v1 manifests for the React-based manifest editor UI), `Dia/DiaApplicationEditor/ManifestSerializer.{h,cpp}` (v1 manifest → JSON for the UI), `Dia/DiaApplicationEditor/ManifestEditorData.h` (owns a v1 `ApplicationManifest`), `Dia/DiaGame/GameFileComposer.{h,cpp}` (composes `.diagame` files via v1 `ApplicationManifest` + v1 `ManifestComposer`). Porting requires design decisions: (a) what shape the UI JSON takes for v2 stages/streams vs v1 phases/transitions, (b) whether `GameFileComposer` becomes `ComposeFromGameFileV2` returning `ApplicationManifestV2` or the whole composition path moves to `ManifestComposerV2`. Not a cleanup — treat as its own feature with a spec. |
| HotReloadManager — `CollectDependentModules()` / `UpdateDependencyReferences()` | Placeholder stubs; needs real implementation |
| `Dia::Core::Blackboard` — general-purpose key-value store | Identified during DiaStateMachine research; useful for AI, animation, gameplay. Needs `/spec-feature` under DiaCore. |
| DiaAssetRuntime / DiaDebugServer / DiaInput / DiaThreading — metrics | Consolidated into DiaObservation Feature #11 (Domain-Level Metric Registration). See [diaobservation.md](specs/systems/dia/diaobservation.md). DiaThreading extraction still needs its own `/spec-system` but its metrics land in Feature #11 once unblocked. |
| Phase 3d — Physics body serialization | DiaRigidBody2D / DiaSoftBody2D body definitions — DiaAssetCatalogue ✅ now unblocked |
| DiaDebugServer ServerStats subsumption | `Dia/DiaDebugServer/DebugServer.h` `ServerStats` struct + `BroadcastCoreMetrics` method are hand-rolled. DiaObservation #5 (DiaMetrics wiring) replaces `ServerStats` fields with registered gauges; DiaObservation #7 (DebugServer Bridge) replaces `BroadcastCoreMetrics` with the registry-driven `ObservationBridge`. Flagged here for visibility. |
