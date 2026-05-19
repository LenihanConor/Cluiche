# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

---

## In Progress

| Item | Spec | What's next |
|------|------|-------------|
| DiaApplicationFlow | [diaapplicationflow.md](specs/systems/dia/diaapplicationflow.md) | All 8 feature specs Approved. Next: implement P1 (Framework Core: Module Lifecycle + Stage System + Error Handling). Research: `docs/research/diapp_simplif/summary.md` |
| CluicheTest Application Flow | [applicationflow.md](specs/systems/cluichetest/applicationflow.md) | All 5 feature specs Approved. Blocked on DiaApplicationFlow implementation. |

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| System | Spec | Features | Depends On |
|--------|------|----------|------------|
| DiaApplicationFlowEditor | [diaapplicationfloweditor.md](specs/systems/dia/diaapplicationfloweditor.md) | 15 features (Draft) — blocked on DiaApplicationFlow implementation | DiaEditor ✅, DiaWebSocket ✅, DiaApplicationFlow (in progress) |

---

### Standalone Features (system Done, feature Approved)

| Feature | Spec | System |
|---------|------|--------|
| per-app-bin-layout | [per-app-bin-layout.md](specs/features/dia/diapipeline/per-app-bin-layout.md) | DiaPipeline ✅ |
| Harness Core | [harness-core.md](specs/features/dia/diatestharness/harness-core.md) | DiaTestHarness |
| Smoke Test Scenario | [smoke-test-scenario.md](specs/features/cluichetest/cluichetestscenarios/smoke-test-scenario.md) | CluicheTestScenarios (depends on Harness Core) |
| HandlePool\<T\> | [handle-pool.md](specs/features/dia/diacore/handle-pool.md) | DiaCore ✅ — foundation for DiaMailbox + DiaEntity |

---

### DiaMetrics + DiaObservation (all 7+1 features Approved — implement in order)

`DiaMetrics` has no session dependency and can be built independently. DiaObservation features are serial: #1 → #2 → #3 → #4/#6 (parallel) → #5 (DiaMetrics wiring) → #7 (last).

