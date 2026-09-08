# Research: Explore — Editor Script API for Automation and AI Tooling

**Session date:** 2026-06-05
**Folder:** docs/research/editor_script_api_ai/

## Problem Space Overview

CluicheEditor is a plugin-based editor built on DiaEditor. Its plugins expose behaviour through a `WebUIBridge` (JSON over CEF/WebSocket) and commands registered with DiaAPI. Today there is no unified surface through which an external Python script — or an AI agent — can drive the editor end-to-end: open a project, trigger a pipeline build, navigate between panels, query plugin state, or validate outcomes. Each plugin has its own JSON handler map with no shared discovery contract.

The goal is a scriptable API layer that maps all meaningful editor and plugin actions to callable Python functions. This serves two audiences: automation engineers writing deterministic test scripts, and AI agents constructing higher-level workflows (e.g. "build the project, open the pipeline panel, check the last run succeeded"). The two audiences share the same wire protocol but differ in how they discover and compose calls.

DiaPython already embeds CPython in the engine and exposes DiaAPI CLI commands as Python callables via `InitializePythonBindings()`. DiaWebSocket provides a live bidirectional channel. DiaAPI's JSON command path (`ExecuteCommandJson`) is a clean request/response envelope. The plumbing exists; what is missing is the editor-side contract: a stable, versioned, discoverable set of named operations covering every plugin action, plus the in-process or out-of-process Python harness that exposes them.

## Existing Approaches

