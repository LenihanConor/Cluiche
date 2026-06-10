# System Spec: DiaChatPlugin

## Parent Application
@docs/specs/applications/cluicheeditor.md

**Research:** @docs/research/editor_ai_chat/summary.md
**UI Mockup:** @docs/research/editor_ai_chat/mockup_c4_hybrid.html

## Purpose

DiaChatPlugin is a dockable CluicheEditor panel that gives developers a conversational AI assistant embedded directly in the editor. The user types natural-language messages; the plugin sends them to an LLM backend (Ollama, Claude, or Gemini) with tool definitions derived from the live `DiaEditorAPI` action registry; the LLM optionally calls tools (editor actions executed directly via `DiaEditorAPI::ExecuteAction()`); the final reply streams back into the chat panel.

The plugin also owns the knowledge context system: a set of curated `.md` files written specifically for LLM consumption that ground the AI in accurate Dia engine knowledge, plus an authoring guide that defines how those files are maintained.

**Target users:** Game programmers and engine developers who want to ask questions about the codebase, navigate the editor faster via natural language, or trigger editor actions (open file, connect to game, run pipeline) conversationally.

## Responsibilities

- Own the `DiaChatPlugin` `IEditorPlugin` subclass: registration, lifecycle (`OnLoad` / `OnUnload`), dockable panel UI (`dia://chat/`)
- Host a `ChatOrchestrator` Python module (`dia_chat.py`) via DiaPython that manages the full LLM conversation loop: build message history, attach tool definitions from DiaEditorAPI manifest, send to backend, receive streaming tokens + tool calls, dispatch tool calls, feed results back
- Own the `ILLMBackend` Python abstraction and three concrete implementations: `OllamaBackend` (primary), `ClaudeBackend`, `GeminiBackend`
- Own the knowledge context system: curated `ai_context/` files and the `KnowledgeLoader` utility that assembles and token-budgets the system prompt at plugin startup
- Bridge streaming tokens from DiaPython background thread → C++ main thread → CEF JS via the editor's WebUIBridge push path
- Expose context controls to the UI: per-session chip toggles, `@file` per-message injection, Tools-only mode toggle
- Persist conversation history per `.cluicheproj` project file across editor restarts
- Provide graceful degradation when no backend is reachable (clear panel state, actionable error message)

## Not Responsible For

- `DiaEditorAPI` action registry or `ExecuteAction()` implementation — DiaChatPlugin is a consumer, not an owner
- The MCP server on port 7777 — that is DiaEditorAPI Phase 2; this plugin does not use it
- Model fine-tuning, prompt engineering beyond the system prompt, or RAG / vector search — static curated files cover the initial knowledge need
- Multi-step agentic planning loops — single tool call round per user message is Phase 1; chaining is Phase 2 (see below)
- Any UI outside the chat panel — layout management, other plugins, docking chrome

## Architecture

### Overview

```
User (React chat panel)
        ↓ message + context mode
ChatPanelBridge (C++ WebUIBridge handler)
        ↓ marshals to DiaPython thread
ChatOrchestrator (dia_chat.py, Python)
        ├─ KnowledgeLoader  → assembles system prompt from ai_context/ files
        ├─ ILLMBackend      → sends messages, streams tokens, receives tool calls
        │    ├─ OllamaBackend   (OpenAI-compat /v1/chat/completions)
        │    ├─ ClaudeBackend   (anthropic SDK)
        │    └─ GeminiBackend   (google-generativeai SDK)
        └─ ToolDispatcher   → calls DiaEditorAPI::ExecuteAction() for each tool call
                ↓ result JSON
        ChatOrchestrator feeds result back to LLM → next token stream
        ↓ streaming tokens / final reply
ChatPanelBridge → pushes to CEF JS → React updates DOM
```

### IEditorPlugin Subclass

```cpp
class DiaChatPlugin : public Dia::Editor::IEditorPlugin {
public:
    static constexpr StringCRC kPluginId = StringCRC("DiaChatPlugin");

    bool        OnLoad(Dia::Editor::EditorModel& model) override;
    void        OnUnload() override;
    StringCRC   GetPluginId() const override { return kPluginId; }
    const char* GetUIPath() const override   { return "dia://chat/"; }
};
```

`OnLoad` initialises `ChatPanelBridge`, loads `dia_chat.py` via DiaPython, calls `KnowledgeLoader::Load()` to assemble the initial system prompt, and registers WebUIBridge handlers for `chat.send_message`, `chat.set_backend`, `chat.set_context_mode`, `chat.add_context_file`, `chat.clear_history`.