| Feature | Spec | Notes |
|---------|------|-------|
| DiaMetrics Registry | [metrics-registry.md](specs/features/dia/diaobservation/metrics-registry.md) · [diametrics.md](specs/systems/dia/diametrics.md) | **Build first** — no observation dep; standalone `Dia/DiaMetrics/` module |
| DiaObservation #1 — Skeleton + DiaLogger Fold | [skeleton-and-logger-fold.md](specs/features/dia/diaobservation/skeleton-and-logger-fold.md) | Ships first; blocks all other DiaObservation features; 155 caller include rewrites |
| DiaObservation #2 — Foundation | [foundation.md](specs/features/dia/diaobservation/foundation.md) | Blocked on #1; `SessionManager`, session directory, `ObservationFileSink`, retention ring, crash dump, `session.json` + `log.jsonl` schemas frozen v1.0 |
| DiaObservation #3 — Config | [config.md](specs/features/dia/diaobservation/config.md) | Blocked on #2; `ObservationConfigLoader`, `.diagame` block, per-channel log levels, all sinks configurable |
| DiaObservation #4 — DiaTrace Spans | [trace-spans.md](specs/features/dia/diaobservation/trace-spans.md) | Blocked on #2; `DIA_TRACE_ZONE` macros, `Tracer` singleton + drain thread, `trace.jsonl` |
| DiaObservation #5 — DiaMetrics wiring | [metrics-registry.md](specs/features/dia/diaobservation/metrics-registry.md) | Blocked on #2; `MetricsFileSink` in DiaObservation, `SessionManager::Tick` snapshot timer, `MetricsCollectorModule` rewrite |
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
| DiaAssetRuntime — add metrics | Register in `AssetRuntimeModule::DoInit`: `dia.assets.loaded` (Gauge), `dia.assets.loading` (Gauge), `dia.assets.failed` (Counter), `dia.assets.load_time_ms` (Histogram per asset type). Update in `DoUpdate` / load callbacks. Needs `/spec-feature` under DiaAssetRuntime. **Do after DiaMetrics ships.** |
| DiaDebugServer — add metrics | Register in server init: `dia.debugserver.connections` (Gauge), `dia.debugserver.subscriptions` (Gauge), `dia.debugserver.messages_sent` (Counter), `dia.debugserver.tick_ms` (Histogram). DiaDebugServer already depends on `DiaMetrics` directly (via `ObservationBridge`). Needs `/spec-feature` under DiaDebugServer. **Do after DiaMetrics ships.** |
| DiaInput — add metrics | Register in input module init: `dia.input.sources` (Gauge), `dia.input.events_per_frame` (Histogram), `dia.input.active_gamepads` (Gauge). Update per-frame. Needs `/spec-feature` under DiaInput. **Do after DiaMetrics ships.** |
| DiaThreading — extract JobSystem from DiaCore + add metrics | `JobSystem` + `ThreadPool` move from `DiaCore` to a new `DiaThreading` module (`DiaThreading → DiaCore + DiaMetrics`). Breaks the circular dep that prevents JobSystem from registering its own gauges. Metrics to add: `dia.jobs.queue_depth`, `dia.jobs.submitted`, `dia.jobs.completed`, `dia.jobs.active_workers`. Needs `/spec-system` (or `/spec-feature` if scoped to extraction only). **Do after DiaMetrics ships** — no value extracting before the registry exists. |
| Phase 3d — Physics body serialization | DiaRigidBody2D / DiaSoftBody2D body definitions — DiaAssetCatalogue ✅ now unblocked |
| DiaLogger system spec final supersede | `docs/specs/systems/dia/dialogger.md` is currently `Superseded (pending)`. Once DiaObservation feature #1 lands and `Dia/DiaLogger/` is deleted from disk, flip the status to `Superseded` and add a forwarding pointer at the top of the document. Tracked as DiaObservation feature #1 AC12. |
| DiaApplicationFlow MetricsCollectorModule replacement | Existing `Dia/DiaApplicationFlow/Metrics/MetricsCollectorModule.{h,cpp}` is hand-rolled (FPS, frame-time, memory, uptime). DiaMetrics Registry feature subsumes it — those four become registered `Gauge` primitives. Replacement is part of the DiaObservation #5 task list, not a separate item; flagged here so the connection is visible. |
| DiaDebugServer ServerStats subsumption | `Dia/DiaDebugServer/DebugServer.h` `ServerStats` struct + `BroadcastCoreMetrics` method are hand-rolled. DiaObservation #5 (DiaMetrics wiring) replaces `ServerStats` fields with registered gauges; DiaObservation #7 (DebugServer Bridge) replaces `BroadcastCoreMetrics` with the registry-driven `ObservationBridge`. Flagged here for visibility. |
| CluicheTest shutdown — verify recent fixes | After 2026-05-17 changes (1) `~RenderWindow` GL teardown reorder — `setActive(true)` now runs before `mImGuiBackend.Shutdown()` and `mTextureHandler.Shutdown()`; (2) cross-PU stop fence — new `KernelModule::sRenderContextReleased` atomic; `RenderModule::DoStop` sets it true after `SetActiveContext(false)`, `KernelModule::DoStop` returns `kStopping` until it observes true. Build was blocked by a hung CluicheTest holding the .exe file lock — needed reboot. **Verification still owed:** `dia pipeline --target cluichetest` then `dia run cluichetest` several times, opening and closing the window, to confirm no GL_INVALID_OPERATION and no hang in `~RenderWindow`. Files: `Dia/DiaSFML/RenderWindow.cpp`, `Cluiche/CluicheGameBaseline/Modules/KernelModule.{h,cpp}`, `Cluiche/CluicheGameBaseline/Modules/RenderModule.cpp`. |
