# Cluiche Backlog History

Completed items moved from BACKLOG.md. For active work see [BACKLOG.md](BACKLOG.md).

---

## Completed Systems

| System | Spec | Completed | Notes |
|--------|------|-----------|-------|
| DiaEntity | [diaentity.md](specs/systems/dia/diaentity.md) | 2026-05-24 | All 10 features Done. Layered ECS — `Domain` container, generational 64-bit `Entity` handles via `Handle<EntityTag>`, IComponent with end-of-frame mutation queue (add/remove/destroy 3-pass). `DIA_COMPONENT` + `FIELD` + `DIA_COMPONENT_REGISTER` macro DSL with `ComponentTypeDesc` + `ComponentRegistry`. REQUIRES dependency validation. JsonBlueprintLoader (3-pass: instantiate → patch EntityRefs → validate). `EntityRef<T>` cross-entity refs. Parent/child hierarchy with cycle detection + `QueueDestroySubtree`. `EntityRouter : IMailboxRouter` (4 address kinds). `QueryView<TComponents...>` with signature-keyed cache. `Domain::Update` opt-in via `DIA_UPDATABLE`. `IEntityInspectable` with ReadField/WriteField via read-modify-write. DomainHealth + 4 metrics (entity.count, entity.components, entity.mutations, entity.query_rebuilds). DIA_TRACE_ZONE + DIA_PROFILE_SCOPE across Update/EndOfFrame/ApplyMutations/RebuildQueryCache. PD-003 / AD-005 marked Superseded. 107/107 tests GREEN (94 spec + 7 stress + 6 integration). |
| DiaReflect | [diareflect.md](specs/systems/dia/diareflect.md) | 2026-05-21 | All phases Done. C7 archive pattern + macro-DSL (DIA_SERIALIZE/DIA_FIELD). JSON + binary archives, container specializations (T[N], DynamicArrayC), polymorphic registry, field attributes (Required/Range/AssetRef). DiaMaths + DiaCore Strings/PathStoreConfig serializers added. DiaCore/Type deleted (31 files); BasicTypeDefines.h relocated to DiaCore/Core/. 5065 tests GREEN. |
| DiaMailbox | [diamailbox.md](specs/systems/dia/diamailbox.md) | 2026-05-21 | All 5 features Done: module-and-build, address-and-types, typed-queue, subscriptions, routers. Generic typed deferred messaging; `Address = (StringCRC, uint64_t)`; per-type ring buffers; poll-drain pattern; HandlePool-backed subscriptions; IMailboxRouter interface + MockRouter. DIA_LOG_WARNING + dia.mailbox.{sent,dropped,drained} metrics. 62/62 tests GREEN (49 spec + 13 exhaustive boundary/stress). |
| DiaObservation | [diaobservation.md](specs/systems/dia/diaobservation.md) | 2026-05-20 | All 13 features Done. 5-pillar observability system (logs, traces, metrics, health, profiling). DiaLogger folded in and deleted. Session directory + session.json + log/trace/metric/profile.jsonl schemas frozen at v1.0. OTel wire-compat without SDK. Async drain thread, per-thread rings, retention ring. DiaMetrics standalone merged into DiaObservation/Metric/. DebugServer bridge via WebSocket topics. Domain instrumentation across ProcessingUnit, DiaGraphics, DiaStream, DiaAssetRuntime. IHealthReporter on Module + AssetServiceModule. dia.assets.* + dia.debugserver.* metrics registered. |
| DiaApplicationFlowEditor | [diaapplicationfloweditor.md](specs/systems/dia/diaapplicationfloweditor.md) | 2026-05-20 | All 17 features implemented, all 56 tasks Done including manual E2E gate. Full v2 `.diaapp` manifest editor: C++ data model + command history + validation + live connection; React UI with GraphView, ModulePresenceGrid, StreamsTab, StagesTab, PUInspector, ModuleInspector, StageConfiguration, ValidationBar, LiveConnectionButton, LiveTransitionPanel, RiskyChangeDialog, FileConflictDialog. Manifest v3 schema (per-stage `transitions[]` + `autoAdvance`); 154 React tests + 141 C++ tests. Replaces v1 DiaApplicationEditor. |
| DiaThreading | [diathreading.md](specs/systems/dia/diathreading.md) | 2026-05-20 | 2 features Done: JobSystem/JobHandle extracted from DiaCore into new DiaThreading static lib (`Dia::Threading::` namespace); `dia.jobs.*` metrics (queue_depth, active_workers, submitted, completed) registered in JobSystem::Initialize and updated inline at Submit/completion — JobSystemModule is now a thin lifecycle adapter. Breaks DiaCore→DiaObservation cycle. |
| CluicheTest Application Flow | [applicationflow.md](specs/systems/cluichetest/applicationflow.md) | 2026-05-19 | All 5 features Done: Main PU Modules (LoggerModule, KernelModule, AssetServiceModule, UIModule), Sim PU Modules (TimeServerModule, InputStreamModule, LoadingScreenModule), Render PU Module, DummyStage Level, Manifest Configuration. Three-PU v2 topology on DiaApplicationFlow; canvas pre-bootstrap pattern; GL teardown cross-PU fence. |
| DiaApplicationFlow | [diaapplicationflow.md](specs/systems/dia/diaapplicationflow.md) | 2026-05-18 | All 8 live features Done: Module Lifecycle, Stage System, Registration, Config Format v2, Validation, Error Handling, Inspectable Interface, Stream Tap & Debug Iteration. Clean-break redesign of v1 (Phase→Stage, MessageBus→EventStream, SubscriptionManager removed). |
| DiaAssetPipeline | [diaassetpipeline.md](specs/systems/dia/diaassetpipeline.md) | 2026-05-05 | All 4 features; CLI command surface, built-in type handlers, deploy integration |
| DiaSerializer | [diaserializer.md](specs/systems/dia/diaserializer.md) | 2026-05-02 | Phase 2 + Phase 3a/b/c complete; 43 Phase 3 tests |
| DiaAssetCatalogue | [diaassetcatalogue.md](specs/systems/dia/diaassetcatalogue.md) | 2026-05-04 | All 4 features; 92 tests (34 feature + 58 exhaustive) |
| DiaIK | [diaik2d.md](specs/systems/dia/diaik2d.md) | 2026-05-02 | 6 features, 33 tests |
| DiaVisualDebugger | [diavisualdebugger.md](specs/systems/dia/diavisualdebugger.md) | 2026-05-04 | All 12 features; 114 C++ tests + 13 Vitest/jsdom tests |
| DiaAssetRuntime | [diaassetruntime.md](specs/systems/dia/diaassetruntime.md) | 2026-05-05 | 6 features, all Done; 58 tests |
| DiaAssetRuntimeEditor | [diaassetruntimeeditor.md](specs/systems/dia/diaassetruntimeeditor.md) | 2026-05-05 | 4 features, all Done; 58 editor tests; gap analysis performed and fixes applied |
| DiaAssetCatalogueEditor | [diaassetcatalogueeditor.md](specs/systems/dia/diaassetcatalogueeditor.md) | 2026-05-06 | 8 features, all Done; 73+ exhaustive tests; manual override system (mManualOverrideFlags), GetRule API, type-based graph coloring, expand no-op |
| DiaApplicationFlow — Flow Tree | [diaapplication.md](specs/systems/dia/diaapplication.md) | 2026-05-06 | 4 features Done: manifest-imports (A), pu-parent-child-tree (B), stage-manifests (C), diagame-file-format; 20 PU tree tests; CluicheTest migrated to tree ownership |

