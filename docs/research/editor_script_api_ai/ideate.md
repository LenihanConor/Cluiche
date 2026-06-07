# Research: Ideate — Editor Script API for Automation and AI Tooling

**Input:** docs/research/editor_script_api_ai/explore.md

## The DiaAPI Boundary Question

Before candidates: every design here must take a stance on where DiaAPI ends and the new layer begins.

**DiaAPI is:** a low-level command registry. It knows about command names, args, categories, versions, and exit codes. It has no concept of editor, plugins, UI, or agent interaction. It lives in Dia (engine layer).

**What we are building is:** an editor-level automation contract. It knows about projects, plugins, panel state, AI agents, and Python scripts. It lives at or above DiaEditor.

These are different things. The right relationship is:
- DiaAPI = infrastructure (dispatch, discovery, JSON envelope, Python auto-gen)
- New layer = consumer of DiaAPI + peer of WebUIBridge, adds the editor action contract on top

The new layer has a name: candidates below use `DiaEditorAPI` as a working title, but naming is a design decision. The key distinction is: **DiaAPI is the plumbing; DiaEditorAPI is the contract.**

---

## Candidates

### Candidate 1: DiaEditorAPI — Dual-Registration Action Layer

**Home module/system:** New module `DiaEditorAPI` (above DiaEditor, depends on DiaAPI + DiaEditor)
**Size:** M

**Description:**
A thin registration layer that lives above both DiaAPI and WebUIBridge. Plugin authors call `RegisterScriptableAction()` once — the layer cross-registers with DiaAPI's JSON path (for script dispatch) and WebUIBridge (for JS/CEF dispatch) simultaneously. A thread-safe command queue on the main processing unit drains script calls on the correct thread. Deregistration is tied to plugin lifecycle so stale commands are impossible.

The layer owns a JSON schema manifest (one entry per action: name, description, params with types + required flags + descriptions). This manifest is the AI tool manifest — it can be served to Ollama as a function-calling schema directly.

**Primary value:** One registration, three callsites (script, UI, AI). Full editor action coverage with no drift between what the UI can do and what scripts can do.

**DiaAPI boundary:** DiaAPI provides the JSON dispatch mechanism and Python auto-gen. DiaEditorAPI owns the editor action contract. DiaAPI never knows about editors or plugins.

---

### Candidate 2: DiaEditorAPI — WebSocket Script Server

**Home module/system:** New module `DiaEditorScriptServer` (depends on DiaWebSocket + DiaEditor)
**Size:** M

**Description:**
A WebSocket server running inside CluicheEditor that accepts JSON-RPC 2.0 messages. External processes (Python scripts, AI agents, test runners) connect and call editor actions by name. The server maps incoming method names to WebUIBridge request handlers and DiaAPI JSON commands via a unified dispatch table. No CEF involvement — the server runs on its own thread with an explicit marshal queue to the main thread.

Python client library is hand-written (thin wrapper over `websocket-client`). AI agents call it directly as an MCP tool or via Python. Manifest is served on a special `rpc.describe` method call — the client or AI fetches it once on connect.

**Primary value:** Fully out-of-process. Editor can be automated from any language, not just Python. AI agents and external test runners connect without being embedded in the editor binary. Headless-friendly.

**DiaAPI boundary:** DiaAPI commands are registered as one subset of the dispatch table. The script server is a separate surface that happens to include DiaAPI coverage. DiaAPI is not the protocol — JSON-RPC 2.0 is.

---

### Candidate 3: Extend DiaAPI — Make It the Universal Layer

**Home module/system:** DiaAPI (extend existing module)
**Size:** L

**Description:**
Extend DiaAPI to support plugin-owned command groups with lifecycle hooks (register/deregister on plugin load/unload), a schema per command (typed params + descriptions, not just name/description/callback), and a thread-dispatch policy per command (which thread to marshal to). All editor plugin actions migrate to DiaAPI. WebUIBridge is relegated to UI-only push notifications — no more request handlers there.

DiaPython auto-gen picks up everything automatically. The AI manifest is generated from the command registry schema fields. No new module needed — DiaAPI grows into the universal action layer.

**Primary value:** Single registry, zero drift by construction. Everything is in one place. No "which API does this action live in?" ambiguity.

**DiaAPI boundary:** There is no separate EditorAPI. DiaAPI absorbs the editor contract. This is the "consolidate fully" option.

**Watch out for:** DiaAPI is currently an engine-layer module with no dependency on DiaEditor. Pulling editor concepts up into it creates a dependency inversion. Either DiaAPI grows an optional editor extension point (complex) or the dependency direction breaks (bad). This is the core risk of full consolidation.

---

### Candidate 4: DiaAgentAPI — AI-First Interface Only

**Home module/system:** New module `DiaAgentAPI` (depends on DiaEditor + DiaPython + DiaWebSocket)
**Size:** M

**Description:**
Skip general scripting. Build specifically for AI agent interaction: an MCP server running inside CluicheEditor (or as a sidecar process) that exposes editor actions as MCP tools. Each tool has a name, description, and typed input schema. Ollama (or Claude, or any MCP-compatible model) can call tools directly. Python scripts call them via the MCP SDK.

