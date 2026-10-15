# Cluiche Backlog History

Completed items moved from BACKLOG.md. For active work see [BACKLOG.md](BACKLOG.md).

---

## Completed Tooling

| Item | Completed | Notes |
|------|-----------|-------|
| Webix removal (vendored 5.2.1 folder, catalogue entries, live docs; `dummyStage.html` rewritten as a styled plain HTML/CSS Exit Level panel) | 2026-10-13 | [webix_removal research](research/webix_removal/summary.md); commits `6de511c3`, `ce2829d9`, `82e9fd7c`, `da95d410`. Pending: manual check of the DummyStage panel render and Exit Level in Ultralight. Alpine/React game-UI convention is unaffected (spec already Done). |
| CoW: `dia scaffold app` (data-driven target registry + templates, command, `ScratchApp` boot proof, docs) | before 2026-10-12 | [scaffold-app.md](specs/applications/dia/systems/diacli/scaffold-app.md); F-1 plan task 9 |
| CoW: `new-cow-stage` skill (AC1 to AC3) | before 2026-10-12 | F-1 plan task 7; trial run (AC4/AC5) still open in the backlog |
| CoW: `dia scaffold stage --app` | before 2026-10-12 | F-1 plan task 6; 22 new tests, 148 scaffold tests pass |
| CI Architecture Gates | 2026-07-09 | `dia check deps` exit code fixed, `dia check arch` wired, `dia check clones` (PMD CPD) added, `.github/workflows/ci.yml` with hard-gate + advisory jobs. |

---

## Completed Systems