### ChatOrchestrator (Python)

`dia_chat.py` owns the conversation loop. Each call to `send_message(text, context_mode, extra_files)`:

1. Assembles messages list (system prompt + history + new user message)
2. Fetches tool definitions from `DiaEditorAPI.GetManifest()` (via DiaPython binding) — skipped in `tools_only` mode; full manifest in `full_context` mode
3. Calls `ILLMBackend.stream_chat(messages, tools)` — yields token chunks and tool call events
4. For each `tool_call` event: calls `ToolDispatcher.dispatch(name, params)` → `DiaEditorAPI::ExecuteAction()` → feeds result back as a `tool` role message
5. Streams final reply tokens to C++ via callback registered at startup
6. Appends completed exchange to `ConversationHistory`

Max tool call depth per message: **8** (configurable, guards against loops).

### ILLMBackend (Python)

```python
class ILLMBackend:
    def stream_chat(self, messages: list, tools: list) -> Iterator[ChatEvent]: ...
    def list_models(self) -> list[str]: ...
    def is_available(self) -> bool: ...
```

`ChatEvent` is a tagged union: `TokenChunk | ToolCall | Done | Error`.

- **OllamaBackend** — calls `/v1/chat/completions` (OpenAI-compat); uses `ollama` Python library or raw `httpx` for streaming; `list_models()` calls `/api/tags`
- **ClaudeBackend** — uses `anthropic` SDK; maps tool definitions to Anthropic `tool_use` format
- **GeminiBackend** — uses `google-generativeai` SDK; maps tool definitions to Gemini function calling format

Backend selection is a runtime config — stored in editor settings, switchable via panel dropdown without restart.

### ToolDispatcher (Python)

```python
class ToolDispatcher:
    def dispatch(self, name: str, params: dict) -> dict:
        # Calls DiaEditorAPI::ExecuteAction() via DiaPython binding
        # Returns result dict; raises ToolError on timeout or action-not-found
```

Calls `dia_editor_api.execute_action(name, params)` — a DiaPython binding to `DiaEditorAPI::ExecuteAction()`. This is a synchronous blocking call on the Python side; the C++ side uses the existing `EditorActionQueue` frame-drain with 5s timeout (per EAPI-004).

### KnowledgeLoader

Reads `ai_context/` files at plugin startup. Assembles a system prompt by concatenating files in priority order until the token budget is reached (budget is configurable, default 4 096 tokens for Ollama, 16 384 for Claude/Gemini). Lower-priority files are trimmed first.

Priority order (highest first):
1. `editor_actions.md` — live tool definitions (always included; regenerated from DiaEditorAPI manifest at startup)
2. `data_types.md` — asset and component schemas (always included; regenerated from DiaEditorAPI data type registry at startup)
3. `engine_overview.md` — platform and architecture summary
4. `editor_workflows.md` — common action sequences, typical task flows
5. `asset_style_guide.md` — field/asset naming conventions, JSON shape conventions

Token counting uses a simple word-based estimate (÷ 0.75) — sufficient for budget management; no tokeniser dependency.

### Streaming Bridge

Streaming tokens arrive on a DiaPython background thread. They cannot touch CEF directly. Flow:

```
Python thread: callback(token_chunk)
  → C++ ChatPanelBridge::OnTokenChunk()         [thread-safe queue push]

Main thread (30hz EditorPU tick):
  ChatPanelBridge::DoUpdate()
    → drain token queue
    → WebUIBridge::PushToJS("chat.token", {text, done})
      → React appends token to current message bubble
```

Tool call cards (start, status, result) are pushed as discrete events: `chat.tool_start`, `chat.tool_result`, `chat.tool_error`.

### Context Controls

Three mechanisms, all surfaced in the React panel:

| Control | Scope | Behaviour |
|---------|-------|-----------|
| **Context chips** | Per-session | Toggle individual `ai_context/` files in/out of the system prompt for the rest of the session |
| **@file injection** | Per-message | Type `@filename.md` in the input; that file is appended to the message context for that turn only; does not persist |
| **Mode toggle** | Per-message | `Full context` (system prompt + tools) · `Tools only` (tools only, no knowledge files) · `Custom` (chip selection) |

### Conversation Persistence

History stored as JSON in the project's `Cluiche/out/<AppName>/` directory (per PD-009):