- **VSCode Extension API** — Every command registered in the command palette is callable from extensions or tests via `vscode.commands.executeCommand`. Discovery is via contributed command manifests.
- **Unreal Python API** — `unreal.EditorAssetLibrary`, `unreal.LevelEditorSubsystem` etc. Each subsystem exposes a flat module of functions, discoverable via `help()`. Editor automation scripts import these modules directly.
- **Unity Scripting (C#)** — `EditorApplication`, `AssetDatabase`, `Selection` static APIs. Test framework via `EditMode`/`PlayMode` test runners. External tools call in via `UnityEditor.Compilation.CompilationPipeline` events.
- **Godot EditorScript / EditorPlugin** — `EditorPlugin.add_tool_menu_item()`, `EditorInterface` singleton, scripted via GDScript or C#. Automatable from CLI via `godot --headless --script`.
- **Remote Procedure Call (MCP / JSON-RPC 2.0)** — Language-agnostic protocol for tool invocation over a socket. Named methods, positional or keyword params, structured errors. Used by Claude MCP servers.
- **Screenplay Pattern (test automation)** — Actors execute Tasks composed of Interactions. Well-suited for AI agents that need to reason about what a "high-level action" means in terms of lower-level calls.
- **OpenTelemetry + structured events** — Emit structured events for every editor action; replay or observe them externally.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **Transport** | In-process (same PID), WebSocket (TCP), Named Pipe, HTTP | WebSocket already exists (DiaWebSocket); in-process is fastest but requires same binary |
| **Protocol** | Raw JSON, JSON-RPC 2.0, REST-like, custom | JSON-RPC 2.0 adds id/method/params/error standard with no extra deps |
| **Discovery** | Static generated bindings, runtime introspection, schema file (OpenAPI-like) | Runtime introspection via DiaAPI command list is already available |
| **Python surface** | Auto-generated stubs from command registry, hand-written facade, both | Auto-gen from registry ensures no drift; hand-written facade for composite operations |
| **Execution model** | Fire-and-forget, request-response, async with callbacks | Editor operations have side effects; request-response is safest default |
| **Scope** | DiaAPI commands only, DiaAPI + plugin handlers, full plugin state | Plugin handlers (WebUIBridge) cover more surface than CLI commands alone |
| **Versioning** | No versioning, semantic version on schema, per-command versions | DiaAPI CommandInfo already has a `version` field |
| **AI interface** | Raw Python bindings, MCP server, both | MCP server is directly usable by Claude and other agents with no extra code |

## Known Tradeoffs

- **In-process vs out-of-process**: In-process Python (DiaPython) is fastest and avoids serialization but ties the scripting lifetime to the editor process. Out-of-process (WebSocket + external Python) is decoupled but adds latency and needs reconnect logic.
- **Auto-generated bindings vs stable API**: Generating Python stubs from the live command registry guarantees coverage but means any command rename breaks scripts. A facade layer absorbs churn.
- **Breadth vs depth**: Covering every plugin's full handler map (WebUIBridge) is comprehensive but expensive to maintain. Starting with DiaAPI commands (already registered, already have JSON path) gives 80% coverage for 20% of the work.
- **MCP overhead**: MCP server adds a process boundary and JSON serialization on every call. For tight automation loops (100s of calls/s) this matters. For AI-driven workflows (seconds between calls) it is irrelevant.
- **Plugin action coverage**: Not all plugin behaviour is reachable via DiaAPI commands. Some actions are registered only as WebUIBridge request handlers (called from JS). Exposing those requires a second surface or unifying the registration.

## Known Pitfalls (C++ / game engine context)

- **Thread safety**: Editor plugins run on the main thread. Script calls arriving via WebSocket arrive on a worker thread. All command dispatch must marshal to the correct thread or be explicitly thread-safe.
- **Synchronous blocking**: Long-running commands (full pipeline build) will block the caller. Need either async design or a job/ticket model.
- **CEF entanglement**: `WebUIBridge` handlers are currently wired to CEF's message pump. Bypassing CEF to call them directly from script requires care around CEF threading rules.
- **State leakage between test runs**: Scripted tests that mutate editor state need explicit teardown. No current reset/restore API exists.
- **DiaPython initialisation order**: DiaPython must be initialised before any `ExecuteScript` call, and after `DiaAPI::Initialize()`. If the script harness starts before the editor is ready, calls fail silently.
- **PD-004 in Python bindings**: Public APIs must not expose STL containers. The Python binding layer must convert `DynamicArrayC` results to Python lists internally.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| **DiaAPI** | Command registry with JSON path already exists; `ExecuteCommandJson` is the natural call target |
| **DiaPython** | CPython already embedded; `InitializePythonBindings()` auto-generates callables from DiaAPI commands |
| **DiaEditor / EditorPluginBase** | `RegisterRequestHandler` / `RegisterEventHandler` on WebUIBridge — already a JSON handler map per plugin |
| **DiaWebSocket** | Bidirectional socket server; could serve as the out-of-process transport for script → editor |
| **DiaAutomation** | Planned: checkpoints, pause/resume, navigation hold, CI heartbeat — not yet implemented |
| **DiaPipelineEditor** | `RegisterCommands()` already registers pipeline operations with DiaAPI |
| **CluicheEditor / PluginLoaderModule** | Can enumerate loaded plugins; plugin instances reachable at runtime |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | Command names and plugin IDs must be StringCRC constants; Python bindings map string literals → CRC at call time |
| PD-002 ProcessingUnit/Phase/Module | Script API lives as a Module on the MainProcessingUnit; all calls must respect phase lifecycle |
| PD-004 No STL in public APIs | Binding layer converts DynamicArrayC ↔ Python list/dict internally; never exposes std::vector |
| PD-006 VS project files are source of truth | Any new module (DiaScriptServer, DiaEditorScriptAPI) needs a `.vcxproj` entry |
| PD-007 C++20 required | Python binding layer can use structured bindings, constexpr, std::span safely |
| PD-009 Generated output under `Cluiche/out/` | Generated Python stub files belong under `Cluiche/out/CluicheEditor/scripts/` |

## Open Questions for Ideation

- Should the Python API be in-process (DiaPython, same PID as the editor) or out-of-process (external Python process talking over WebSocket)? Or both?
- Should plugin actions be exposed uniformly through DiaAPI commands, or should there be a second registration path for "scriptable plugin actions" that aren't full CLI commands?
- Does an MCP server (for Claude/AI agents) replace or complement a Python test harness?
- What is the minimum viable surface for the first iteration — DiaAPI commands only, or must plugin handler coverage be included?
- How does versioning work? If a command is renamed or its params change, how do existing scripts fail gracefully rather than silently?
- Should AI agents call a Python harness, or should they talk directly to a DiaAPI WebSocket/MCP server without Python in the loop?
- What is the rollout strategy — does the script API live in CluicheEditor itself, or in a headless mode that doesn't require the full editor UI to be running?
