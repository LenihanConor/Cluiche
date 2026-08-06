# Application Spec: Dia

## Parent Platform
@docs/specs/platform/Cluiche.md

## Purpose

Dia is the game engine application that provides all shared engine infrastructure for the Cluiche platform. While organized as an "application" in the spec hierarchy, Dia functionally serves as the shared codebase that all other applications (games, tools, tests) depend on. It provides core systems (DiaCore, DiaMaths, DiaGraphics, DiaInput, etc.), runtime frameworks (DiaApplicationFlow), and build tooling (DiaAPI) that enable rapid game development with a component-based architecture, multi-threaded execution model, and modular design.

## Systems

| System | Description | Spec |
|--------|-------------|------|
| DiaAPI | C++ command registration and execution API framework - provides runtime command infrastructure | [diaapi.md](systems/diaapi/diaapi.md) |
| DiaCLI | Python-based CLI tool for development workflows (build, assets, scaffolding, utilities) | [diacli.md](systems/diacli/diacli.md) |
| DiaPython | Python embedding framework - wraps pybind11 with clean C++ API for scripting integration | [diapython.md](systems/diapython/diapython.md) |
| DiaApplicationFlow (v1) | Application framework (ProcessingUnit/Phase/Module) — SUPERSEDED | [diaapplication.md](systems/diaapplication/diaapplication.md) |
| DiaApplicationFlow | Config-driven application framework (PU/Stage/Module with unified streams) | [diaapplicationflow.md](systems/diaapplicationflow/diaapplicationflow.md) |
| DiaGame | Game project system — owns .diagame/.diastage file formats, serializers, GameFileComposer, GameLoader | [diagame.md](systems/diagame/diagame.md) |
| DiaApplicationEditor (v1) | Editor plugin for .diaapp manifests — SUPERSEDED | [diaapplicationeditor.md](systems/diaapplicationeditor/diaapplicationeditor.md) |
| DiaApplicationFlowEditor | Visual .diaapp v2 editor with live runtime inspection (PU graph, presence grid, streams) | [diaapplicationfloweditor.md](systems/diaapplicationfloweditor/diaapplicationfloweditor.md) |
| DiaDebugProtocol | Shared header-only protocol types for editor-game communication (used by DiaEditor and DiaDebugServer) | [diadebugprotocol.md](systems/diadebugprotocol/diadebugprotocol.md) |
| DiaDebugServer | WebSocket server for remote debugging - broadcasts game state, forwards DiaAPI commands to editors | [diadebugserver.md](systems/diadebugserver/diadebugserver.md) |
| DiaEditor | Editor framework system (MVC, plugin system, CEF UI, command integration, live WebSocket connection) | [diaeditor.md](systems/diaeditor/diaeditor.md) |
| DiaUICEF | CEF-based UI system implementing IUISystem | [diauicef.md](systems/diauicef/diauicef.md) |
| DiaUIUltralight | Ultralight-based IUISystem implementation | [diauiultralight.md](systems/diauiultralight/diauiultralight.md) |
| DiaWebSocket | WebSocket server/client abstraction wrapping websocketpp (used by DiaDebugServer, DiaEditor, future networking) | [diawebsocket.md](systems/diawebsocket/diawebsocket.md) |
| DiaLogger | Engine-wide logging system — SUPERSEDED by DiaObservation (folded into `Dia/DiaObservation/Log/`) | [dialogger.md](systems/dialogger/dialogger.md) (Superseded) |
| DiaMetrics | General-purpose metric primitive library — `Counter`/`Gauge`/`Histogram`/`MetricRegistry`; `DiaCore`-only dependency; usable from any system for profiling, gameplay stats, or editor live panels without observation overhead. | [diametrics.md](systems/diametrics/diametrics.md) |
| DiaObservation | Engine-wide observability system covering logs, traces, and health — per-session directory at `Cluiche/out/<App>/sessions/<id>/`, OTel wire-compatible, designed as the substrate for a future `DiaE2E` test framework. Folds in DiaLogger. Depends on DiaMetrics for metric file output. | [diaobservation.md](systems/diaobservation/diaobservation.md) |
| DiaGeometry2D | 2D geometry system: shape primitives, intersection tests, Transform, spatial structures (Grid, Quadtree, BVH) | [diageometry2d.md](systems/diageometry2d/diageometry2d.md) |
| DiaGeometry3D | 3D geometry system: shape primitives (AABB, OOBB, Sphere, Capsule, Triangle, Cylinder, Ray, Plane, Frustum), intersection tests, ISpatialStructure3D + SpatialGrid3D | [diageometry3d.md](systems/diageometry3d/diageometry3d.md) |
| DiaGeometryBridge | Cross-dimension shape conversions: project (3D→2D) and lift (2D→3D) helpers in `Dia::GeometryBridge::` namespace | [diageometrybridge.md](systems/diageometrybridge/diageometrybridge.md) |
| DiaRigidBody2D | 2D rigid body physics: velocity/force integration, collision detection + response, constraints/joints, sleeping, collision layers, Physics-channel logging | [diarigidbody2d.md](systems/diarigidbody2d/diarigidbody2d.md) |
| DiaRigidBody2DVisualDebugger | Read-only debug visualization of rigid body physics state — superseded by DiaVisualDebugger system | [diarigidbody2dvisualdebugger.md](systems/diarigidbody2dvisualdebugger/diarigidbody2dvisualdebugger.md) (Superseded) |
| DiaSoftBody2D | 2D soft body simulation: PBD ropes, cloth, particle-geometry collision, rigid body coupling, Physics-channel logging, visual debugger | [diasoftbody2d.md](systems/diasoftbody2d/diasoftbody2d.md) |
| DiaEnv | Portable development environment system — SDK manifest, toolchain manifest, `dia env setup/verify`, MSBuild auto-restore, git submodule migration, AI context hardening | [diaenv.md](systems/diaenv/diaenv.md) |
| DiaTest | Test execution system — `dia test cli` (pytest for DiaCLI), `dia test env-integration` (agentic env→pipeline→test loop), future `dia test googletest` and `dia test ui` | [diatest.md](systems/diatest/diatest.md) |
| DiaTestHarness | External Python/pytest e2e orchestration — JSON scenarios, WebSocket to DiaDebugServer, crash detection, structured results — SUPERSEDED by DiaAutomation + dia orchestrate | [diatestharness.md](systems/diatestharness/diatestharness.md) |
| DiaAutomation | Pure capability layer for external automation — checkpoint registry, pause/resume, navigation hold, CI safety; `dia.automation.*` commands via DiaAPI JSON path | [diaautomation.md](systems/diaautomation/diaautomation.md) |
| DiaPipeline | Multi-stage build pipeline — `dia pipeline` command surface; proto-compile, compile-code, asset-build, package stages; `pipeline.toml` config; host or Docker execution | [diapipeline.md](systems/diapipeline/diapipeline.md) |
| DiaPipelineEditor | Live pipeline viewer + trigger panel inside CluicheEditor — tails NDJSON log, stage timeline with drill-down, build triggering, last 10 run history | [diapipelineeditor.md](systems/diapipelineeditor/diapipelineeditor.md) |
| DiaEditorUI | Shared React component library for all editor plugin UIs — VS Code dark theme, TrafficLightDot, TabBar, EmptyState, ConnectionStatus, useBridge hook, Toast system | [diaeditorui.md](systems/diaeditorui/diaeditorui.md) |
| DiaData | Foundational data model — asset identity (StringCRC), type framework, central registry with bidirectional relationships, JSON definition loader | [diadata.md](systems/diadata/diadata.md) |
| DiaAssetPipeline | Build-time asset pipeline — discover, validate, transform, deploy raw assets to built output; NDJSON logging; DiaAPI commands | TBD |
| DiaStageLoader | Runtime asset loading — Stage-based loading, Bundle resolution, typed asset access from built output | TBD |
| DiaAssetBrowserEditor | Editor UI for asset browsing, inspection, relationship graph navigation, validation status display | TBD |
| DiaSerializer | Shared serialization primitives — `MetadataValue`/`MetadataArray`, `SerializeResult`, `ISerializer` interface (version, migration query, file I/O helpers), `JsonMetadataHelpers`. Foundation for all domain serializers. | [diaserializer.md](systems/diaserializer/diaserializer.md) |
| DiaMailbox | Generic typed deferred messaging primitive — `Mailbox` with per-type fixed-capacity rings, opaque `Address`, pluggable `IMailboxRouter` system (domain-specific resolution lives in domain modules). Foundation for diaentitytemplate messaging and any future module-to-module deferred-event traffic. | [diamailbox.md](systems/diamailbox/diamailbox.md) |
| diaentitytemplate | Layered ECS where systems own their data and entities are gameplay glue — `Realm` container, generational `Entity` handles, reflection-backed components via `DIA_COMPONENT`/`FIELD` macros, JSON blueprint loading, typed entity refs, parent/child hierarchy, end-of-frame mutation pipeline, query system, entity router for DiaMailbox addressing, `IEntityInspectable` for editor. Replaces the old IComponent/IComponentObject infrastructure (PD-003/AD-005 pending Supersede). | [diaentitytemplate](systems/diaentity/diaentity.md) |
| DiaStateMachine | Generic state machine library — dual flat FSM + hierarchical state machine, pushdown automaton, component wrapper, transition logging/tracing, shared inspection interface | [diastatemachine.md](systems/diastatemachine/diastatemachine.md) |
| DiaBlackboard | Named typed-blob store — per-entity and global blackboards, `Register<T>`/`Get<T>`/`TryGet<T>` slot API, StringCRC keys, Observer lifecycle events (register/unregister), `BlackboardComponent` entity integration | [diablackboard.md](systems/diablackboard/diablackboard.md) |
| DiaBlackboardInspector | Live CluicheEditor panel showing all registered blackboards, slot keys + opt-in field values, and observer names via WebSocket + `BlackboardRegistry` | [diablackboardinspector.md](systems/diablackboardinspector/diablackboardinspector.md) |
| DiaOrder | Order queue and action system — `IOrder<TContext>` (Start/Update/Finish/Cancel), FIFO `OrderQueue` with `EnqueueFront` for urgent orders, Observer lifecycle events, `OrderQueueComponent` entity integration, `OrderMessage` for cross-PU posting via DiaMailbox | [diaorder.md](systems/diaorder/diaorder.md) |
| DiaPathfinding | Grid A* pathfinding — `CPathGraph` concept (zero-overhead static polymorphism), `SquarePathGrid` + `HexPathGrid`, injectable `IPathCostProvider`, sync `FindPath<TGraph>()`, async `PathfindingSystem` with time-sliced `Update(budgetMs)` and `IPathResultObserver` | [diapathfinding.md](systems/diapathfinding/diapathfinding.md) |
| DiaFlowField | Vector-field navigation for mass-unit movement — `CFlowFieldGraph` concept, sync `ComputeFlowField<TGraph>()` (Dijkstra sweep from goal), `FlowField` per-cell direction array, `FlowFieldCache` with named dirty-flag invalidation; square + hex support | [diaflowfield.md](systems/diaflowfield/diaflowfield.md) |
| DiaScalarField | Topology-agnostic grid-sized float field primitive — templated `<Topology, Policy>` (zero-cost hot path), UniformDecayPolicy, blocked cell mask, static modifier map, double-buffering, write shapes (point/radial/box), gradient query, spatial queries, multi-field weighted combination, optional DiaRules and DiaVisualDebugger adaptors | [diascalarfield.md](systems/diascalarfield/diascalarfield.md) |
| DiaEntitySpatial | Thin adapter that indexes entities by position using `ISpatialStructure<Entity>` from DiaGeometry2D — `SpatialComponent` opt-in, per-domain `EntitySpatialModule`, five query shapes (circle, region, knearest, ray, sector), bitmask layer filtering | [diaentityspatial.md](systems/diaentityspatial/diaentityspatial.md) |
| DiaEntitySpatialVisualDebugger | Runtime debug overlay for `DiaEntitySpatial` — grid cell outlines, per-entity circles colour-coded by layer mask, and last-query shape + hit highlight; three independently-togglable `IVisualDebugger` implementations | [diaentityspatialvisualdebugger.md](systems/diaentityspatialvisualdebugger/diaentityspatialvisualdebugger.md) |
| DiaSteering | Local movement behaviour system — stateless free-function behaviours (Seek, Flee, Arrive, Wander, Pursue, Evade, Obstacle Avoidance, Separation), `SteeringPipeline` (priority-group composition + weighted blend within group), `SteeringSystem` (agent registry + output cache); desired velocity output only, caller integrates position | [diasteering.md](systems/diasteering/diasteering.md) |
| DiaAIBudget | Frame-budget AI scheduler — `AIBudgetScheduler` (priority-ordered work queue, microsecond budget flush, Critical/Normal/Background tiers), `AIBudgetModule` (IModule wrapper on SimPU), DiaMetrics counters for budget utilisation | [diaaibudget.md](systems/diaaibudget/diaaibudget.md) |
| DiaCondition | Shared expression evaluator — `ConditionRegistry` (float/bool accessor registration by slot+field StringCRC pairs), `ConditionExpr` (JSON-loadable boolean expression tree), `ConditionGuardAdapter` (wires expressions into DiaStateMachine `CallbackRegistry` with zero FSM changes) | [diacondition.md](systems/diacondition/diacondition.md) |
| DiaRules | Forward-chaining rule engine — `RuleActionRegistry` (open handler registration by StringCRC), `RuleSet` (all-matching condition→action evaluation), JSON loader, `RuleSetComponent` entity integration | [diarules.md](systems/diarules/diarules.md) |
| DiaUtilityAI | Score-based action selection — `ResponseCurve` (easing-shaped float scorer), `ActionDef` (prerequisites + scorers + cooldown + max_concurrent), `GroupConsiderationContext` (shared frame pick-count for squad coordination), `UtilitySet` (sync + async AIBudgetScheduler evaluation), DiaVisualDebugger score overlay | [diautilityai.md](systems/diautilityai/diautilityai.md) |
| DiaHTN | Hierarchical Task Network planner — `HTNDomain` (JSON compound+primitive task definitions), `HTNPlanner` (stateless depth-first forward-chaining), `HTNPlan` (flat operator sequence with divergence detection), `OperatorRegistry` (multi-tick TaskResult callbacks), `RegisterRuleActionAsOperator()` bridge adapter, `HTNPlannerComponent` entity integration | [diahtn.md](systems/diahtn/diahtn.md) |
| DiaSensor | Entity perception framework — sight/proximity/damage/sound sensors writing to SensorResultsComponent, distilled to blackboard by module-driven adapter | [diasensor.md](systems/diasensor/diasensor.md) |
| DiaAICallout | Exclusive-claim coordination registry — entities post typed callouts (position, radius, faction, TTL); eligible entities claim one exclusively; DiaRules/DiaCondition in game code decides who claims what | [diaaibroadcast.md](systems/diaaibroadcast/diaaibroadcast.md) |
| DiaEconomy | Data-driven resource economy — `EconomySchema` (JSON asset: resource definitions, income rules, cost tables, modifiers), `EconomyInstance` (per-participant runtime pool state), transaction + transfer API, Observer events on SimPU, conditional modifiers via DiaCondition | [diaeconomy.md](systems/diaeconomy/diaeconomy.md) |
| DiaEconomyInspector | Live CluicheEditor panel for per-faction economy inspection — Resources tab (pool bars, sparklines, saturation/starvation badges), Events tab (rolling Earn/Spend/Clamped/Transfer log), Modifiers tab (active modifier stack), Schema tab (read-only cost table). Editor accumulates full session history after connect; server pre-buffers history before connect. | [diaeconomyinspector.md](systems/diaeconomyinspector/diaeconomyinspector.md) |
| DiaRig2D | 2D skeletal rig system — bone hierarchy, forward kinematics, pose representation/blending, JSON skeleton definitions, skeleton component, debug renderer | [diarig2d.md](systems/diarig2d/diarig2d.md) |
| DiaIK2D | Inverse kinematics — analytic two-bone solver, FABRIK N-joint solver, look-at constraint; post-process pass on DiaRig2D skeletons | [diaik2d.md](systems/diaik2d/diaik2d.md) |
| DiaAnimation2D | Animation playback and blending — damped spring chains, keyframe clip player, pose blend stack, procedural locomotion oscillator (deferred) | [diaanimation2d.md](systems/diaanimation2d/diaanimation2d.md) |
| DiaVisualDebugger | Visual debug rendering system — `DebugLayerManager`, stack of focused draw classes, `DebugColourPalette`, `DiaVisualDebuggerConsole` (ImGui), editor layer panel, extensions to DiaGraphics (budget, TextPrimitive) | [diavisualdebugger.md](systems/diavisualdebugger/diavisualdebugger.md) |
| RenderBackend | Concrete renderer behind `Graphics::ICanvas` — replaces SFML render path with bgfx in two phases (parity, then light 3D); new `DiaBgfx` module + Phase 2 3D module family (Mesh3D/Rig3D/Animation3D/Skinning3D/Scene3D) | [render-backend.md](systems/render-backend/render-backend.md) |
| DiaBgfx3D | Phase 2 3D rendering layer — `Canvas3D`, `MeshRenderer`, `SkinnedMeshRenderer`, `ShadowRenderer`, `MaterialRegistry`, `MeshGpuCache`, 3D shaders; separate module so 2D-only games never pull in the DiaScene3D chain | [diabgfx3d.md](systems/diabgfx3d/diabgfx3d.md) |
| DiaMesh3D | 3D mesh asset library — `Vertex3D`, `Submesh`, `Mesh3DAsset`, `Mesh3DAssetHandler` (IAssetTypeHandler for cooked `.mesh3d` binaries); pure geometry, no skinning attributes or glTF dependency at runtime | [diamesh3d.md](systems/diamesh3d/diamesh3d.md) |
| DiaThreading | Task-based parallelism — `JobSystem`, `JobHandle`; extracted from DiaCore to allow DiaObservation dependency; `dia.jobs.*` metrics via JobSystemModule | [diathreading.md](systems/diathreading/diathreading.md) |
| DiaBugDetection | Static analysis + sanitizer build configs + agentic Claude fix loop — `dia check`, `dia diagnose`, CI gate | [diabugdetection.md](systems/diabugdetection/diabugdetection.md) |
| DiaRenderTest | Offline visual correctness pipeline — frame capture PNG writer (DiaBgfx extension), C++ pixel diff engine + JSON report + expectation evaluator (DiaCaptureTest module), Python SSIM + `dia check render-diff` + AI triage tools | [diarendertest.md](systems/diarendertest/diarendertest.md) |
| DiaArchitecture | Domain-oriented module layer model + CMake enforcement — `layer:` YAML formalisation, `dia check --tool=arch` audit, Foundation CMake pilot, full layered INTERFACE model | [diaarchitecture.md](systems/diaarchitecture/diaarchitecture.md) |
| DiaCore | Foundation library (containers, type system, memory, logging, CRC) | [diacore.md](systems/diacore/diacore.md) |
| DiaReflect | Archive-based reflection & serialization — macro DSL, JSON/binary archives, versioning, polymorphism; eventual replacement for DiaCore/Type | [diareflect.md](systems/diareflect/diareflect.md) |
| DiaMaths | Math library (vectors, matrices, quaternions, transforms, core math utilities — pure linear algebra only after DiaGeometry2D migration) | [diamaths.md](systems/diamaths/diamaths.md) |
| DiaCamera2D | 2D camera library — Camera2D type, CameraRegistry2D, ICameraBehaviour interface with factory, 8 engine behaviours (Follow, SmoothDamp, Deadzone, BoundsClamp, ScreenShake, ZoomToFit, Pan, Zoom) | [diacamera2d.md](systems/diacamera2d/diacamera2d.md) |
| DiaCamera3D | 3D camera library — Camera3D type (position + quaternion + perspective/ortho projection), ViewportTransform3D, CameraRegistry3D, ICameraBehaviour3D interface with factory, 6 engine behaviours (Follow3D, SmoothDamp3D, BoundsClamp3D, ScreenShake3D, Orbit, Flythrough) | [diacamera3d.md](systems/diacamera3d/diacamera3d.md) |
| DiaLighting2D | 2D point light library — PointLight2D type, LightRegistry2D, layer-mask affinity, query by layer index | [dialighting2d.md](systems/dialighting2d/dialighting2d.md) |
| DiaLighting3D | 3D light library — PointLight3D, DirectionalLight3D, SpotLight3D, AmbientLight3D types, LightRegistry3D, ILightBehaviour3D interface with factory, 3 engine behaviours (Flicker, Pulse, ColorCycle), LightPathBehaviour3D (spline path) | [dialighting3d.md](systems/dialighting3d/dialighting3d.md) |
| DiaLighting3DVisualDebugger | Visual debug rendering for 3D lights — per-light debug widgets (sphere/arrow), spline arc preview for path behaviours, opt-in via DebugWidgetConfig on light structs | [dialighting3dvisualdebugger.md](systems/dialighting3dvisualdebugger/dialighting3dvisualdebugger.md) |
| DiaScene2D | 2D scene library — `.diascene` reflected file format, LayerTable, SceneLoader2D (hydrates camera/light registries + spawns entities) | [diascene2d.md](systems/diascene2d/diascene2d.md) |
| DiaScene3D | 3D scene library — `.diascene` reflected file format (`"scene3d"` key), SceneGraph3D (flat list + parent-index links), SceneLoader3D (hydrates camera/light registries + spawns entities + builds graph), Submit() (frustum derivation, transform resolution, culling, draw command emission) | [diascene3d.md](systems/diascene3d/diascene3d.md) |
| DiaGraphics | Graphics abstraction layer (ICanvas, FrameData, DebugPrimitive, rendering contracts) | [diagraphics.md](systems/diagraphics/diagraphics.md) |
| DiaGraphics3D | 3D rendering type layer — Camera3D, lights, Mesh3DDrawCommand, Mesh3DFrameData, FrameData3D; separate module so 2D-only consumers never pull in Matrix44 | [diagraphics3d.md](systems/diagraphics3d/diagraphics3d.md) |
| DiaWindow | Window management | TBD |
| DiaInput | Input handling (keyboard, mouse, events) | TBD |
| DiaSDL | SDL3-backed window + input backend — implements `IWindow` + `IInputSource` for Windows, Linux, Android, iOS; replaces DiaSFML | [replace-diasfml-with-sdl3.md](systems/diasdl/replace-diasfml-with-sdl3.md) |
| DiaUI | UI system abstraction | TBD |