---

## Completed Standalone Features

| Feature | Spec | System | Completed |
|---------|------|--------|-----------|
| DiaApplicationFlowEditor — Stages Tab | [stages-tab.md](specs/features/dia/diaapplicationfloweditor/stages-tab.md) | DiaApplicationFlowEditor | 2026-05-20 — SVG transition graph (explicit `transitions[]` per stage, forward `<line>` + back-edge bezier `<path>`), live overlay (pulse ring on active stage, dash-flow on auto-advance edges), click-to-transition, first tab + default view in AppV2; 19 StagesTab tests + 4 AppV2 tests. Manifest v3 schema adopted (per-stage `transitions[]` + `autoAdvance`, `autoStages` removed). |
| DiaApplicationFlowEditor — Validation Fix Suggestions | [validation-fix-suggestions.md](specs/features/dia/diaapplicationfloweditor/validation-fix-suggestions.md) | DiaApplicationFlowEditor | 2026-05-20 — `SuggestedCommand` + `IssueTarget` on `ValidationIssue`; click-to-navigate in ValidationBar; one-click Fix dispatches command and re-validates; 12 ValidationBar tests. |
| Asset Lifecycle Management | [asset-lifecycle-management.md](specs/features/dia/diaassetruntime/asset-lifecycle-management.md) | DiaAssetRuntime | 2026-05-19 — Full state machine (Null→Staged→Loading→Loaded/Failed→Unloaded); `IAssetTypeHandler`/`IAssetLoadCallback`; handler registry by type prefix; auto-validation for handler-less types; `TextureHandler` + `UIHandler` real I/O; `TextureManager::LoadTexture` internalized; deferred: ShaderHandler (T15), AudioHandler (T16), Ultralight internalization (T20), shader internalization (T21), tests (T12). |
| Project Context Bar | [project-context-bar.md](specs/features/dia/diaeditor/project-context-bar.md) | DiaEditor | 2026-05-18 — `IEditorContext::LoadProject/GetProject/OnProjectChanged`; `ProjectContext` struct; toolbar project button (name/No project, dropdown with Open/Recent/Reveal/Close); `--project` CLI arg; live connection auto-load from `get_app_state`; `.cluicheproj` recent-projects persistence; all 4 existing plugins migrated to shared `ProjectContext`; `config.assetCatalogue` added to `DiaGameConfig` + serializer. |
| DiaApplicationFlow — Stream Tap & Debug Iteration | [stream-tap.md](specs/features/dia/diaapplicationflow/stream-tap.md) | DiaApplicationFlow | 2026-05-18 — Type-erased AttachTap/DetachTap/GetTapCount on IStreamStore + EventStreamStore; TapEvent/TapCallback/TapHandle; StreamInfo extended (payloadType, overflowPolicy, currentSequence, attachedReaderCount, attachedTapCount); StreamTypeRegistry serializer registration + SerializeToJson; DIA_STREAM_TYPE_WITH_SERIALIZER macro; DebugServer SubscriptionManager deleted; DebugServer migrated to tap-based subscribe/unsubscribe/connection-close; $lifecycle tap consumer; mock tap + count + stress + lifecycle integration tests. |
| data-driven-application-system | [data-driven-application-system.md](specs/features/dia/diaapplication/data-driven-application-system.md) | DiaApplicationFlow | 2026-05-02 — JsonApplicationManifestSerializer + 12 tests |

