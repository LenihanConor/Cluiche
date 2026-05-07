# Application Spec: Dia

## Parent Platform
@docs/specs/platform/Cluiche.md

## Purpose

Dia is the game engine application that provides all shared engine infrastructure for the Cluiche platform. While organized as an "application" in the spec hierarchy, Dia functionally serves as the shared codebase that all other applications (games, tools, tests) depend on. It provides core systems (DiaCore, DiaMaths, DiaGraphics, DiaInput, etc.), runtime frameworks (DiaApplication), and build tooling (DiaAPI) that enable rapid game development with a component-based architecture, multi-threaded execution model, and modular design.

## Systems

| System | Description | Spec |
|--------|-------------|------|
| DiaAPI | C++ command registration and execution API framework - provides runtime command infrastructure | [diaapi.md](../systems/dia/diaapi.md) |
| DiaCLI | Python-based CLI tool for development workflows (build, assets, scaffolding, utilities) | [diacli.md](../systems/dia/diacli.md) |
| DiaPython | Python embedding framework - wraps pybind11 with clean C++ API for scripting integration | [diapython.md](../systems/dia/diapython.md) |
| DiaApplication | Application framework (ProcessingUnit/Phase/Module architecture for multi-threaded execution) | [diaapplication.md](../systems/dia/diaapplication.md) |
| DiaApplicationEditor | Editor plugin for editing .diaapp manifests (separate system, uses DiaEditor framework) | [diaapplicationeditor.md](../systems/dia/diaapplicationeditor.md) |
| DiaDebugProtocol | Shared header-only protocol types for editor-game communication (used by DiaEditor and DiaDebugServer) | [diadebugprotocol.md](../systems/dia/diadebugprotocol.md) |
| DiaDebugServer | WebSocket server for remote debugging - broadcasts game state, forwards DiaAPI commands to editors | [diadebugserver.md](../systems/dia/diadebugserver.md) |
| DiaEditor | Editor framework system (MVC, plugin system, CEF UI, command integration, live WebSocket connection) | [diaeditor.md](../systems/dia/diaeditor.md) |
| DiaUICEF | CEF-based UI system implementing IUISystem | [diauicef.md](../systems/dia/diauicef.md) |
| DiaUIUltralight | Ultralight-based IUISystem implementation | [diauiultralight.md](../systems/dia/diauiultralight.md) |
| DiaWebSocket | WebSocket server/client abstraction wrapping websocketpp (used by DiaDebugServer, DiaEditor, future networking) | [diawebsocket.md](../systems/dia/diawebsocket.md) |
| DiaLogger | Engine-wide logging system (levels, channels, thread-local buffers, pluggable sinks via ISink) | [dialogger.md](../systems/dia/dialogger.md) |
| DiaGeometry2D | 2D geometry system: shape primitives, intersection tests, Transform, spatial structures (Grid, Quadtree, BVH) | [diageometry2d.md](../systems/dia/diageometry2d.md) |
| DiaRigidBody2D | 2D rigid body physics: velocity/force integration, collision detection + response, constraints/joints, sleeping, collision layers, Physics-channel logging | [diarigidbody2d.md](../systems/dia/diarigidbody2d.md) |
| DiaRigidBody2DVisualDebugger | Read-only debug visualization of rigid body physics state — superseded by DiaVisualDebugger system | [diarigidbody2dvisualdebugger.md](../systems/dia/diarigidbody2dvisualdebugger.md) (Superseded) |
| DiaSoftBody2D | 2D soft body simulation: PBD ropes, cloth, particle-geometry collision, rigid body coupling, Physics-channel logging, visual debugger | [diasoftbody2d.md](../systems/dia/diasoftbody2d.md) |
| DiaEnv | Portable development environment system — SDK manifest, toolchain manifest, `dia env setup/verify`, MSBuild auto-restore, git submodule migration, AI context hardening | [diaenv.md](../systems/dia/diaenv.md) |
| DiaTest | Test execution system — `dia test cli` (pytest for DiaCLI), `dia test env-integration` (agentic env→pipeline→test loop), future `dia test googletest` and `dia test ui` | [diatest.md](../systems/dia/diatest.md) |
| DiaTestHarness | External Python/pytest e2e orchestration — JSON scenarios, WebSocket to DiaDebugServer, crash detection, structured results | [diatestharness.md](../systems/dia/diatestharness.md) |
| DiaPipeline | Multi-stage build pipeline — `dia pipeline` command surface; proto-compile, compile-code, asset-build, package stages; `pipeline.toml` config; host or Docker execution | [diapipeline.md](../systems/dia/diapipeline.md) |
| DiaPipelineEditor | Live pipeline viewer + trigger panel inside CluicheEditor — tails NDJSON log, stage timeline with drill-down, build triggering, last 10 run history | [diapipelineeditor.md](../systems/dia/diapipelineeditor.md) |
| DiaData | Foundational data model — asset identity (StringCRC), type framework, central registry with bidirectional relationships, JSON definition loader | [diadata.md](../systems/dia/diadata.md) |
| DiaAssetPipeline | Build-time asset pipeline — discover, validate, transform, deploy raw assets to built output; NDJSON logging; DiaAPI commands | TBD |
| DiaStageLoader | Runtime asset loading — Stage-based loading, Bundle resolution, typed asset access from built output | TBD |
| DiaAssetBrowserEditor | Editor UI for asset browsing, inspection, relationship graph navigation, validation status display | TBD |
| DiaSerializer | Shared serialization primitives — `MetadataValue`/`MetadataArray`, `SerializeResult`, `ISerializer` interface (version, migration query, file I/O helpers), `JsonMetadataHelpers`. Foundation for all domain serializers. | [diaserializer.md](../systems/dia/diaserializer.md) |
| DiaStateMachine | Generic state machine library — dual flat FSM + hierarchical state machine, pushdown automaton, component wrapper, transition logging/tracing, shared inspection interface | [diastatemachine.md](../systems/dia/diastatemachine.md) |
| DiaRig2D | 2D skeletal rig system — bone hierarchy, forward kinematics, pose representation/blending, JSON skeleton definitions, skeleton component, debug renderer | [diarig2d.md](../systems/dia/diarig2d.md) |
| DiaIK2D | Inverse kinematics — analytic two-bone solver, FABRIK N-joint solver, look-at constraint; post-process pass on DiaRig2D skeletons | [diaik2d.md](../systems/dia/diaik2d.md) |
| DiaAnimation2D | Animation playback and blending — damped spring chains, keyframe clip player, pose blend stack, procedural locomotion oscillator (deferred) | [diaanimation2d.md](../systems/dia/diaanimation2d.md) |
| DiaVisualDebugger | Visual debug rendering system — `DebugLayerManager`, stack of focused draw classes, `DebugColourPalette`, `DiaVisualDebuggerConsole` (ImGui), editor layer panel, extensions to DiaGraphics (budget, TextPrimitive) | [diavisualdebugger.md](../systems/dia/diavisualdebugger.md) |
| DiaCore | Foundation library (containers, type system, memory, logging, CRC) | [diacore.md](../systems/dia/diacore.md) |
| DiaMaths | Math library (vectors, matrices, core math utilities — pure linear algebra only after DiaGeometry2D migration) | TBD |
| DiaGraphics | Graphics abstraction layer (ICanvas, FrameData, DebugPrimitive, rendering contracts) | [diagraphics.md](../systems/dia/diagraphics.md) |
| DiaWindow | Window management | TBD |
| DiaInput | Input handling (keyboard, mouse, events) | TBD |
| DiaUI | UI system abstraction | TBD |