| System | Spec | Completed | Notes |
|--------|------|-----------|-------|
| DiaPathfinding: Rectangular Hex Grid | [hex-rectangular-grid.md](specs/applications/dia/systems/diapathfinding/hex-rectangular-grid.md) | 2026-10-14 | `HexPathGrid(width, height)` rectangular flat-top odd-q layout, `HexTopology` (neighbours, distance, direction helpers, cell geometry), corrected hex heuristic, `HexFlowAdapter` support, `GridVisibilitySystem` compile-time guard. 252 tests pass. Amended from pointy-top odd-r during implementation. CoW needs for S-1, A4 Features 3–6 |
| CoW GameSpeedController (Features 1 and 2) | [gamespeedcontroller.md](specs/applications/cow/systems/gamespeedcontroller/gamespeedcontroller.md) | 2026-10-13 | `GameSpeedController` core + `GameSpeedControllerModule` (pause as time scale 0, day `SimulationTick`, 12 `cow.gamespeed.*` metrics, health reporter). 105 tests, full suite and ASan pass. Save participant still open in BACKLOG (needs E1 `SaveLoadModule`) |
| DiaAttribute Feature 7 (Band-Based Initialization) | @docs/specs/applications/dia/systems/diaattribute/band-based-initialization.md | 2026-10-12 | Schema-defined named band ranges, `InitializeFromBands()` midpoint resolution, `band_schema_version` support, fallback to default_value. 18 unit tests (11 core + 7 edge cases), 154/154 full suite passes. ~3 hours. |
|--------|------|-----------|-------|
| CoW F-1: Boot choice (Play entry only) | [boot-choice.md](specs/applications/cow/systems/f1-harness/boot-choice.md) | before 2026-10-12 | F-1 plan task 5; spec marked Done. Recorded during the 2026-10-12 backlog cleanup |
| CoW F-1: CoW app skeleton (B1 to B7: `cow` target, `sln-add --root`, project, manifests, `FrontEnd` stub, `Main.cpp`, verify, observation scan) | [cow-app-skeleton.md](specs/applications/cow/systems/f1-harness/cow-app-skeleton.md) | before 2026-10-12 | F-1 plan tasks 2 to 4. Promotion and skeleton edits to baseline, CluicheTest, DiaCLI and `pipeline.toml` were signed off in the F-1 spec |
| CoW F-1: BootMenuModule promotion + `StageStatusModule` | [bootmenu-promotion.md](specs/applications/cow/systems/f1-harness/bootmenu-promotion.md) | before 2026-10-12 | F-1 plan task 1 |
| DiaBgfx3D | [diabgfx3d.md](specs/applications/dia/systems/diabgfx3d/diabgfx3d.md) | 2026-07-09 | Phase 2 3D rendering ship gate. gpu-resources (MaterialRegistry + MeshGpuCache), 3d-renderers (MeshRenderer + ShadowRenderer + shaders), canvas3d (Canvas3D ProcessFrame 2D+3D dispatch), mesh-texture-pipeline (albedo + normal maps + TBN), pbr-shading (GGX BRDF + ORM), multiple-directional-lights (8-light array uniforms). |
| DiaBlackboardInspector | [diablackboardinspector.md](specs/applications/dia/systems/diablackboardinspector/diablackboardinspector.md) | 2026-07-09 | Live runtime blackboard visibility in CluicheEditor. blackboard-registry (BlackboardRegistry + serializer opt-in + IBlackboardObserver::GetId), blackboard-inspector-source (ChangeDetectedSourceBase + EventStream → DebugServer), blackboard-inspector-plugin (LiveConnectionPluginBase + dockable HTML panel). |
| DiaRenderTest CLI Pipeline | [diarendertest.md](specs/applications/dia/systems/diarendertest/diarendertest.md) | 2026-07-09 | Offline render regression pipeline. png-writer, diff-engine, expectations, python-tools, metrics-writer, cluichetest-integration all shipped. Unblocks RenderTestPlugin (CluicheEditor). |
| DiaEntityInspector | [diaentityinspector.md](specs/applications/dia/systems/diaentityinspector/diaentityinspector.md) | 2026-06-06 | 4 features: entity-inspector-panel, query-browser-tab, mailbox-traffic-monitor, entity-watch-list. Runtime debug via WebSocket. Entity list, fields tab with type-aware widgets + editing, queries tab, mailbox ring-buffer tab, watch list with reconnect persistence. 27 tests (15 integration + 12 serializer). |
| DiaSceneEditor | [diasceneeditor.md](specs/applications/dia/systems/diasceneeditor/diasceneeditor.md) | 2026-06-06 | 6 features: scene-hierarchy-panel, entity-placement-crud, change-blueprint, layer-authoring, camera-light-authoring, scene-validation. Spatial authoring of `.diascene` files. Full CRUD (add/duplicate/delete/rename/enable), change-blueprint analysis + transfer, layer reorder, active camera enforcement, light affects-layers, scene validation (6 rules), world bounds editor. 22 tasks. |
| DiaEntityTemplateEditor | DiaEntityTemplateEditor | 2026-06-03 | Blueprint list panel (grouped by type), component accordion + field editing, `.diaentitytemplate`/`.diacamera`/`.dialight` I/O, searchable component picker, C++ code defaults from schema, blueprint-level field overrides (green border + clear), cross-scene usage panel, cascade warnings on add/remove. `registered-types-schema` pipeline (`dia reflect`) generates `registeredtypes.diaschema` from game binary. |
| DiaCamera2D | [diacamera2d.md](specs/applications/dia/systems/diacamera2d/diacamera2d.md) | 2026-06-02 | Camera2D type (migrated from DiaGraphics), CameraRegistry2D, ICameraBehaviour + factory, 8 engine behaviours (Follow, SmoothDamp, Deadzone, BoundsClamp, ScreenShake, ZoomToFit, Pan, Zoom), ViewportTransform. |
| DiaLighting2D | [dialighting2d.md](specs/applications/dia/systems/dialighting2d/dialighting2d.md) | 2026-06-02 | PointLight2D type, LightRegistry2D, layer-mask storage, query by layer. v1 data model only (no rendering output yet). |
| diaentitytemplate | [diaentitytemplate](specs/applications/dia/systems/diaentity/diaentity.md) | 2026-05-24 | All 10 features Done. Layered ECS — `Domain` container, generational 64-bit `Entity` handles via `Handle<EntityTag>`, IComponent with end-of-frame mutation queue (add/remove/destroy 3-pass). `DIA_COMPONENT` + `FIELD` + `DIA_COMPONENT_REGISTER` macro DSL with `ComponentTypeDesc` + `ComponentRegistry`. REQUIRES dependency validation. JsonBlueprintLoader (3-pass: instantiate → patch EntityRefs → validate). `EntityRef<T>` cross-entity refs. Parent/child hierarchy with cycle detection + `QueueDestroySubtree`. `EntityRouter : IMailboxRouter` (4 address kinds). `QueryView<TComponents...>` with signature-keyed cache. `Domain::Update` opt-in via `DIA_UPDATABLE`. `IEntityInspectable` with ReadField/WriteField via read-modify-write. DomainHealth + 4 metrics (entity.count, entity.components, entity.mutations, entity.query_rebuilds). DIA_TRACE_ZONE + DIA_PROFILE_SCOPE across Update/EndOfFrame/ApplyMutations/RebuildQueryCache. PD-003 / AD-005 marked Superseded. 107/107 tests GREEN (94 spec + 7 stress + 6 integration). |
| DiaReflect | [diareflect.md](specs/applications/dia/systems/diareflect/diareflect.md) | 2026-05-21 | All phases Done. C7 archive pattern + macro-DSL (DIA_SERIALIZE/DIA_FIELD). JSON + binary archives, container specializations (T[N], DynamicArrayC), polymorphic registry, field attributes (Required/Range/AssetRef). DiaMaths + DiaCore Strings/PathStoreConfig serializers added. DiaCore/Type deleted (31 files); BasicTypeDefines.h relocated to DiaCore/Core/. 5065 tests GREEN. |
| DiaMailbox | [diamailbox.md](specs/applications/dia/systems/diamailbox/diamailbox.md) | 2026-05-21 | All 5 features Done: module-and-build, address-and-types, typed-queue, subscriptions, routers. Generic typed deferred messaging; `Address = (StringCRC, uint64_t)`; per-type ring buffers; poll-drain pattern; HandlePool-backed subscriptions; IMailboxRouter interface + MockRouter. DIA_LOG_WARNING + dia.mailbox.{sent,dropped,drained} metrics. 62/62 tests GREEN (49 spec + 13 exhaustive boundary/stress). |
| DiaObservation | [diaobservation.md](specs/applications/dia/systems/diaobservation/diaobservation.md) | 2026-05-20 | All 13 features Done. 5-pillar observability system (logs, traces, metrics, health, profiling). DiaLogger folded in and deleted. Session directory + session.json + log/trace/metric/profile.jsonl schemas frozen at v1.0. OTel wire-compat without SDK. Async drain thread, per-thread rings, retention ring. DiaMetrics standalone merged into DiaObservation/Metric/. DebugServer bridge via WebSocket topics. Domain instrumentation across ProcessingUnit, DiaGraphics, DiaStream, DiaAssetRuntime. IHealthReporter on Module + AssetServiceModule. dia.assets.* + dia.debugserver.* metrics registered. |
| DiaApplicationFlowEditor | [diaapplicationfloweditor.md](specs/applications/dia/systems/diaapplicationfloweditor/diaapplicationfloweditor.md) | 2026-05-20 | All 17 features implemented, all 56 tasks Done including manual E2E gate. Full v2 `.diaapp` manifest editor: C++ data model + command history + validation + live connection; React UI with GraphView, ModulePresenceGrid, StreamsTab, StagesTab, PUInspector, ModuleInspector, StageConfiguration, ValidationBar, LiveConnectionButton, LiveTransitionPanel, RiskyChangeDialog, FileConflictDialog. Manifest v3 schema (per-stage `transitions[]` + `autoAdvance`); 154 React tests + 141 C++ tests. Replaces v1 DiaApplicationEditor. |
| DiaThreading | [diathreading.md](specs/applications/dia/systems/diathreading/diathreading.md) | 2026-05-20 | 2 features Done: JobSystem/JobHandle extracted from DiaCore into new DiaThreading static lib (`Dia::Threading::` namespace); `dia.jobs.*` metrics (queue_depth, active_workers, submitted, completed) registered in JobSystem::Initialize and updated inline at Submit/completion — JobSystemModule is now a thin lifecycle adapter. Breaks DiaCore→DiaObservation cycle. |
| CluicheTest Application Flow | [applicationflow.md](specs/applications/cluichetest/systems/applicationflow/applicationflow.md) | 2026-05-19 | All 5 features Done: Main PU Modules (LoggerModule, KernelModule, AssetServiceModule, UIModule), Sim PU Modules (TimeServerModule, InputStreamModule, LoadingScreenModule), Render PU Module, DummyStage Level, Manifest Configuration. Three-PU v2 topology on DiaApplicationFlow; canvas pre-bootstrap pattern; GL teardown cross-PU fence. |
| DiaApplicationFlow | [diaapplicationflow.md](specs/applications/dia/systems/diaapplicationflow/diaapplicationflow.md) | 2026-05-18 | All 8 live features Done: Module Lifecycle, Stage System, Registration, Config Format v2, Validation, Error Handling, Inspectable Interface, Stream Tap & Debug Iteration. Clean-break redesign of v1 (Phase→Stage, MessageBus→EventStream, SubscriptionManager removed). |
| DiaAssetPipeline | [diaassetpipeline.md](specs/applications/dia/systems/diaassetpipeline/diaassetpipeline.md) | 2026-05-05 | All 4 features; CLI command surface, built-in type handlers, deploy integration |
| DiaSerializer | [diaserializer.md](specs/applications/dia/systems/diaserializer/diaserializer.md) | 2026-05-02 | Phase 2 + Phase 3a/b/c complete; 43 Phase 3 tests |
| DiaAssetCatalogue | [diaassetcatalogue.md](specs/applications/dia/systems/diaassetcatalogue/diaassetcatalogue.md) | 2026-05-04 | All 4 features; 92 tests (34 feature + 58 exhaustive) |
| DiaIK | [diaik2d.md](specs/applications/dia/systems/diaik2d/diaik2d.md) | 2026-05-02 | 6 features, 33 tests |
| DiaVisualDebugger | [diavisualdebugger.md](specs/applications/dia/systems/diavisualdebugger/diavisualdebugger.md) | 2026-05-04 | All 12 features; 114 C++ tests + 13 Vitest/jsdom tests |
| DiaAssetRuntime | [diaassetruntime.md](specs/applications/dia/systems/diaassetruntime/diaassetruntime.md) | 2026-05-05 | 6 features, all Done; 58 tests |
| DiaAssetRuntimeEditor | diaassetruntimeeditor | 2026-05-05 | 4 features, all Done; 58 editor tests; gap analysis performed and fixes applied |
| DiaAssetCatalogueEditor | [diaassetcatalogueeditor.md](specs/applications/dia/systems/diaassetcatalogueeditor/diaassetcatalogueeditor.md) | 2026-05-06 | 8 features, all Done; 73+ exhaustive tests; manual override system (mManualOverrideFlags), GetRule API, type-based graph coloring, expand no-op |
| DiaApplicationFlow — Flow Tree | [diaapplication.md](specs/applications/dia/systems/diaapplication/diaapplication.md) | 2026-05-06 | 4 features Done: manifest-imports (A), pu-parent-child-tree (B), stage-manifests (C), diagame-file-format; 20 PU tree tests; CluicheTest migrated to tree ownership |
| DiaLighting3D | [dialighting3d.md](specs/applications/dia/systems/dialighting3d/dialighting3d.md) ✅ | 2026-06-08 | All 9 tasks complete. 59 tests (35 unit + 24 boundary/stress/golden/invariant). RGBA moved to DiaCore as prerequisite. |
| DiaScene3D | [diascene3d.md](specs/applications/dia/systems/diascene3d/diascene3d.md) ✅ | 2026-06-08 | All 9 tasks complete. 59 tests (35 unit + 24 boundary/stress/golden/invariant). RGBA moved to DiaCore as prerequisite. |
| diacamera3d | [spec](specs/README.md) | 2026-06-08 | Marked done by `dia docs spec-done` |
| app-editor-actions | [spec](specs/README.md) | 2026-06-08 | Marked done by `dia docs spec-done` |
| diachatplugin | [spec](specs/README.md) | 2026-06-10 | Marked done by `dia docs spec-done` |
| diamesh3d | [spec](specs/README.md) | 2026-06-10 | Marked done by `dia docs spec-done` |
| spline3d | [spec](specs/) | 2026-06-23 | Marked done by `dia docs spec-done` |
| light-path-behaviour | [spec](specs/) | 2026-06-23 | Marked done by `dia docs spec-done` |
| mesh3d-bounds-and-origins | [spec](specs/) | 2026-06-23 | Marked done by `dia docs spec-done` |
| debug-widget-config | [spec](specs/) | 2026-06-23 | Marked done by `dia docs spec-done` |
| Mesh3DRenderSystemTestStage | [mesh3d-render-system-stage.md](specs/applications/cluichetest/systems/teststages/mesh3d-render-system-stage.md) | 2026-07-07 | Implemented: 3 cubes + avocado + silent-skip ghost; fixed Lambert NdotL direction bug in fs_mesh.sc |
| ci-architecture-gates | [spec](specs/) | 2026-07-09 | Marked done by `dia docs spec-done` |
| DiaBlackboardInspector | [diablackboardinspector.md](specs/applications/dia/systems/diablackboardinspector/diablackboardinspector.md) | 2026-07-09 | Moved by `dia docs backlog move` |
| mesh-texture-pipeline | [spec](specs/) | 2026-07-09 | Marked done by `dia docs spec-done` |
| diarendertest | [spec](specs/) | 2026-07-09 | Marked done by `dia docs spec-done` |
| diabgfx3d | [spec](specs/) | 2026-07-09 | Marked done by `dia docs spec-done` |
| `dia diagnose --last-run` CLI command | Automate crash triage: find latest session log, extract last module transition + all ERROR entries, show incomplete E2E report, check `%LocalAppData%\CrashDumps`. Saves the manual log-digging cycle after every crash. | 2026-07-10 | Implemented in dia_cli/cli/diagnose.py; surfaces truncated log, last stage/module, errors, E2E report, crash dumps in one command |
| DiaConsole | [diaconsole.md](specs/applications/dia/systems/diaconsole/diaconsole.md) | 2026-10-08 | All 7 features Done: console-command-model, console-native-shell, console-typed-results, console-project-context, console-presets, console-desktop-install (2026-09-30), console-run-tabs (2026-09-30, manually verified in-browser 2026-10-08). Native-window web console over DiaCLI's Click command tree: reflected registry, NDJSON execution streaming, typed results, presets, project/target context, packaged desktop exe, concurrent-run tabs. |
| DiaAIBudget | IAIBudgetedSystem, AIBudgetScheduler, AIBudgetModule, Budget Metrics | 2026-07-29 | All features implemented and tested (46 tests) |
| DiaUtilityAI | ResponseCurve, ActionDef, UtilitySet, AsyncEvaluation, GroupConsideration, UtilitySetComponent, ScoreOverlay, TestUtilities | 2026-07-29 | All features implemented and tested (46 tests) |
| DiaHTN | [diahtn.md](specs/applications/dia/systems/diahtn/diahtn.md) | 2026-07-29 | All features implemented and tested (46 tests) |
| DiaAIDecisionInspector | — | 2026-07-29 | All features implemented and tested (46 tests) |
| AIDecisionTestStage + scenario | Integration proof that DiaCondition + DiaRules + DiaUtilityAI + DiaAIBudget all wire together correctly. Single entity with BlackboardComponent + RuleSetComponent + UtilitySetComponent. Checkpoints: `ai.condition.health_low_passes`, `ai.condition.enemy_visible_passes`, `ai.rules.call_for_help_fired`, `ai.utility.flee_wins`, `ai.budget.work_item_completed`. Prerequisite: all four AI systems built. | 2026-07-29 | All features implemented and tested (46 tests) |
| DiaRules | RuleActionRegistry, RuleSet, RuleSetComponent, Test Utilities | 2026-07-30 | All features implemented; 85 GoogleTests pass (20 ActionRegistry + 39 RuleSet + 13 Component + 13 TestHelpers); spec marked Done 2026-07-30 |
| DiaUtilityAI | ResponseCurve, ActionDef, UtilitySet, AsyncEvaluation, GroupConsideration, UtilitySetComponent, ScoreOverlay, TestUtilities | 2026-07-30 | All features implemented; 85 GoogleTests pass (20 ActionRegistry + 39 RuleSet + 13 Component + 13 TestHelpers); spec marked Done 2026-07-30 |
| DiaHTN | OperatorRegistry, RuleActionBridge, HTNDomain, HTNPlan, SyncPlanner, AsyncPlanning, HTNPlannerComponent, TestUtilities | 2026-07-30 | All features implemented; 85 GoogleTests pass (20 ActionRegistry + 39 RuleSet + 13 Component + 13 TestHelpers); spec marked Done 2026-07-30 |
| DiaAIDecisionInspector | — | 2026-07-30 | All features implemented; 85 GoogleTests pass (20 ActionRegistry + 39 RuleSet + 13 Component + 13 TestHelpers); spec marked Done 2026-07-30 |
| diautilityai | [spec](specs/) | 2026-07-31 | Marked done by `dia docs spec-done` |
| diahtn | [spec](specs/) | 2026-07-31 | Marked done by `dia docs spec-done` |
| diaflowfield | [spec](specs/) | 2026-08-03 | Marked done by `dia docs spec-done` |
| diasteering | [spec](specs/) | 2026-08-03 | Marked done by `dia docs spec-done` |
| diascalarfield | [spec](specs/) | 2026-08-03 | Marked done by `dia docs spec-done` |
| DiaAIBudget | IAIBudgetedSystem, AIBudgetScheduler, AIBudgetModule, Budget Metrics | 2026-08-05 | Moved from backlog — already implemented |
| DiaCondition | IConditionContext, ConditionRegistry, ConditionExpr, ConditionGuardAdapter, Test Utilities | 2026-08-05 | Moved from backlog — already implemented |
| DiaFlowField | CFlowFieldGraph concept, SquareFlowAdapter + HexFlowAdapter, FlowField, ComputeFlowField, FlowFieldCache, Test Utilities | 2026-08-05 | Moved from backlog — already implemented |
| DiaScalarField | CFieldTopology concept, topologies, decay/propagation policies, double-buffering, write shapes, gradient/spatial queries, multi-field combine, ScalarFieldOverlay, Test Utilities | 2026-08-05 | Moved from backlog — already implemented |
| DiaAIDecisionInspector | CluicheEditor panel: blackboard slots → condition results → fired rules → utility scores | 2026-08-05 | Moved from backlog — already implemented |
| diaentityspatial | [spec](specs/) | 2026-08-05 | Marked done by `dia docs spec-done` |
| EntitySpatialTestStage | TestStages | 2026-08-05 | Moved by `dia docs backlog move` |
| diascalarfieldvisualdebugger | [spec](specs/) | 2026-08-05 | Marked done by `dia docs spec-done` |
| diasensor | [spec](specs/) | 2026-08-05 | Marked done by `dia docs spec-done` |
| diaeconomy | [spec](specs/) | 2026-08-06 | Marked done by `dia docs spec-done` |
| DiaEntitySpatial | SpatialComponent, EntitySpatialIndex, EntitySpatialModule, Test Utilities | 2026-08-06 | Moved from backlog — already implemented |
| DiaAICallout | Callout (emit/query/claim/release), CalloutHandle, CalloutRegistry, TTL expiry, Test Utilities | 2026-08-06 | Moved from backlog — already implemented |
| DiaAICalloutVisualDebugger | CalloutRegistryDebugger (IDebugDomain), CalloutRadiiDrawer (green=unclaimed, red=claimed) | 2026-08-14 | Tasks 10–13 done; 20 GoogleTests pass |
| EntitySpatialTestStage | TestStages | 2026-08-06 | Moved from backlog — already implemented |
| DiaScalarFieldVisualDebugger | Gradient arrow overlay (arrowhead triangle + hex axial-to-world fix), heatmap, layer names | 2026-08-06 | Moved from backlog — already implemented || DiaSensor | SensorResultsComponent, SightSensorComponent, ProximitySensorComponent, DamageSensorComponent, SoundSensorComponent, SensorBlackboardAdapter, SensorModule, Test Utilities | 2026-08-06 | Moved from backlog — already implemented |
| DiaSensorVisualDebugger | World-space overlay: sight cones + proximity circles; IVisualDebugger impl; reads SensorResultsComponent | 2026-08-06 | Moved from backlog — already implemented |
| DiaEconomy | Named resource pools, earn/spend/transfer/tick, rate-of-change metrics | 2026-08-06 | Moved from backlog — already implemented |
| diaobjective | [spec](specs/) | 2026-08-06 | Marked done by `dia docs spec-done` |
| DiaTriggerScript | Data-driven level events — spatial/temporal/state/count triggers, four action types, ITriggerActionHandler extension point, TriggerFiredEvent on DiaStreams | 2026-08-06 | Moved by `dia docs backlog move` |
| diaentityspawner | [spec](specs/) | 2026-08-06 | Marked done by `dia docs spec-done` |
| diasavegame | [spec](specs/) | 2026-08-10 | Marked done by `dia docs spec-done` |
| ArenaTestStage (CluicheTest) | **In Progress (5/11 tasks done)** — scaffold + JSON assets + header + OnStart/LoadTriggerScript + SpawnWave/EnemyAgent done; remaining: DoUpdate frame loop (T6), metrics (T7), ImGui visuals (T8), DoStop (T9), pytest (T10), E2E verify (T11) | 2026-08-10 | Moved by `dia docs backlog move` |
| ArenaTestStage spec → backlog | Promoted from loose end to Approved spec + backlog entry 2026-08-07. See Ready to Build above. | 2026-08-10 | Moved by `dia docs backlog move` |
| DiaEconomyInspector | EconomyInstancesSource, EconomyModifiersSource, EconomyEventsSource, EconomySchemaSource, dockable editor plugin | 2026-08-10 | All 9 tasks done: 4 game-side inspector sources, EconomyInspectorModule (SimPU Bind/Unbind), React UI (4 tabs, 44 Vitest tests), 38 GoogleTests, E2E scenario with DiaClient topic subscriptions |
| DiaScalarFieldInspector | [spec](specs/applications/dia/systems/diascalarfieldinspector/diascalarfieldinspector.md) | 2026-08-11 | Dockable editor panel for scalar field inspection. WriteRadial + WriteBox widgets, C++ tests, JS logic test page. |
| DiaDebugDomain | [spec](specs/applications/dia/systems/diadebugdomain/diadebugdomain.md) | 2026-08-12 | IDebugDomain interface + DiaDebugDomainRegistry + DiaDebugPanel HTML overlay (Ultralight, tilde toggle) + JS↔C++ command bridge + debugger-contract CLI check + all 14 visual debugger modules migrated to IDebugDomain + DiaVisualDebuggerConsole retired. 8188 tests. |
| DiaUIUltralight — Game Input Bridge | [spec](specs/applications/dia/systems/diauiultralight/diauiultralight.md) | 2026-08-12 | CallJSFunction, keyboard injection, InputRouter mode stack. Unblocked DiaDebugDomain. |
| DiaSensorInspector | — | 2026-08-13 | Dropped — not wanted |
| Clang-Tidy analysis | CMake migration (compile_commands.json) | 2026-08-13 | Dropped — not wanted |
| TSan (ThreadSanitizer) | Linux target (WSL2 CI) | 2026-08-13 | Dropped — not wanted |
| Visual Debugger Domain Stats Tests | Add TDD RED stats-field assertions to all 13 domain test files (extend existing `*_JSONState` suites — do not create new files). Each test targets the specific `stats.xxx` fields each `debugger-impl.plan.md` task populates. Also add drawer-name assertions for tasks 13–15 (Scene2D split → 3 drawers, LightRangesDrawer, IK2D/Lighting3D label renames). Run gate: `dia run googletest --filter="*DebugDomain*_JSONState_Stats*"`. Add these after the impl work lands to avoid conflicts. Full plan: `.claude/plans/deep-stargazing-aurora.md`. | 2026-08-14 | 42 tests across 12 _JSONState_Stats suites (28 pass 1 + 14 pass 2); 366/366 green |
| Visual Debugger Panel Stats | debugger-impl.plan.md | 2026-08-14 | Stats fields wired for all 13 debug domains; 338/338 tests green. |
| diarulesvisualdebugger | [spec](specs/) | 2026-08-14 | Marked done by `dia docs spec-done` |
| diablackboardvisualdebugger | [spec](specs/) | 2026-08-14 | Marked done by `dia docs spec-done` |
| diaaibudgetvisualdebugger | [spec](specs/) | 2026-08-14 | Marked done by `dia docs spec-done` |
| diasteeringvisualdebugger | [spec](specs/) | 2026-08-14 | Marked done by `dia docs spec-done` |
| diapathfindingvisualdebugger | [spec](specs/) | 2026-08-14 | Marked done by `dia docs spec-done` |
| diastatemachinevisualdebugger | [spec](specs/) | 2026-08-14 | Marked done by `dia docs spec-done` |
| diahtnvisualdebugger | [spec](specs/) | 2026-08-14 | Marked done by `dia docs spec-done` |
| diamailboxvisualdebugger | [spec](specs/) | 2026-08-14 | Marked done by `dia docs spec-done` |
| diabehaviourtree | [spec](specs/) | 2026-08-15 | Marked done by `dia docs spec-done` |
| DiaBehaviourTreeVisualDebugger | [diabehaviourtreevisualdebugger.md](specs/applications/dia/systems/diabehaviourtreevisualdebugger/diabehaviourtreevisualdebugger.md) | 2026-08-16 | IDebugDomain + IBehaviourTreeEventListener dual-inheritance; 128-slot fixed ring buffer; GetJSONState (drawers, stats, lastTickNodes); OnCommand toggle; 6 GoogleTests pass. |
| diagridvisibility | [spec](specs/) | 2026-08-16 | Marked done by `dia docs spec-done` |
| diagridvisibilityvisualdebugger | [spec](specs/applications/dia/systems/diagridvisibilityvisualdebugger/diagridvisibilityvisualdebugger.md) | 2026-08-16 | GridVisibilityDebugDomain (template); CellState/SightRadii/ShadowcastBoundary drawers; GetJSONState + group selector; 23 GoogleTests; wired into GridVisibility test stage |
| GridVisibilityTestStage | [spec](specs/applications/cluichetest/systems/teststages/) | 2026-08-16 | Two-group fog-of-war E2E stage; Red/Blue patrol + 5 enemies; 24×18 grid with walls; shadowcasting + Revealed persistence + enemy flash; GridVisibilityDebugDomain wired |
| AICalloutTestStage | [plan](archive/plans/aicallout-test-stage.plan.md) | 2026-09-04 | All 9 tasks Done: 2-faction stage, 6 wandering emitters + 6 relay responders, emit/claim/release/TTL lifecycle, faction-coloured ring overlay, cross-faction violations metric == 0; `CalloutRegistryDebugger` IDebugDomain wired; pytest E2E scenario passes. |
| diasimtime | [spec](specs/) | 2026-09-02 | Marked done by `dia docs spec-done` |
| visual-debugger | [spec](specs/) | 2026-09-04 | Marked done by `dia docs spec-done` |
| DiaAttribute | System spec + all 6 feature specs Approved ✅: core, conditional-modifiers, change-notifications, accessor-bridge, visual-debugger, save-serialization. Data-driven gameplay attribute/stat framework — same idiom as DiaEconomy (StringCRC, JSON schema, DiaCondition-gated modifiers, Observer events), no shared code. Plan: [diaattribute.plan.md](specs/applications/dia/systems/diaattribute/diaattribute.plan.md) — 6 tasks, 3 phases. Entry point: task 1 (core). Task 1 has an open design question (component attachment mechanism) to resolve before `AttributeSetComponent`; task 4 (accessor-bridge) is the highest-uncertainty task — `DiaCondition`'s `ConditionRegistry` has no unregister method, may need an upstream fix. Deferred: Archetype/Instance Layering. Parked: Dependency-Graph Derived Attributes, Non-Numeric Typed Properties. Research: [gameplay_attribut_stat_system/summary.md](research/gameplay_attribut_stat_system/summary.md). | 2026-09-04 | All 6 features Done: Core, Conditional Modifiers, Change Notifications, Accessor Bridge, Save/Serialization, Visual Debugger. 124 GoogleTests pass. Full unfiltered regression: 8933/8933. |
| Architecture layer violation remediation | `dia check arch` flags ~35 violations across 6 independent root causes (VisualDebugger family → DiaVisualDebugger — the long-standing ~42-file conflict below, now audited; DiaScalarFieldInspector → DiaEditor; GridVisibilityVisualDebugger mistagged vs. its own domain; a duplicate RGBA type pulling RigidBody2DVisualDebugger into the visual domain; DiaSensor/DiaTriggerScript falsely attributed to `dia.root` via malformed module.md metadata; DiaSimTime → DiaSaveGame). Audit complete with a recommended fix and task breakdown per cluster: [arch_layer_violations/plan.md](refactors/arch_layer_violations/plan.md). One task (retagging `dia.editor` from `domain/visual/core` to `foundation/application`) is gated on explicit sign-off before implementation. | 2026-09-07 | dia check arch: 172 -> 0 violations. All 6 original clusters + expanded metadata sweep (task 12, 10 modules) fixed across 6 commits. Task 9 (DiaDebugDraw.vcxproj duplicate-compile cleanup) remains Blocked -- tracked as its own Loose Ends entry, not part of this line's Done state. |
| DiaDebugDraw.vcxproj duplicate-compile hack | `DiaDebugDraw.vcxproj` still duplicate-compiles `DebugLayerManager.cpp` et al. out of `DiaVisualDebugger` via relative path (unrelated to layer violations — `dia check arch` is clean). Tried removing the duplicate entries once (see [arch_layer_violations/plan.md](refactors/arch_layer_violations/plan.md) task 9): broke the real link for `GoogleTests.exe` with 59 unresolved externals, even though `DiaDebugDraw.lib` isn't referenced anywhere in `GoogleTests.vcxproj` (not in `ProjectReference`, not in `AdditionalDependencies`) while `DiaVisualDebugger.lib` — which should contain these exact symbols — is explicitly linked. Reverted immediately, confirmed clean. Working theory, unconfirmed: a shared intermediate-object-path collision between the two projects compiling the same source files under MSBuild's default parallel build. Needs real investigation (check `IntDir` settings, try a forced serial rebuild) before attempting again. | 2026-09-08 | Resolved 2026-09-08: root-caused as an unfinished extraction (DiaDebugDraw's own doc already claimed the shared registry/palette/dispatch as its responsibility, but the .cpp files never physically moved out of DiaVisualDebugger). Merged DiaDebugDraw into DiaVisualDebugger for real: moved the 4 shared files, added the missing ClCompile entries, fixed ~65 #include paths repo-wide, stripped 11 dead ProjectReferences, deleted the module. Verified via GoogleTests (435/435) and CluicheTest build. |
| diaconsole | [spec](specs/) | 2026-09-30 | Marked done by `dia docs spec-done` |
| Dia | F-1: DiaCLI `cow` target + `scaffold sln-add --root` (skeleton B1, B2) | 2026-10-09 | Done: [targets.cow] block + sln-add --root; commits cfc5824f, 7ce0b3b0 |
| Dia | F-1: DiaCLI data-driven target registry (scaffold-app task 1); makes the `cow` target toml-only | 2026-10-09 | Done: registry derives launch/deploy/error lists from pipeline.toml; scaffold-app task 1; commit 04f9aee5 |
| bootmenu-promotion | [spec](specs/) | 2026-10-09 | Marked done by `dia docs spec-done` |
| boot-choice | [spec](specs/) | 2026-10-09 | Marked done by `dia docs spec-done` |
| cow-app-skeleton (CoW app, generated by `dia scaffold app`) | [spec](specs/applications/cow/systems/f1-harness/cow-app-skeleton.md) | 2026-10-09 | Boots to the Boot menu, FrontEnd launch and return verified, F5 verified |
| DiaCLI `cow` target, `scaffold sln-add --root`, data-driven target registry | [spec](specs/applications/dia/systems/diacli/scaffold-app.md) | 2026-10-09 | Skeleton B1/B2 and scaffold-app task 1 |
| `dia scaffold app` | [spec](specs/applications/dia/systems/diacli/scaffold-app.md) | 2026-10-09 | Templates, command, baseline links; task 4 covered by generating CoW |
| `dia scaffold stage --app` | [plan](specs/applications/cow/systems/f1-harness/f1-harness.plan.md) | 2026-10-09 | F-1 task 6; 22 tests |
| `new-cow-stage` skill (written; trial pending) | [spec](specs/applications/cow/systems/f1-harness/new-cow-stage-skill.md) | 2026-10-09 | Trial run is a loose end in BACKLOG.md |
| scaffold-app | [spec](specs/) | 2026-10-09 | Marked done by `dia docs spec-done` |
| Dia | `dia docs spec-scaffold` writes the wrong parent and fails on a relative `--output-dir` | 2026-10-09 | Parent inference now uses real layout; added --app; relative --output-dir works; tests added |
| Dia | `dia docs vcxproj-add` picks the filter from the first path segment only | 2026-10-09 | Filter now reused from same-directory entries, then nearest ancestor, then first segment; tests added |
| CoW | F-1: Bus live verification | 2026-10-09 | Implemented as MessageBusStressTestStage in CluicheTest (commit f717ac07). Validates Primary/Reaction two-pass delivery with 10k messages at 1:1 ratio. CoW can reference this test for F-1 Test 4 verification instead of building a separate CoW-specific stage. |
| CoW / Dia | F-1: Bus traffic recorder | 2026-10-09 | Implemented as MessageBusStressTestStage in CluicheTest (commit f717ac07). Validates Primary/Reaction two-pass delivery with 10k messages at 1:1 ratio. CoW can reference this test for F-1 Test 4 verification instead of building a separate CoW-specific stage. |
| CoW | DiaMessageBus module doc: `status: stub` is stale | 2026-10-09 | Moved by `dia docs backlog move` |
| CoW | Shared application-bootstrap helper | 2026-10-09 | `Dia::ApplicationFlow::LoadManifest` + `RegisterPathAliases` (Manifest/ManifestBootstrap.h); both mains use it, loop/frame rate stay per-app |
| Dia | DiaMessageBus: bus-traffic-recorder | 2026-10-13 | `IBusRecorder` + `Bus::SetRecorder` hook, `FileBusRecorder` (format v1, injected tick source), test helpers, observation (3 counters, flush profile scope, `FileBusRecorderHealthReporter`). 37 tests; AC-10 relaxed to a runtime assert (spec amended). Commits 2cc53024..17a3aa47. CoW wiring (path, tick source, flush/close on exit) remains a separate CoW task |
| command-unregister | [spec](specs/) | 2026-10-13 | Marked done by `dia docs spec-done` |
| panel-palette | [spec](specs/) | 2026-10-13 | Marked done by `dia docs spec-done` |
| panel-console | [spec](specs/applications/dia/systems/diadebugdomain/panel-console.md) | 2026-10-13 | Console strip in DiaDebugPanel: parser, results, history, autocomplete, log view, PanelController, golden contract tests. Replaced the old DiaDebugConsole/ModuleDebugRegistry/DiaDebugDataPanel plan (the last two superseded). User-verified in CluicheTest. Also fixed along the way: ASan link and runner, StringCRC / String<N> over-reads, Application metric pointers |
| f1-harness | [spec](specs/applications/cow/systems/f1-harness/f1-harness.md) | 2026-10-13 | CoW boots to a Boot menu with a stub front end; baseline BootMenuModule; `dia scaffold stage --app` and `dia scaffold app`; debug panel host (console, Ctrl+P palette) wired into CoW; message bus in the manifest (baseline MessageBusHostModule + CoW BusProbeModule); bus traffic recorder wired (BusRecorderModule, Test 6 verified from the file after exit). Follow-ups left in BACKLOG Loose Ends |
| bus-recorder-debug-card | [spec](specs/applications/dia/systems/diamessagebus/visual-debugger.md) | 2026-10-13 | Recorder section in the MessageBus debug card's Live tab (AC-9): attached/state/path/records/bytes/dropped via `IBusRecorder::GetStatus` and `Bus::GetRecorder()`; 4 tests. Also closed the F-1 observation gaps (StageStatus table-full log + counter, BootMenu empty-stage warning, bus host domain logs, BusProbe counters) |
| Dia | 3 tests fail only in the whole-suite ASan run | 2026-10-13 | Not reproduced 2026-10-13: the 3 tests passed in two consecutive full ASan runs; dia check sanitizer exits 0 (9192 tests, no findings). No code change. If they recur, first check for a stale Debug-Asan build (touch the source and rebuild) and the relative Cluiche/out temp paths with GetTickCount(). |
| Dia | `--shards` runs under Debug-Asan still cannot find the ASan runtime DLL | 2026-10-13 | Fixed 2026-10-13 (6d4262d0): shard_runner builds each shard's env with with_asan_runtime_on_path (config threaded from googletest_runner); unit test added; verified with dia launch googletest --config Asan --shards 2. |
| Dia | `dia validate manifest` misses ServiceStream orphan providers | 2026-10-13 | Fixed 2026-10-13: dia validate manifest composes each .diagame with its imports and checks ServiceStream provider/consumer constraints (mirrors C++); LoadManifest labels warnings as warnings |
| CoW | E1 Save/Load | 2026-10-14 | Built 2026-10-14. SaveLoadModule shell + participants game_speed, random_streams, DiaSimTime; atomic saves, 10 MB buffer, slot-id validation, SaveLoadHealthReporter, migration pre-validation. Dia fixes: RandomStream Restore, TimeServer::SetTime/SetTimeScaleImmediate, SimTimeScheduler::Clear, SlotManager id fix, manifest timestamp. Full suite green. |
| CoW | GameSpeedController (A2) | 2026-10-14 | Built 2026-10-13; save participant delivered with E1 on 2026-10-14. |
| CoW | E1 follow-ups | 2026-10-14 | Moved by `dia docs backlog move` |
| CoW: F-2 Core Runtime | [spec](specs/applications/cow/systems/f2-core-runtime/f2-core-runtime.md) | 2026-10-14 | Done. One Save/Reload/Replay scenario with checks C1-C6 and debug panel; found and fixed the GameSpeedController restore gap (participant v2). Hatch path of C6 waits on B6 |
| Dia | DiaPathfinding: Rectangular Hex Grid | 2026-10-14 | Built 2026-10-14: HexTopology, rectangular odd-r HexPathGrid, layout-aware heuristic; 9417 tests pass |

---

## Completed Standalone Features (continued — from backlog cleanup)

| Feature | System | Completed | Notes |
|---------|--------|-----------|-------|
| AI Personality | DiaUtilityAI | 2026-08-05 | Moved from backlog — already implemented |
| E2E suite: RigidBody2DTestStage / SoftBody2DTestStage checkpoint failures | TestStages | 2026-08-05 | Doubled frame budgets (900→1800, 600→1200) and pytest timeouts (15s→100s, 14s→70s) to match ~50ms Debug SimPU frame rate |

---

## Completed Standalone Features

| Feature | Spec | System | Completed |
|---------|------|--------|-----------|
| Multi-Stream RNG | [multi-stream-rng.md](specs/applications/dia/systems/diamaths/multi-stream-rng.md) | DiaMaths | 2026-10-09 — `RandomStream` class + `RandomStreamManager` instance-based registry for deterministic multi-system RNG. 8 independent named streams (WLD, A1, B4, B6, C1, C2, B5, C4) for CoW F-2. StringCRC-keyed, no singleton, per-stream mutex, auto-grows capacity. 26 comprehensive tests: stream independence, determinism, thread safety (concurrent access + cross-thread), edge cases (seed=0/UINT_MAX, capacity limits), pointer stability, backward compatibility. Serialization deferred. |
| Animation2DTestStage | animation2d-test-stage | TestStages | 2026-06-01 — Dragon skeleton clip playback, pose validation, blend weight interpolation; `animation.clip_complete` + `animation.pose_matches` checkpoints; pytest scenario passes. |
| EntityTestStage | [entity-test-stage.md](specs/applications/cluichetest/systems/teststages/entity-test-stage.md) | TestStages | 2026-06-01 — Spawn/destroy/hierarchy/query/mailbox/lifecycle validation; `TransformComponent` + `VisualTestRenderComponent`; 6 checkpoints; EntityModule (SimPU, reusable) + EntityTestModule (MainPU, test logic). |
| Scene2DTestStage | [scene2d-stage.md](specs/applications/cluichetest/systems/teststages/scene2d-stage.md) | TestStages | 2026-06-01 — Scene loading, camera/light/entity hydration, layer resolution; 5 checkpoints; pytest scenario passes. |
| component-readonly-flag | component-readonly-flag | diaentitytemplate | 2026-06-01 — `DIA_READONLY` + `DIA_WRITES(Target)` macros, Domain enforcement (skip hooks for readonly, single-writer assert on add/remove). Enshrines SD-ENT-022. 17 GoogleTests pass. |
| RigidBody2DStage | [rigidbody2d-stage.md](specs/applications/cluichetest/systems/teststages/rigidbody2d-stage.md) | TestStages | 2026-05-28 — 10 circles + static ground; `rigid_body.all_settled` checkpoint; pytest scenario passes; `TestStageModuleBase` extracted as shared base class; pattern proven for future stages. |
| module-metadata | [module-metadata.md](specs/applications/dia/systems/diaapplicationflow/module-metadata.md) | DiaApplicationFlow | 2026-05-28 — `PUAffinity` bitmask; `TypeRegistry` stores `kAllowedPUs`+`kDescription`; `AddModule()` asserts on mismatch; `IApplicationInspectable` extended; 24 modules annotated; 14 tests. |
| shared-debug-console | [shared-debug-console.md](specs/applications/dia/systems/diavisualdebugger/shared-debug-console.md) | DiaVisualDebugger | 2026-05-28 — `VisualDebuggerModule` + `VisualDebuggerConsoleModule` always-active; layers persist across transitions with stageTag; per-stage tab bar in console; 7 tests. |
| stage-scaffold-simplification | [stage-scaffold-simplification.plan.md](specs/applications/cluichetest/systems/teststages/stage-scaffold-simplification.plan.md) | TestStages | 2026-05-28 — 8→4 touch points; FrameStream auto-flush with fire-once warn + trace zone; pipeline auto-derives stages from catalogue; Boot transitions auto-derived from stages[]; staleness fix; `/new-cluichetest-stage` skill updated. |
| replace-diasfml-with-sdl3 | [replace-diasfml-with-sdl3.md](specs/applications/dia/systems/diasdl/replace-diasfml-with-sdl3.md) | DiaSDL (new) | 2026-05-28 — DiaSDL module; SDL3 submodule at External/SDL3/; Win32WndProcChain moved to DiaBgfx; DiaSFML fully deleted; KernelModule swapped to DiaSDL; 18+ GoogleTests pass. |
| EntityModule (CluicheTest) | [entity-module.md](specs/applications/cluichetest/systems/applicationflow/entity-module.md) | CluicheTest ApplicationFlow | 2026-05-24 — `EntityModule : Module` in SimPU; owns `Dia::Entity::Domain`; registers Hierarchy pools on DoStart; drives `Update + EndOfFrame` each tick; exposes `IEntityInspectable&` for future editor wiring. Registered in DummyStage. |
| DiaApplicationFlowEditor — Stages Tab | [stages-tab.md](specs/applications/dia/systems/diaapplicationfloweditor/stages-tab.md) | DiaApplicationFlowEditor | 2026-05-20 — SVG transition graph (explicit `transitions[]` per stage, forward `<line>` + back-edge bezier `<path>`), live overlay (pulse ring on active stage, dash-flow on auto-advance edges), click-to-transition, first tab + default view in AppV2; 19 StagesTab tests + 4 AppV2 tests. Manifest v3 schema adopted (per-stage `transitions[]` + `autoAdvance`, `autoStages` removed). |
| DiaApplicationFlowEditor — Validation Fix Suggestions | [validation-fix-suggestions.md](specs/applications/dia/systems/diaapplicationfloweditor/validation-fix-suggestions.md) | DiaApplicationFlowEditor | 2026-05-20 — `SuggestedCommand` + `IssueTarget` on `ValidationIssue`; click-to-navigate in ValidationBar; one-click Fix dispatches command and re-validates; 12 ValidationBar tests. |
| Asset Lifecycle Management | [asset-lifecycle-management.md](specs/applications/dia/systems/diaassetruntime/asset-lifecycle-management.md) | DiaAssetRuntime | 2026-05-19 — Full state machine (Null→Staged→Loading→Loaded/Failed→Unloaded); `IAssetTypeHandler`/`IAssetLoadCallback`; handler registry by type prefix; auto-validation for handler-less types; `TextureHandler` + `UIHandler` real I/O; `TextureManager::LoadTexture` internalized; deferred: ShaderHandler (T15), AudioHandler (T16), Ultralight internalization (T20), shader internalization (T21), tests (T12). |
| Project Context Bar | [project-context-bar.md](specs/applications/dia/systems/diaeditor/project-context-bar.md) | DiaEditor | 2026-05-18 — `IEditorContext::LoadProject/GetProject/OnProjectChanged`; `ProjectContext` struct; toolbar project button (name/No project, dropdown with Open/Recent/Reveal/Close); `--project` CLI arg; live connection auto-load from `get_app_state`; `.cluicheproj` recent-projects persistence; all 4 existing plugins migrated to shared `ProjectContext`; `config.assetCatalogue` added to `DiaGameConfig` + serializer. |
| DiaApplicationFlow — Stream Tap & Debug Iteration | [stream-tap.md](specs/applications/dia/systems/diaapplicationflow/stream-tap.md) | DiaApplicationFlow | 2026-05-18 — Type-erased AttachTap/DetachTap/GetTapCount on IStreamStore + EventStreamStore; TapEvent/TapCallback/TapHandle; StreamInfo extended (payloadType, overflowPolicy, currentSequence, attachedReaderCount, attachedTapCount); StreamTypeRegistry serializer registration + SerializeToJson; DIA_STREAM_TYPE_WITH_SERIALIZER macro; DebugServer SubscriptionManager deleted; DebugServer migrated to tap-based subscribe/unsubscribe/connection-close; $lifecycle tap consumer; mock tap + count + stress + lifecycle integration tests. |
| data-driven-application-system | [data-driven-application-system.md](specs/applications/dia/systems/diaapplication/data-driven-application-system.md) | DiaApplicationFlow | 2026-05-02 — JsonApplicationManifestSerializer + 12 tests |
| DiaMessageBus | [diamessagebus.md](specs/applications/dia/systems/diamessagebus/diamessagebus.md) | Dia | 2026-08-21 — Declaration-first message routing: `.diagamemessages` IDL + `dia codegen messages` → structs/Handlers/RegisterMessages wiring; `Bus` (BroadcastRouter + EntityRouter registration point, two-pass Primary/Reaction flush, last-tick ledger); `LedgerHistory` ring buffer; `MessageBusDebugDomain` visual debugger; `IFlushAdapter` (Physics/Input); Schema Browser CluicheEditor plugin; 8 Phase 8 consumer adapters (Economy, AICallout, AssetRuntime, Animation2D, EntitySpawner, BehaviourTree, Order, EntityRouter-bus-wiring). 21 plan tasks, all Done. 3 real bugs found+fixed during implementation: Reaction-pass delivery order-dependence, codegen kTypeId/include-path, Bus::kEntityRouterId value mismatch vs the real EntityRouter. Also fixed an unrelated pre-existing DiaGridVisibility link failure blocking all of CluicheTest. |

---

## Completed Standalone Features (continued)

| Feature | Spec | System | Completed |
|---------|------|--------|-----------|
| DiaCore — DirectedGraph + RelationshipIndex refactor | [directed-graph.md](specs/applications/dia/systems/diacore/directed-graph.md) | DiaCore | 2026-05-05 — `DirectedGraph<..., IDType>` with 3 policies + 7th IDType template param; RelationshipIndex rebuilt on DirectedGraph with CRC keys (~47KB stack); 78 graph tests + 92 asset catalogue tests passing |
| DiaMetrics Registry (standalone) | [metrics-registry.md](specs/applications/dia/systems/diaobservation/metrics-registry.md) · [diametrics.md](specs/applications/dia/systems/diametrics/diametrics.md) | DiaMetrics | 2026-05-18 — New `Dia/DiaMetrics/` static lib: `Counter` (per-thread lock-free shards), `Gauge` (atomic\<double\>), `Histogram` (fixed buckets, p50/p95/p99), `MetricRegistry` Meyer's singleton, `IMetricSink`, `MetricSnapshot`. 24 tests. `MetricsCollectorModule` rewritten to v2 Module API, registers 4 engine gauges. Tasks 11–13 (MetricsFileSink, SessionManager wiring) deferred to DiaObservation #5. |
| DiaObservation #1 — Skeleton + DiaLogger Fold + Async Drain | [skeleton-and-logger-fold.md](specs/applications/dia/systems/diaobservation/skeleton-and-logger-fold.md) | DiaObservation | 2026-05-18 — Created `Dia/DiaObservation/` module; folded all DiaLogger source into `Log/` subsystem (git mv, blame preserved); hard-switched namespace `Dia::Logger::` → `Dia::Observation::Log::` across 107 callers; deleted `Dia/DiaLogger/`; added async drain thread (lazy-start via `std::call_once` on first `RegisterSink`, 1ms loop, `Stop()` joins on shutdown); `LoggerModule::DoStop` calls `Stop()` before `UnregisterSink` to avoid use-after-free; added `MockSink.h` in `Testing/`; 5 new async drain tests (AC14/AC17/AC18/AC20/AC22); all 3 pipelines (googletest + cluichetest + cluicheeditor) green. |

---

## Completed E2E Stages

| Item | Completed | Notes |
|------|-----------|-------|
| AIDecisionTestStage + scenario | 2026-07-31 | 5 checkpoints: Condition eval (health<50, enemy.visible), Rules (CallForHelp fires), UtilityAI sync (Flee wins at health=30), AIBudget async (EvaluateAsync callback). `scenarios/cluichetest/ai_decision/test_ai_decision.py`. BlackboardComponent + ConditionRegistry inline bridge + RuleSetComponent + UtilitySetComponent + AIBudgetScheduler. |
| AIHTNTestStage + scenario | 2026-07-31 | 6 checkpoints: sync plan built, plan complete, diverged+replanned after health mutation, RuleActionBridge operator fired, async plan completed, async plan correct. `scenarios/cluichetest/ai_htn/test_ai_htn.py`. Phase state machine (7 phases) driving HTNPlannerComponent + HTNPlanner direct async path. |
| BehaviourTreeTestStage | 2026-08-16 | 3-guard patrol/alert/chase loop; shared BT asset + independent blackboards; all 5 node types; DiaOrder (`GuardMoveOrder`); debugger panel auto-shown; 5 checkpoints; 10 tasks. |
| MessageBusTestStage | 2026-08-21 | 5 Emitters + 5 Receivers; BroadcastRouter fan-out, real EntityRouter targeted delivery (first stage to register it on its own Bus), Reaction-pass PongEvent chaining, BurstAdapter (IFlushAdapter) injection at frame 150, MessageBusDebugDomain wired via a TestableMessageBusModule wrapper. 5 checkpoints + dropped==0 + determinism. `scenarios/cluichetest/msgbus/test_msgbus_e2e.py`. |

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

## CoW: Designed in the GDD (struck from "CoW: Features to Be Designed")

| Testbed / piece | Locked | Notes |
|-----------------|--------|-------|
| S-3 Living Ecology | 2026-10-11 | `S3_LivingEcology.md`; updated to include A4 weather overlays |
| S-2 Single Cell Ecology | 2026-10-11 | `S2_SingleCellEcology.md` |
| V-1 Tile Rendering | 2026-10-11 | `V1_TileRendering.md`; 5-layer rendering + A4 weather |
| S-1 World Generation | 2026-10-11 | `S1_WorldGeneration.md` |
| F-2 Core Runtime | 2026-10-09 | `F2_CoreRuntime.md` |
| F-1 Boot & Debug Menu · Event Bus & Inspector | 2026-10-09 | `F1_Harness.md`; Record & Replay still open |

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
| DiaEnv — env-export | [diaenv.md](specs/applications/dia/systems/diaenv/diaenv.md) | Covered by deps.json + mirrors + submodules |
| DiaEnv — docker continuous monitoring (`dia env doctor`) | [diaenv.md](specs/applications/dia/systems/diaenv/diaenv.md) | Premature until setup + verify stable (both now Done) |

---

## Dropped

| Item | Project | Dropped | Why |
|------|---------|---------|-----|
| Shipping-build gating (compile the menu out, shipping manifest) | CoW | 2026-10-09 | User decision; revisit when a shipping build is planned |
| `--play` command-line flag and DiaCLI argument forwarding | CoW | 2026-10-09 | Replaced by a Play entry in the Boot menu; DiaCLI has no argument forwarding |