## Application-Specific Architecture

### Modular Engine Structure

Dia is organized into independent subsystems with clear responsibilities and dependencies:
- **Module System**: Each module documented with `dia.*.architecture.module.md` YAML frontmatter
- **Dependency Management**: Module dependency graph validated via `Tools/dia_modules.py`
- **Public APIs**: Each module exposes headers, namespaces, and entry points

### Runtime Framework (DiaApplicationFlow)

ProcessingUnit/Phase/Module architecture for multi-threaded game execution:
- **ProcessingUnit**: High-level execution containers (can run on separate threads)
- **Phase**: Execution stages with state machine transitions
- **Module**: Functional units providing services to phases

### Build-Time Tooling

DiaAPI provides extensible command-line tools for asset pipelines and build automation.

## Platform Dependencies

Dia is the engine - it provides dependencies for other applications rather than consuming them. External dependencies:

- **SDL3** (External/SDL3) - Window management and input (replaces SFML window/input path; git submodule, built via `dia env setup`)
- **jsoncpp** (External/jsoncpp-master) - JSON parsing
- **Webix / VisJS** (External/) - Web UI for debugging/visualization
- **GoogleTest** (External/googletest) - Unit testing framework

## Out of Scope

What the Dia engine deliberately does NOT provide:

- **Game-specific logic** - Dia is an engine, not a game; game logic belongs in game applications
- **High-level game features** - No built-in inventory, quest, or gameplay systems
- **Content creation tools** - No level editors, asset authoring tools (yet - DiaAPI is first step)
- **Network/multiplayer** - Not yet implemented
- **Mobile/console platforms** - Windows-only currently; Android/iOS targeted via DiaSDL (window+input ready; full platform pipeline TBD)