---

## Completed Standalone Features (continued)

| Feature | Spec | System | Completed |
|---------|------|--------|-----------|
| DiaCore — DirectedGraph + RelationshipIndex refactor | [directed-graph.md](specs/features/dia/diacore/directed-graph.md) | DiaCore | 2026-05-05 — `DirectedGraph<..., IDType>` with 3 policies + 7th IDType template param; RelationshipIndex rebuilt on DirectedGraph with CRC keys (~47KB stack); 78 graph tests + 92 asset catalogue tests passing |
| DiaMetrics Registry (standalone) | [metrics-registry.md](specs/features/dia/diaobservation/metrics-registry.md) · [diametrics.md](specs/systems/dia/diametrics.md) | DiaMetrics | 2026-05-18 — New `Dia/DiaMetrics/` static lib: `Counter` (per-thread lock-free shards), `Gauge` (atomic\<double\>), `Histogram` (fixed buckets, p50/p95/p99), `MetricRegistry` Meyer's singleton, `IMetricSink`, `MetricSnapshot`. 24 tests. `MetricsCollectorModule` rewritten to v2 Module API, registers 4 engine gauges. Tasks 11–13 (MetricsFileSink, SessionManager wiring) deferred to DiaObservation #5. |
| DiaObservation #1 — Skeleton + DiaLogger Fold + Async Drain | [skeleton-and-logger-fold.md](specs/features/dia/diaobservation/skeleton-and-logger-fold.md) | DiaObservation | 2026-05-18 — Created `Dia/DiaObservation/` module; folded all DiaLogger source into `Log/` subsystem (git mv, blame preserved); hard-switched namespace `Dia::Logger::` → `Dia::Observation::Log::` across 107 callers; deleted `Dia/DiaLogger/`; added async drain thread (lazy-start via `std::call_once` on first `RegisterSink`, 1ms loop, `Stop()` joins on shutdown); `LoggerModule::DoStop` calls `Stop()` before `UnregisterSink` to avoid use-after-free; added `MockSink.h` in `Testing/`; 5 new async drain tests (AC14/AC17/AC18/AC20/AC22); all 3 pipelines (googletest + cluichetest + cluicheeditor) green. |

---

## Completed Loose Ends

