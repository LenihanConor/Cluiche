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

---

## Spec Work Needed (Draft or unset — review/approve before building)

| Item | Spec | What's needed |
|------|------|---------------|
| DiaEntity system | TBD | Needs `/spec-system` — layered ECS (components = config + asset trigger + interface). Depends on HandlePool (DiaCore), DiaMailbox. Remove old IComponent first. Research: `docs/research/entity_system/summary.md` |
| DiaMailbox system | TBD | Needs `/spec-system` — standalone typed deferred messaging with structured addressing (Entity/All/ComponentType/Self). DiaEntity depends on this. Research: `docs/research/entity_system/summary.md` |
| HandlePool\<T\> (DiaCore) | TBD | Needs `/spec-feature` under DiaCore — generic freelist + generation bump allocator, companion to existing `DiaCore/Containers/Handle.h`. DiaEntity and DiaMailbox depend on this. |
| Old IComponent removal | TBD | Needs `/spec-feature` — remove `DiaCore/Architecture/Components/` (IComponent, IComponentObject, IComponentFactory). Migrate StateMachineComponent + SkeletonComponent. Prerequisite for DiaEntity. |
| Asset Lifecycle Management | [asset-lifecycle-management.md](specs/features/dia/diaassetruntime/asset-lifecycle-management.md) | Draft — needs `/spec-review` then Approval. Extends state machine with `IAssetTypeHandler` dispatch, `Loading`/`Failed` states, progress query. |
| DiaAPI quit command | TBD | Needed for DiaTestHarness graceful shutdown. No quit command exists today (exit is UI-driven). Needs `/spec-feature` under DiaAPI |
| CluicheTest TestStages system | TBD | Needs `/spec-system` under CluicheTest — multi-stage test stages for deep engine validation (DiaRigidBody2D first). Open questions: phase vs level vs own PU; reporting mechanism. Research: `docs/research/e2e_testing/summary.md` |
| DiaStateMachineEditor system | TBD | Needs `/spec-system` — editor plugin for state machine visual debugging + design-time editing. Depends on DiaStateMachine ✅, DiaEditor |

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| DiaAssetRuntime — Hot reload path | Asset Lifecycle Management adds `Failed → Loading` retry but no `Loaded → Loading → Loaded` path. Still need a `ReloadAsset(assetId)` for live iteration (future feature on top of lifecycle management). |
| DiaApplicationFlow — Feature 6: Compile-Time Dependency Validation | Deferred by user ("let's come back and talk about 6") |
| DiaApplicationFlow — v1 source removal | v1 types (ApplicationPhase, ApplicationProcessingUnit, ApplicationModule, MessageBus, MetricsCollector, ApplicationManifest v1, HotReloadManager, ManifestComposer v1, ManifestValidator v1) still live in `Dia/DiaApplicationFlow/`. Live consumers block deletion: DiaDebugServer (`DebugServerModule.cpp` + `StateSerializer.cpp`, ~700 lines — uses `GetMessageBus()` / `GetMetricsCollector()` / `DoStart(IStartData*)` — needs design for how v2 DebugServer observes stage transitions), DiaGame::GameLoader, CluicheTest DataDrivenApplicationExample (318 lines), 4 Integration tests, Fixtures/ProcessingUnitFixture.h. v1 unit tests already deleted (commit f0fbee4). |
| HotReloadManager — `CollectDependentModules()` / `UpdateDependencyReferences()` | Placeholder stubs; needs real implementation |
| `Dia::Core::Blackboard` — general-purpose key-value store | Identified during DiaStateMachine research; useful for AI, animation, gameplay. Needs `/spec-feature` under DiaCore. |
| DiaStateMachine — `MarkValid()` exposed on definitions | Added to support serializer load path; could be misused to bypass `Validate()`. Consider making package-internal if access control becomes a concern. |
| Phase 3d — Physics body serialization | DiaRigidBody2D / DiaSoftBody2D body definitions — DiaAssetCatalogue ✅ now unblocked |