## Key Users / Personas

1. **Game Developers** - Building games on Dia; need stable APIs and good documentation
2. **Engine Developers** - Extending Dia systems; need clear module boundaries and architecture
3. **Technical Artists** - Processing assets through build tools; need reliable pipelines
4. **Platform Maintainers** - Managing engine evolution; need dependency tracking and breaking change management

## Decisions

<!-- Decisions specific to this application. Binding decisions cascade to all systems within Dia.
     AI: Always check parent platform decisions (Cluiche.md) first — those take precedence.
     Use AD- prefix for application-level decision IDs. -->

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| AD-001 | Module system with YAML frontmatter documentation | Enables tooling (dependency validation, graph generation); enforces clear ownership | All Dia modules | Accepted | Yes |
| AD-002 | No STL containers in public APIs | Dia containers (DynamicArrayC, HashTable) ensure consistent memory management and engine integration | All Dia modules | Accepted | Yes |
| AD-003 | Namespace convention: `Dia::<Module>::` | Clear ownership; prevents naming conflicts | All Dia modules | Accepted | Yes |
| AD-004 | ProcessingUnit/Phase/Module for application structure | Multi-threaded execution with explicit scheduling; clear lifecycle management | DiaApplicationFlow | Accepted | Yes |
| AD-005 | Component-based entities (IComponent/IComponentObject) | Composition over inheritance; flexible runtime entity construction | All game-facing modules | Superseded by diaentitytemplate | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all child systems · `No` = guidance only

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Architecture | Should each Dia module (DiaCore, DiaMaths, etc.) be a system in this spec? | Yes - each major module should have a system spec defining its public API, responsibilities, and features |
| 2 | Scope | Are build tools (DiaAPI) part of Dia or separate application? | Part of Dia - they're engine tooling, depend on DiaCore, and extend the engine's capabilities |
| 3 | Dependencies | Should deprecated code (Dia/DiaCore/Deprecated/) be documented? | No - deprecated code is not compiled and should not be referenced |
| 4 | Decisions | Do Dia decisions (AD-xxx) override platform decisions (PD-xxx)? | No - platform decisions take precedence. Dia decisions add engine-specific constraints within platform rules |

## Status

`Active` - Core engine application for the Cluiche platform