```
Cluiche/out/CluicheEditor/chat/<project-slug>/history.jsonl
```

Each line is one completed exchange: `{role, content, tool_calls[], timestamp}`. Loaded at plugin startup if the file exists. Cleared on explicit "New conversation" action.

### Knowledge Context System

**Files** live at `Cluiche/Assets/CluicheEditor/ai_context/`:

| File | Content | Authored | Target token size |
|------|---------|----------|-------------------|
| `editor_actions.md` | Auto-generated from DiaEditorAPI action registry at startup | Auto-gen | ~400 tok |
| `data_types.md` | Auto-generated from DiaEditorAPI data type registry at startup — asset descriptors, component field schemas, JSON payload shapes for all plugin-owned types | Auto-gen | ~600 tok |
| `engine_overview.md` | Platform summary, application list, threading model, key patterns | Hand-authored | ~600 tok |
| `editor_workflows.md` | Common action sequences and typical task flows (e.g. open project → load scene → place entity) | Hand-authored | ~400 tok |
| `asset_style_guide.md` | Field/asset naming conventions, JSON shape conventions | Hand-authored | ~200 tok |

**Authoring guide:** `docs/reference/ai-guides/knowledge-authoring.md` — defines format rules (dense, factual, no padding), update triggers (when module API changes, when new actions are registered), token budget per file, and validation checklist.

### Destructive Action Confirmation

Any action with `EditorActionDescriptor::destructive = true` requires explicit user confirmation before dispatch. The AI cannot proceed without it.

Flow:
```
AI decides to call a destructive action
  → ChatOrchestrator detects destructive = true in manifest
  → pauses tool call dispatch
  → pushes chat.confirm_required event to UI
    { call_id, fn, params, description }

User clicks Confirm or Cancel in panel
  → chat.confirm_response { call_id, confirmed: bool }
  → if confirmed: dispatch proceeds, result fed back to AI
  → if cancelled: AI receives { error: "User cancelled action" }, reports to user
```

The confirmation card shows the action name, a plain-English description, and the resolved parameter values — not raw JSON.

### Context Window Indicator

When the assembled context (system prompt + history) exceeds 75% of the backend's context window, the panel displays a subtle warning: *"Context window near limit — early messages may be trimmed."* At 90%, older history entries are summarised automatically by a lightweight prompt before the next send. The indicator is visible but non-blocking — the user can continue chatting.

### Empty State / First Run

When no conversation history exists for the current project, the panel shows:

> **Hello! Ask me anything about Dia.**
> *Try: "What scenes are in this project?" or "Place a player entity at the origin."*

The suggested prompts are static but chosen to exercise the two most common workflows (query + action). They disappear on first message.

### Graceful Degradation

| Condition | Panel state |
|-----------|-------------|
| Ollama not running | Warning banner: "Ollama not detected at localhost:11434. Start Ollama or switch backend." |
| No models installed | Warning: "No models found. Run `ollama pull llama3.2`." |
| API key missing (Claude/Gemini) | Warning: "ANTHROPIC_API_KEY / GOOGLE_API_KEY not set." |
| DiaEditorAPI Phase 1 not loaded | Tools unavailable; chat works in knowledge-only mode |
| Tool call timeout (>5s) | Inline error card in conversation; AI receives error result and reports to user |
| Backend drops mid-stream | Partial reply shown with "[connection lost]" suffix; retry button re-sends the last user message |
| Destructive action cancelled | Inline card: "Action cancelled by user"; AI acknowledges and continues conversation |

## Public API

```cpp
// Plugin registration (via REGISTER_EDITOR_PLUGIN macro)
class DiaChatPlugin : public Dia::Editor::IEditorPlugin;

// WebUIBridge handlers registered by DiaChatPlugin::OnLoad()
// "chat.send_message"    { text, context_mode, extra_files[] }
// "chat.set_backend"     { backend: "ollama"|"claude"|"gemini", model }
// "chat.set_context_mode"{ mode: "full"|"tools_only"|"custom" }
// "chat.add_context_file"{ path }
// "chat.clear_history"   {}

// WebUIBridge push events (C++ → JS)
// "chat.token"           { text, done }
// "chat.tool_start"        { call_id, fn, params }
// "chat.tool_result"       { call_id, result, duration_ms }
// "chat.tool_error"        { call_id, error }
// "chat.confirm_required"  { call_id, fn, params, description }
// "chat.error"             { message }
// "chat.backend_status"    { backend, model, available }
// "chat.context_warning"   { used_tokens, budget_tokens, pct }

// WebUIBridge handlers (JS → C++) — confirmation response
// "chat.confirm_response"  { call_id, confirmed: bool }
```

