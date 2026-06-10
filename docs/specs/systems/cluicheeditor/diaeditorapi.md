# System Spec: DiaEditorAPI

## Parent Application
@docs/specs/applications/cluicheeditor.md

**Research:** @docs/research/editor_script_api_ai/summary.md

## Purpose

DiaEditorAPI is the editor action contract layer for CluicheEditor. It owns a registry of all scriptable editor actions — project management, plugin control, pipeline operations, game connection — and exposes them through two derived surfaces: an auto-generated Python module (`dia_editor`) for automation test scripts, and a protocol-agnostic AI manifest with MCP as the first serialisation target for Ollama/Claude tool-calling.

DiaAPI remains engine-layer plumbing (dispatch, JSON envelope, Python infrastructure). DiaEditorAPI is the editor-layer contract that sits above it. These are distinct responsibilities and must not be merged.

**Target users:**
- Automation engineers writing deterministic Python test scripts
- AI agents (Ollama, Claude) driving editor workflows via natural language
- CI pipelines invoking editor operations headlessly

## Responsibilities

- Own the `EditorAction` registry: name, description, param schema, thread dispatch policy, handler
- Cross-register actions with DiaAPI's JSON path (for script dispatch) and WebUIBridge (for JS/CEF dispatch) simultaneously via `RegisterAction()`
- Auto-generate the `dia_editor` Python module from the live registry at startup
- Emit `.pyi` stub files to `Cluiche/out/CluicheEditor/scripts/` for IDE completion
- Serve the action manifest on demand (for AI adapter consumption)
- Own the MCP server (Phase 2): `tools/list` and `tools/call` endpoints
- Manage a thread-safe marshal queue: script calls arriving on non-UI threads are dispatched to the correct thread per action's declared policy
- Manage action deregistration tied to plugin lifecycle (no stale commands after plugin unload)
- Enforce the Action Manifest authoring contract (EAPI-009): every action's `description`, param schema, and return shape must be spec'd before implementation begins

## Not Responsible For

- DiaAPI command registry internals — DiaEditorAPI calls into DiaAPI, does not replace it
- WebUIBridge push notifications — WebUIBridge stays for C++→JS events; DiaEditorAPI only touches request/response
- CEF-entangled UI actions (`save_layout`, `toggle_panel_visibility`) — these remain WebUIBridge-only and are not scriptable
- AI model selection or prompt construction — that is the AI agent's concern
- Headless editor mode — deferred; DiaEditorAPI is a prerequisite, not the solution

## Architecture

### Three-Tier Design

```
[External: Python script / Ollama / CI]
        ↓
Tier 3 — AI Adapter (MCP server, Phase 2)
        ↓
Tier 2 — Python Adapter (auto-generated dia_editor module)
        ↓
Tier 1 — EditorAction Registry (C++, DiaEditorAPI module)
        ↓
DiaAPI JSON path  +  WebUIBridge request handlers
```

### Tier 1 — EditorAction Registry

Each registered action has an `EditorActionDescriptor`:

```cpp
struct EditorActionDescriptor {
    StringCRC           name;           // e.g. StringCRC("project.open_path")
    const char*         description;    // human + AI readable
    const char*         category;       // e.g. "project", "plugin", "pipeline"
    const char*         owner;          // e.g. "DiaEditor"
    EditorActionParamSchema params;     // typed param definitions (name, type, required, description)
    DispatchThread      dispatchThread; // kMainThread | kCallerThread
    ActionHandler       handler;        // std::function<Json::Value(const Json::Value&)>
};
```

Registration API:
```cpp
// Register an action — cross-registers with DiaAPI JSON path automatically
bool DiaEditorAPI::RegisterAction(const EditorActionDescriptor& descriptor);

// Deregister all actions owned by a given plugin (called on plugin unload)
void DiaEditorAPI::DeregisterActionsForOwner(StringCRC owner);

// Get the full manifest (used by Python gen + AI adapter)
const EditorActionManifest& DiaEditorAPI::GetManifest() const;
```

### Tier 2 — Python Adapter