## Application-Specific Architecture

### Modular Engine Structure

Dia is organized into independent subsystems with clear responsibilities and dependencies:
- **Module System**: Each module documented with `dia.*.architecture.module.md` YAML frontmatter
- **Dependency Management**: Module dependency graph validated via `Tools/dia_modules.py`
- **Public APIs**: Each module exposes headers, namespaces, and entry points

### Runtime Framework (DiaApplication)

ProcessingUnit/Phase/Module architecture for multi-threaded game execution:
- **ProcessingUnit**: High-level execution containers (can run on separate threads)
- **Phase**: Execution stages with state machine transitions
- **Module**: Functional units providing services to phases

### Build-Time Tooling

DiaAPI provides extensible command-line tools for asset pipelines and build automation.

## Platform Dependencies

Dia is the engine - it provides dependencies for other applications rather than consuming them. External dependencies:

- **SFML** (External/SFML) - Graphics, window, multimedia
- **jsoncpp** (External/jsoncpp-master) - JSON parsing
- **Webix / VisJS** (External/) - Web UI for debugging/visualization
- **GoogleTest** (External/googletest) - Unit testing framework

## Out of Scope

What the Dia engine deliberately does NOT provide:

- **Game-specific logic** - Dia is an engine, not a game; game logic belongs in game applications
- **High-level game features** - No built-in inventory, quest, or gameplay systems
- **Content creation tools** - No level editors, asset authoring tools (yet - DiaAPI is first step)
- **Network/multiplayer** - Not yet implemented
- **Mobile/console platforms** - Windows-only currently

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
| AD-004 | ProcessingUnit/Phase/Module for application structure | Multi-threaded execution with explicit scheduling; clear lifecycle management | DiaApplication | Accepted | Yes |
| AD-005 | Component-based entities (IComponent/IComponentObject) | Composition over inheritance; flexible runtime entity construction | All game-facing modules | Accepted | Yes |

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