## Phased Delivery

### Phase 1 — Core Chat (this spec)
1. `DiaChatPlugin` IEditorPlugin scaffold + WebUIBridge handlers
2. `ChatPanelBridge` streaming token queue + DoUpdate drain
3. `ChatOrchestrator` (`dia_chat.py`) — message loop, single tool call round
4. `OllamaBackend` — OpenAI-compat streaming, model discovery
5. `ToolDispatcher` — `DiaEditorAPI::ExecuteAction()` binding + destructive confirmation gate
6. `KnowledgeLoader` — system prompt assembly, token budget management
7. React chat panel UI — hybrid layout per `mockup_c4_hybrid.html`; empty state; context window indicator
8. Context controls — chips, @file injection, mode toggle
9. `ClaudeBackend` + `GeminiBackend` adapters
10. Knowledge context files (`ai_context/`) + authoring guide
11. Conversation persistence (history.jsonl per project)
12. Graceful degradation states (including mid-stream retry, destructive cancel)

### Phase 2 — Multi-Step Agentic Loop (future spec)
- Tool call chaining (up to configurable `max_steps`)
- Step log view in detail panel

## Dependencies

| Module | Role |
|--------|------|
| DiaEditorAPI | `ExecuteAction()` for tool dispatch; `GetManifest()` for tool definitions; `GetDataTypeRegistry()` for `data_types.md` generation; requires Phase 1 + data-type-registry feature |
| DiaEditor / IEditorPlugin | Plugin base class, `OnLoad` / `OnUnload`, WebUIBridge push |
| DiaPython | Hosts `dia_chat.py`; provides `AddFunction()` binding for `execute_action` |
| DiaUICEF / React | Chat panel UI rendered in CEF; receives streamed tokens via WebUIBridge push |
| DiaApplicationFlow | `ChatPanelBridge` is a Module on EditorPU; `DoUpdate` drains the token queue |
| DiaCore / DiaIO | Reading `ai_context/` files; history.jsonl read/write |

**External Python (via DiaPython environment):**
- `ollama` or `httpx` — Ollama streaming
- `anthropic` — Claude API
- `google-generativeai` — Gemini API

## Inherited Binding Decisions