At startup, after all plugins have registered their actions, `DiaEditorAPI` calls `GeneratePythonModule()`. This introduces a **startup ordering constraint**: all plugins must be loaded before `GeneratePythonModule()` fires. CluicheEditor Phase 1 loads all plugins eagerly at startup to satisfy this. If on-demand plugin loading is added later, re-calling `GeneratePythonModule()` after each load is sufficient.
- Enumerates all registered `EditorActionDescriptor`s
- Creates Python callables in the `dia_editor` module via DiaPython's `AddFunction()`
- Each callable validates params, calls `DiaAPI::ExecuteCommandJson()`, returns result dict
- Emits `dia_editor.pyi` to `Cluiche/out/CluicheEditor/scripts/`

Example generated surface:
```python
import dia_editor

# Open a project
result = dia_editor.project.open_path(path="C:/MyGame/mygame.diagame")

# Trigger a pipeline build
result = dia_editor.pipeline.build(target="cluichetest", config="Debug")

# Connect to a running game
result = dia_editor.game_connection.connect(address="localhost", port=8080)
```

### Tier 3 — AI Adapter (Phase 2)

A MCP server that serialises the `EditorActionManifest` to MCP tool descriptors:
- `tools/list` → returns all registered actions as MCP tool schemas
- `tools/call` → dispatches to `DiaEditorAPI::ExecuteAction()`, returns structured result

The registry is protocol-agnostic — the MCP adapter reads descriptors and serialises them. Adding an OpenAI function-calling adapter or a Gemini adapter means writing a new serialiser, not touching the registry.

Ollama at-desk workflow:
```
User: "Add me an entity asset"
Ollama: calls tools/list → sees entity.create(name, template, ...)
Ollama: "What name and template?" → gathers params conversationally
Ollama: calls tools/call entity.create → editor creates asset
Ollama: reports result to user
```

### Thread Safety

Script calls arrive on a non-UI thread (DiaPython thread or WebSocket thread). Actions with `dispatchThread = kMainThread` are queued to an `EditorActionQueue` that the `EditorActionModule` drains each frame. The Python caller **blocks synchronously** on a future until the result is set, with a 5-second timeout that surfaces as a Python exception rather than a deadlock. Actions with `kCallerThread` execute inline (read-only queries only).

```
Python call (non-UI thread):
  → push (action, params, future) onto EditorActionQueue
  → block on future.get() with 5s timeout

Frame tick (main thread):
  EditorActionModule::DoUpdate()
    → drain EditorActionQueue
      → for each pending call: invoke handler on main thread
        → set result on the caller's future → Python unblocks
```

### Routing and Handler Uniqueness

WebUIBridge and DiaAPI JSON are routing surfaces only. Both routes call `DiaEditorAPI::ExecuteAction(name, params)`. The handler is stored once in the registry — there is no divergence between the JS/CEF path and the Python/script path.

### Action Coverage Plan

Phase 1 migrates the highest-priority WebUIBridge-only handlers to dual-registration:

| Action | Current home | Priority |
|--------|-------------|----------|
| `project.open_path` | WebUIBridge only | P0 |
| `project.close` | WebUIBridge only | P0 |
| `project.get_state` | WebUIBridge only | P0 |
| `game_connection.connect` | WebUIBridge only | P0 |
| `game_connection.disconnect` | WebUIBridge only | P0 |
| `game_connection.get_state` | WebUIBridge only | P0 |
| `plugin.load` | DiaAPI JSON | P0 |
| `plugin.unload` | DiaAPI JSON | P0 |
| `pipeline.build` | DiaPipelineEditor WebUIBridge | P1 |
| `pipeline.get_status` | DiaPipelineEditor WebUIBridge | P1 |
| All DiaAPI JSON commands | DiaAPI (already reachable) | P2 — surfaced automatically |

CEF-entangled actions (`save_layout`, `toggle_panel_visibility`, `editor.open_file_dialog`) are **excluded** — they require a visible CEF window and cannot be safely called from script.

## Public API

