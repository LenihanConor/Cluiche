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
| DiaObservation Skeleton + DiaLogger Fold + Async Drain | [skeleton-and-logger-fold.md](specs/features/dia/diaobservation/skeleton-and-logger-fold.md) | DiaObservation — feature #1 of 7; ships first, blocks all other DiaObservation work; folds `Dia/DiaLogger/` into `Dia/DiaObservation/Log/` (155 caller files rewritten) and moves logger drain off the main PU thread |

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

## Spec Work Needed (Draft or unset — review/approve before building)

| Item | Spec | What's needed |
|------|------|---------------|
| Asset Lifecycle Management | [asset-lifecycle-management.md](specs/features/dia/diaassetruntime/asset-lifecycle-management.md) | Draft — needs `/spec-review` then Approval. Extends state machine with `IAssetTypeHandler` dispatch, `Loading`/`Failed` states, progress query. |
| DiaAPI quit command | TBD | Needed for DiaTestHarness graceful shutdown. No quit command exists today (exit is UI-driven). Needs `/spec-feature` under DiaAPI |
| CluicheTest TestStages system | TBD | Needs `/spec-system` under CluicheTest — multi-stage test stages for deep engine validation (DiaRigidBody2D first). Open questions: phase vs level vs own PU; reporting mechanism. Research: `docs/research/e2e_testing/summary.md` |
| DiaStateMachineEditor system | TBD | Needs `/spec-system` — editor plugin for state machine visual debugging + design-time editing. Depends on DiaStateMachine ✅, DiaEditor |
| DiaObservation features 2–7 | [diaobservation.md](specs/systems/dia/diaobservation.md) | System Approved (2026-05-17); feature #1 Approved and Ready to Build. Need `/spec-feature` (in this order, parallel where noted): (#2) DiaObservation Foundation — session directory, `JsonLineSink`, retention ring, crash auto-dump, exit-reason capture, `session.json` schema; (#3) DiaObservation Config — `.diagame` `observation` block + CLI overrides; (#4) DiaTrace Spans, (#5) DiaMetrics Registry, (#6) DiaHealth Reporting — independent, can spec in parallel; (#7) DebugServer Observation Bridge — last, depends on at least one pillar shipping. Research: `docs/research/observ_telemetry/summary.md`. **Terminal goal: substrate for future `DiaE2E` test framework.** |
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
| DiaStateMachine — `MarkValid()` exposed on definitions | Added to support serializer load path; could be misused to bypass `Validate()`. Consider making package-internal if access control becomes a concern. |
| Phase 3d — Physics body serialization | DiaRigidBody2D / DiaSoftBody2D body definitions — DiaAssetCatalogue ✅ now unblocked |
| DiaLogger system spec final supersede | `docs/specs/systems/dia/dialogger.md` is currently `Superseded (pending)`. Once DiaObservation feature #1 lands and `Dia/DiaLogger/` is deleted from disk, flip the status to `Superseded` and add a forwarding pointer at the top of the document. Tracked as DiaObservation feature #1 AC12. |
| DiaApplicationFlow MetricsCollectorModule replacement | Existing `Dia/DiaApplicationFlow/Metrics/MetricsCollectorModule.{h,cpp}` is hand-rolled (FPS, frame-time, memory, uptime). DiaObservation feature #5 (DiaMetrics Registry) subsumes it — those four become registered gauges populated by the same module. Replacement is part of feature #5, not a separate item; flagged here so the connection is visible. |
| DiaDebugServer ServerStats subsumption | `Dia/DiaDebugServer/DebugServer.h` `ServerStats` struct + `BroadcastCoreMetrics` method are hand-rolled. DiaObservation feature #5 replaces ServerStats fields with registered counters/gauges; feature #7 (DebugServer Observation Bridge) replaces `BroadcastCoreMetrics` with the registry-driven version. Flagged here for visibility. |