The MCP server covers the same action set as Candidate 1/2 but the protocol is MCP, not raw JSON-RPC. Discovery is via `tools/list`. Tool calls are via `tools/call`. Results are structured content blocks.

Automation test scripts use the MCP Python SDK — same protocol as the AI, no separate test API.

**Primary value:** AI-native from day one. No custom protocol to design. Claude and Ollama work immediately. Test scripts and AI agents share one surface.

**DiaAPI boundary:** MCP server internally calls DiaAPI JSON commands and WebUIBridge handlers. DiaAPI is invisible to external callers — MCP is the contract.

---

### Candidate 5: DiaEditorAPI — Layered (Script + AI as separate consumers)

**Home module/system:** New module `DiaEditorAPI` with two consumer adapters
**Size:** L

**Description:**
A three-layer design:
1. **Action registry** (C++): `DiaEditorAPI` owns a registry of `EditorAction` descriptors (name, description, param schema, thread policy, handler). Plugin authors register here. Single source of truth.
2. **Script adapter** (Python): auto-generated Python module `dia_editor` with one function per registered action. Callable from test scripts or automation harness.
3. **AI adapter** (MCP or JSON-RPC): serves the action registry as an MCP tool manifest or OpenAI-compatible function list. Ollama/Claude read this to know what they can do.

WebUIBridge is kept for push notifications only (UI ↔ C++ events). All request/response moves to DiaEditorAPI. DiaAPI remains the low-level plumbing.

**Primary value:** Clean separation of concerns. The registry is the single source of truth; adapters are swappable. Adding a new AI framework means writing a new adapter, not touching the registry.

**DiaAPI boundary:** DiaAPI is the JSON dispatch mechanism that DiaEditorAPI's action registry invokes internally. DiaAPI handles the envelope; DiaEditorAPI handles the contract.

---

### Candidate 6: Minimal — DiaAPI JSON + Generated Python Stubs Only

**Home module/system:** DiaAPI (extend) + DiaPython (extend)
**Size:** S

**Description:**
Don't build a new module. Instead: (1) migrate the most automation-critical WebUIBridge handlers (`project.open_path`, `project.close`, `game_connection.connect/disconnect`, `plugin.load/unload`) to DiaAPI JSON path — about 8 handlers; (2) extend `InitializePythonBindings` to also emit a `.pyi` stub file listing all commands with their param shapes; (3) write a hand-authored `dia_editor.py` facade that imports the auto-gen stubs and provides friendly composite operations. AI agents call `dia_editor.py` functions via DiaPython.

No new module. No protocol design. No thread marshaling framework. Just fill the most important gap (project + connection actions missing from DiaAPI) and generate a usable Python surface.

**Primary value:** Shippable in a week. Gets automation unblocked immediately. Can be grown into Candidate 1 or 5 later without throwaway work.

**DiaAPI boundary:** DiaAPI grows slightly (8 new commands). No architectural change.

---

### Candidate 7: DiaEditorAPI — Headless Automation Mode

**Home module/system:** New module `DiaEditorHeadless` + `DiaEditorAPI`
**Size:** XL

**Description:**
A mode where CluicheEditor launches without CEF/UI rendering, exposes a WebSocket script server (Candidate 2), and runs automation scripts against a fully live editor state machine. Scripts can open projects, trigger pipelines, navigate stages, validate checkpoints, and shut down — all without a visible window. CI pipelines run this mode. AI agents use it for batch workflows ("build and deploy all projects in the workspace").

Requires: headless CEF guard, script server (Candidate 2), DiaAutomation integration, and a process supervisor.

**Primary value:** Enables CI automation and AI batch workflows that don't need a human at the keyboard. Unlocks use cases Candidates 1–6 can't reach.

**DiaAPI boundary:** Same as Candidate 2. DiaAPI is one of several dispatch backends.

---

## Coverage Map

| Candidate | Scope | DiaAPI role | AI-ready | Size |
|-----------|-------|-------------|----------|------|
| 1. DiaEditorAPI Dual-Reg | Full editor actions | Dispatch backend | Via manifest | M |
| 2. WebSocket Script Server | Full editor actions | One dispatch backend | Via JSON-RPC | M |
| 3. Extend DiaAPI | Full (DiaAPI absorbs all) | The layer itself | Via schema fields | L |
| 4. DiaAgentAPI MCP-first | Full editor actions | Internal only | Native MCP | M |
| 5. Layered Registry | Full, three-tier | Dispatch backend | Via adapter | L |
| 6. Minimal Stubs | ~60% (critical path only) | Slightly extended | Via Python facade | S |
| 7. Headless Mode | Full + CI/batch | One dispatch backend | Via WebSocket | XL |

Candidates span: cheap+partial (C6) → complete+in-process (C1, C5) → complete+out-of-process (C2, C4) → complete+absorb-all (C3) → complete+headless (C7). The DiaAPI consolidation question is answered differently by each: C3 says yes fully, C6 says partially, all others say no — keep DiaAPI as plumbing only.