```cpp
// Registration (called by plugin authors)
bool RegisterAction(const EditorActionDescriptor&);
void DeregisterActionsForOwner(StringCRC owner);

// Manifest (used internally by adapters)
const EditorActionManifest& GetManifest() const;

// Execution (used by the marshal queue and adapters)
Json::Value ExecuteAction(StringCRC name, const Json::Value& params);

// Python generation (called once after all plugins loaded)
void GeneratePythonModule();
void EmitPythonStubs(const char* outputPath);

// MCP server (Phase 2)
bool StartMCPServer(uint16_t port);
void StopMCPServer();
```

## Phased Delivery

### Phase 1 — Automation Testing
1. `EditorActionDescriptor` struct + `EditorActionManifest`
2. `DiaEditorAPI::RegisterAction()` with DiaAPI JSON cross-registration
3. Thread-safe `EditorActionQueue` + `EditorActionModule` frame drain
4. Plugin lifecycle deregistration (`DeregisterActionsForOwner`)
5. Migrate 8 P0 actions from WebUIBridge to dual-registration
6. Python adapter: `GeneratePythonModule()` + `EmitPythonStubs()`
7. `dia_editor` Python module available for test scripts

### Phase 2 — AI Tooling
1. Protocol-agnostic manifest serialiser interface
2. MCP adapter: `tools/list` + `tools/call` over DiaWebSocket
3. P1 pipeline actions migrated to dual-registration
4. Ollama integration validated with at-desk workflow

## Dependencies

| Module | Role |
|--------|------|
| DiaAPI | JSON dispatch (`ExecuteCommandJson`), Python infrastructure (`InitializePythonBindings`) |
| DiaEditor / WebUIBridge | Cross-registration target for request handlers |
| DiaPython | Python module creation (`CreateModule`, `AddFunction`) and script execution |
| DiaWebSocket | MCP server transport (Phase 2) |
| DiaApplicationFlow | `EditorActionModule` lives here as a Module on EditorPU |

## Inherited Binding Decisions

| Source | ID | Decision | Impact on DiaEditorAPI |
|--------|----|----------|-----------------------|
| Platform | PD-001 | StringCRC for all IDs | Action names, categories, owner IDs all use StringCRC constants |
| Platform | PD-002 | PU/Phase/Module architecture | `EditorActionModule` is a Module on EditorPU; DrainQueue runs in `DoUpdate` |
| Platform | PD-004 | No STL in public APIs | `EditorActionManifest` uses `DynamicArrayC`; Python binding layer converts internally |
| Platform | PD-007 | C++20 required | Descriptor structs use designated initialisers; constexpr StringCRC literals |
| Platform | PD-009 | Generated output under `Cluiche/out/` | `.pyi` stubs emit to `Cluiche/out/CluicheEditor/scripts/` |
| CluicheEditor | AED-001 | DiaEditor is a pure library; CluicheEditor owns application flow | `EditorActionModule` lives in CluicheEditor, not DiaEditor |
| CluicheEditor | AED-004 | WebSocket-first for game connection | MCP server uses DiaWebSocket per this decision |

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| EAPI-001 | DiaAPI is plumbing; DiaEditorAPI is the editor action contract — never merge | DiaAPI is engine-layer with no editor dependency; pulling editor concepts into it would invert the dependency direction | System-wide | Accepted | Yes |
| EAPI-002 | Python adapter is auto-generated from the live registry, never hand-authored | Hand-authored facades drift as plugins are added; registry is the single source of truth | Python surface | Accepted | Yes |
| EAPI-003 | AI adapter is protocol-agnostic at the registry level; MCP is the first serialisation target | AI protocols are evolving; binding the registry to MCP would require registry changes when the protocol landscape shifts | AI surface | Accepted | Yes |
| EAPI-004 | Thread dispatch policy is declared per action, not inferred | Inferring thread safety is fragile; explicit policy is checked at registration and enforced by the marshal queue | Thread safety | Accepted | Yes |
| EAPI-005 | CEF-entangled actions are excluded from scriptable surface; flag with `kCEFOnly` | Actions that drive CEF directly cannot be safely called without a visible window; including them would crash headless callers | Action coverage | Accepted | Yes |
| EAPI-006 | Deregistration is tied to plugin owner StringCRC — all actions for an owner deregister atomically on plugin unload | Per-action deregistration is error-prone; owner-scoped cleanup is safe and matches plugin lifecycle | Plugin lifecycle | Accepted | Yes |

