# Cluiche Backlog History

Completed items moved from BACKLOG.md. For active work see [BACKLOG.md](BACKLOG.md).

---

## Completed Tooling

| Item | Completed | Notes |
|------|-----------|-------|
| CI Architecture Gates | 2026-07-09 | `dia check deps` exit code fixed, `dia check arch` wired, `dia check clones` (PMD CPD) added, `.github/workflows/ci.yml` with hard-gate + advisory jobs. |

---

## Completed Systems

| System | Spec | Completed | Notes |
|--------|------|-----------|-------|
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
| DiaEnv — env-export | [diaenv.md](specs/applications/dia/systems/diaenv/diaenv.md) | Covered by deps.json + mirrors + submodules |
| DiaEnv — docker continuous monitoring (`dia env doctor`) | [diaenv.md](specs/applications/dia/systems/diaenv/diaenv.md) | Premature until setup + verify stable (both now Done) |