| Source | ID | Decision | Impact on DiaChatPlugin |
|--------|----|----------|------------------------|
| Platform | PD-001 | StringCRC for all IDs | `kPluginId`, WebUIBridge event names, backend IDs all use StringCRC constants |
| Platform | PD-002 | PU/Phase/Module architecture | `ChatPanelBridge` is a Module on EditorPU; token drain runs in `DoUpdate` |
| Platform | PD-004 | No STL in public APIs | Message history and tool definition lists use DiaCore containers in any public C++ surface; Python internals may use native Python types |
| Platform | PD-007 | C++20 required | `ChatPanelBridge` uses designated initialisers, `std::span` where applicable |
| Platform | PD-009 | Generated output under `Cluiche/out/` | Conversation history written to `Cluiche/out/CluicheEditor/chat/` |
| CluicheEditor | AED-001 | DiaEditor is a pure library; CluicheEditor owns app flow | `DiaChatPlugin` and `ChatPanelBridge` live in CluicheEditor, not DiaEditor |
| CluicheEditor | AED-003 | Each system owns its editor as `<System>/Editor/` | Plugin lives at `CluicheGameBaseline/Plugins/DiaChatPlugin/` or `CluicheEditor/Plugins/DiaChatPlugin/` |
| CluicheEditor | AED-005 | React + DiaUICEF for UI | Chat panel is a React/TypeScript component served via `dia://chat/`; no alternative UI stack |
| DiaEditorAPI | EAPI-003 | AI adapter is protocol-agnostic; MCP is the first serialiser | DiaChatPlugin reads the DiaEditorAPI manifest directly and builds its own tool definitions per backend — it does not use the MCP server |
| DiaEditorAPI | EAPI-004 | Thread dispatch policy per action | Tool calls from the Python thread block on a future; the 5s timeout and frame-drain are owned by DiaEditorAPI, not this plugin |
| DiaEditorAPI | EAPI-005 | CEF-entangled actions excluded | `kCEFOnly` actions will not appear in `GetManifest()` tool list; plugin does not need to filter them |

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| DCP-001 | DiaPython hosts the LLM orchestrator; no bespoke C++ HTTP streaming client | Python `ollama`, `anthropic`, `google-generativeai` libraries cover all three backends in ~50 lines; writing a correct C++ SSE streaming client is significant engineering effort with no benefit until Python proves measurably too slow | LLM client | Accepted | Yes |
| DCP-002 | Tool calls route directly to `DiaEditorAPI::ExecuteAction()` — no MCP roundtrip | Cloud backends (Claude, Gemini) cannot call localhost:7777; the plugin must orchestrate tool calls regardless; routing Ollama via MCP would be a self-connection to localhost with no gain | Tool execution | Accepted | Yes |
| DCP-003 | Ollama is the primary backend; Claude and Gemini are supported via thin adapters with no architecture change | Ollama is free, local, and controllable; cloud backends are fallback when local model quality is insufficient; switching is a config change, not a code change | Backend selection | Accepted | Yes |
| DCP-004 | Knowledge files are curated, dense, LLM-optimised `.md` files — no RAG, no vector store | Static files are predictable, versionable, and sufficient for a codebase of this size; RAG adds a vector store dependency and retrieval latency with marginal benefit at <50k tokens total | Knowledge system | Accepted | Yes |
| DCP-005 | `editor_actions.md` is auto-generated from the DiaEditorAPI manifest at plugin startup — never hand-authored | Hand-authored action docs drift as actions are added or renamed; the registry is the source of truth per EAPI-002 | Knowledge system | Accepted | Yes |
| DCP-006 | Streaming tokens bridge via `ChatPanelBridge::DoUpdate()` on the main thread — no direct CEF call from Python thread | CEF JS calls must come from the browser process thread; bridging via the frame-drain queue is the established pattern in this codebase | Streaming | Accepted | Yes |
| DCP-007 | Max tool call depth per message is 8 (configurable) | Guards against LLM tool-call loops without requiring multi-step planning logic; 8 is sufficient for any realistic single-turn action sequence | Safety | Accepted | Yes |
| DCP-008 | Conversation history persisted as JSONL under `Cluiche/out/CluicheEditor/chat/` per PD-009 | Keeps history out of the repo; parallel to other generated output; per-project subdirectory gives clean separation | Persistence | Accepted | Yes |
| DCP-009 | Destructive actions require explicit user confirmation in Phase 1 — not deferred to Phase 2 | The AI can call `scene_editor.remove_entity` and similar actions in Phase 1; a silent destructive dispatch is unacceptable even before multi-step chaining; the confirmation card is a small UI addition that prevents data loss | Safety | Accepted | Yes |

## Open Design Questions

1. **DiaPython streaming latency** — Ollama's Python library uses blocking iteration; if the Python GIL causes perceptible streaming lag (>50ms between tokens), the escape hatch is `DCP-001`'s inverse: a thin C++ HTTP client for Ollama only. Worth measuring on first milestone before committing to Phase 2 scope.

2. **@file injection format** — Should `@filename.md` resolve against `ai_context/` only, or any file the user can navigate to in the manifest editor? Broader scope is more powerful but requires a file picker integration and security consideration (user could inject arbitrary filesystem content into the LLM context).

3. **Tool definition filtering** — DiaEditorAPI `GetManifest()` returns all registered actions. Some may be irrelevant to a chat context (e.g. internal pipeline plumbing). Should DiaChatPlugin filter by category, or expose everything and let the LLM ignore what it doesn't need?

4. **Project name resolution** — `project.open_path` requires a full file path. When a user says "open cluichetest" the AI has a name, not a path. Requires a `project.list` action (returns all discovered `.diagame` files with names and full paths) so the AI can resolve the name at runtime before calling `open_path`. This is the correct pattern — same as `asset_catalogue.get_available` for assets. Static path conventions in a knowledge file are fragile and unverifiable. `project.list` must be added to DiaEditorAPI before conversational project switching is reliable.

5. **AI memory** — The AI has no write-back mechanism; it cannot persist corrections, user preferences, or project conventions across sessions. A writable `user_notes.md` (appended via a `chat.save_note` action, stored alongside `history.jsonl`) would close this gap. Deferred to Phase 2.

## Status

**Status:** Done

**Plan:** @docs/specs/systems/cluicheeditor/diachatplugin.plan.md