## Decisions (continued)

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| EAPI-007 | MCP server binds to a fixed port with a default of `7777`, overridable in `editor.diaapp` | Ollama requires a known address; dynamic port adds sidecar/stdout complexity with no benefit for single-editor desktop use. If a second editor instance fails to bind it logs a clear error. | AI surface (Phase 2) | Accepted | Yes |
| EAPI-008 | No version field on `EditorActionDescriptor`; breaking param changes require a renamed action (e.g. `project.open_path_v2`) | A version integer defers the breaking-change decision; renaming forces it explicitly and gives callers a clear "action not found" error rather than silent misbehaviour. Same pattern as REST. | Action contract | Accepted | Yes |
| EAPI-009 | Every feature spec that registers DiaEditorAPI actions must include an **Action Manifest** section defining, for each action: `description` (rich, AI-readable, ~2–4 sentences), `category`, `owner`, `dispatch`, `params` (name, type, required, description), and `returns`. Descriptions are the source of truth — copied verbatim into `EditorActionDescriptor` at implementation time. | Descriptions written at spec time are richer and more intentional than those written during coding. They flow into MCP `tools/list`, Python `.pyi` docstrings, and generated documentation without any extra work. Leaving them to implementers produces terse, inconsistent strings that degrade AI tool-calling quality. | All DiaEditorAPI feature specs | Accepted | Yes |

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| app-editor-actions | `app_editor.*` namespace — `get_active_context` + 4 navigate actions; single code path via `AppEditorController`; C++ internal navigation rerouted through `ExecuteAction()` | [app-editor-actions.md](../../features/cluicheeditor/diaeditorapi/app-editor-actions.md) | Approved |
| asset-catalogue-migration | Dual-register all 33 `asset_catalogue.*` WebUIBridge handlers + new `get_available` action | [asset-catalogue-migration.md](../../features/cluicheeditor/diaeditorapi/asset-catalogue-migration.md) | Done |
| entity-template-migration | Dual-register all 9 `entity_template_editor.*` WebUIBridge handlers | [entity-template-migration.md](../../features/cluicheeditor/diaeditorapi/entity-template-migration.md) | Done |
| scene-editor-scriptable | Dual-register 35 `scene_editor.*` handlers + 5 new actions (`get_entities`, `place_entity`, `remove_entity`, `create_scene`, `create_asset`) | [scene-editor-scriptable.md](../../features/cluicheeditor/diaeditorapi/scene-editor-scriptable.md) | Done |
| plugin-browser-migration | Register missing `plugin_browser.get_available`; fix deregistration; audit existing descriptors | [plugin-browser-migration.md](../../features/cluicheeditor/diaeditorapi/plugin-browser-migration.md) | Done |
| live-inspector-migration | Migrate `live.connect/disconnect/getStatus/transitionTo/shutdown` from DiaApplicationFlowInspectorPlugin | TBD | — |
| app-flow-editor-migration | Migrate `manifest.*`, `history.*`, `validation.*`, `types.*`, `risk.*` from DiaApplicationFlowEditorPlugin | TBD | — |
| asset-runtime-inspector-migration | Migrate `asset_runtime_inspector.*` from DiaAssetRuntimeInspectorPlugin | TBD | — |
| entity-inspector-migration | Migrate `entity_inspector.*` handlers from DiaEntityInspectorPlugin controllers | TBD | — |
| pipeline-migration | Migrate `pipeline.*` from DiaPipelineEditorPlugin — deferred; callable via DiaCLI from Python already | TBD | Deferred |

## Status

`In Progress` — plan: @docs/specs/systems/cluicheeditor/diaeditorapi.plan.md