| Item | Completed | Notes |
|------|-----------|-------|
| DiaApplicationFlow — v1 source removal | 2026-05-20 | All v1 Manifest files deleted (`ApplicationManifest`, `ApplicationManifestLoader`, `ManifestComposer`, `ManifestValidator`, `JsonApplicationManifestSerializer`). `GameFileComposer` deleted (zero callers). `DiaGameManifestLoader` return type simplified to `bool`. v1 `ApplicationTypeRegistry`, `RegistrationMacros.h`, and `ModuleRef.h` deleted (all callers already on v2 equivalents). 4798 tests pass. |
|------|-----------|-------|
| FlatStateMachine `WildcardTransitionFiresFromAnyState` crash | 2026-05-05 | Stack overflow from `MetadataArray` embedded in each `StateDef`; fixed by moving state metadata to parallel slab `mStateMetadata` on definition, reducing `StateDef` from ~2604 B to ~432 B |
| DiaRig2D — Exhaustive tests | 2026-05-02 | 38 new tests (golden, invariant, stress, boundary, determinism, integration) in `Cluiche/Tests/GoogleTests/Rig2D/` |
| DiaVisualDebugger — implement fixed-draw-layer (feature 12) | 2026-05-04 | `FixedDrawRegistry`, `IObjectRenderer`, `TypedObjectRenderer<T>`, `IFixedPrimitiveBuffer`, `FixedPrimitiveBuffer`; default renderers Spatial/Quadtree/BVH/Hex; 24 tests |
| DiaVisualDebugger — migrate Rig2D rest pose to fixed-draw-layer | 2026-05-04 | `RigRestPoseRenderer` in `DiaRig2DVisualDebugger/`; 4 tests. RestPoseDrawer kept for dynamic callers |
| DiaVisualDebugger — migrate Geometry2D spatial structures to fixed-draw-layer | 2026-05-04 | Re-export headers in `DiaGeometry2DVisualDebugger/Renderers/`; canonical renderers in `DiaVisualDebugger/Renderers/` |
| CluicheTest shutdown — GL teardown + cross-PU stop fence | 2026-05-18 | `~RenderWindow` reordered: `setActive(true)` before ImGui/texture shutdown. Cross-PU fence: `KernelModule::sRenderContextReleased` atomic; `RenderModule::DoStop` sets it after `SetActiveContext(false)`; `KernelModule::DoStop` spins until observed. Verified clean: no GL_INVALID_OPERATION, no hang. |
| DiaCore JobSystem refactor | 2026-05-17 | All 5 phases done. P1 (`c1fe4d5`): `ThreadPool` .h/.cpp split, instance API + refcounted `JobHandle` behind a static shim. P2 (`684f9af`): `JobSystemModule` owns the `JobSystem` instance via `GetStatic()`/`GetJobSystem()`. P3 (`b88449e`): `TextureHandler` migrated to `JobHandle` + `Submit`; KernelModule wires the JobSystem in. P4+P5: deleted the static shim, `Job` struct, `ParallelFor`, parent-child fan-out, priority field, and all dead surface; renamed `Init`/`Quit` → `Initialize`/`Shutdown`; honest header docstrings (no work-stealing/priority claims); double-`Initialize` asserts; `ThreadPool` shutdown drains pending tasks (test pins the contract). Final: 11 instance-API tests, full GoogleTest suite (4245 tests) pass; cluichetest launches and exits clean. |

---

## Superseded Items

| Item | Reason |
|------|--------|
| DiaRigidBody2D — Visual Debugger | Superseded by DiaVisualDebugger / rigidbody2d-visual-debugger-stack |
| DiaSoftBody2D — Visual Debugger | Superseded by DiaVisualDebugger / softbody2d-visual-debugger-stack |
| DiaAssetPipeline system spec work | Approved — moved to Ready to Build |
| DiaAssetRuntime system spec work | Approved — moved to Ready to Build |
| DiaAssetCatalogueEditor system spec work | Approved — moved to Ready to Build |
| DiaApplicationFlow — Flow Tree (Phases A/B/C) spec work | Approved — moved to Ready to Build |

---

## Deferred

| Item | Spec | Reason |
|------|------|--------|
| HotReloadManager stubs (`CollectDependentModules` / `UpdateDependencyReferences`) | Superseded by DiaApplicationFlow v2 Stage model — hot reload via Phase/Module replacement is no longer the architecture. Never landed in the main codebase (only existed in old agent worktrees). |
| DiaEnv — env-export | [diaenv.md](specs/systems/dia/diaenv.md) | Covered by deps.json + mirrors + submodules |
| DiaEnv — docker continuous monitoring (`dia env doctor`) | [diaenv.md](specs/systems/dia/diaenv.md) | Premature until setup + verify stable (both now Done) |
