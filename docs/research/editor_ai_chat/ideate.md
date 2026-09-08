# Research: Ideate — Editor AI Chat Plugin

**Input:** docs/research/editor_ai_chat/explore.md

## Candidates

### Candidate 1: Minimal Chat Shell
**Home module/system:** New `DiaChatPlugin` editor plugin (CluicheEditor)
**Size:** S
**Description:** A dockable React panel with a message input, conversation list, and streaming token display. Connects to Ollama only via DiaPython (`ollama` library). No tool calling — the AI answers questions but cannot execute editor actions. Includes model discovery (pulls `/api/tags` to show available local models) and graceful "Ollama not running" degradation state.

This is the thinnest possible slice that proves the UI pipeline: streaming tokens from a background Python thread into a CEF React panel. It does not depend on DiaEditorAPI Phase 1 or Phase 2 at all.

**Primary value:** Proves the streaming IPC path from DiaPython → C++ main thread → CEF JS without building anything irreversible.

---

### Candidate 2: Python-Orchestrated Chat + Direct Tool Execution
**Home module/system:** New `DiaChatPlugin` editor plugin + thin Python orchestrator script
**Size:** M
**Description:** DiaPython hosts a Python orchestrator (`dia_chat.py`) that manages the full LLM conversation loop: sends messages with tool definitions derived from DiaEditorAPI's manifest, receives `tool_call` responses, executes them via a direct C++ bridge to `DiaEditorAPI::ExecuteAction()`, feeds results back to the LLM, and streams the final reply to the React panel.

Backend abstraction uses the OpenAI-compatible API endpoint, which Ollama exposes at `/v1/chat/completions`. Claude is supported via `anthropic` SDK (thin adapter); Gemini via `google-generativeai` (thin adapter). Switching backends is a config change, not a code change. Tool definitions are derived at startup from the DiaEditorAPI manifest — no hardcoding.

Single-step tool calls only (one round of tool use per user message). Includes inline tool call display in the chat UI (shows "→ calling `entity.create`" with params before the final reply). Depends on DiaEditorAPI Phase 1 (`ExecuteAction()` available) but not Phase 2 (no MCP server needed).

**Primary value:** Full tool-augmented chat inside CluicheEditor. User can say "open project X" and it happens. Ollama-first, cloud backends available as fallback with no architecture change.

---

### Candidate 3: C++-Native LLM Client
**Home module/system:** New `DiaLLMClient` Dia module + `DiaChatPlugin` editor plugin
**Size:** L
**Description:** Write a bespoke C++ HTTP streaming client for LLM backends. Implements SSE parsing for Ollama's `/api/chat` and `/v1/chat/completions` streams, tool call JSON parsing, and a backend interface (`ILLMBackend`) in C++. DiaPython dependency removed from the critical path.

Tighter editor integration (no Python GIL contention, no subprocess startup cost), but writing a correct streaming HTTP client with chunked-transfer decoding, tool call JSON accumulation, and error recovery in C++ is substantial engineering effort — especially for three different API formats. Would use a C++ HTTP library (e.g. libcurl or WinHTTP) not currently in the dependency tree.

**Primary value:** Maximum integration depth; no Python runtime in the call chain for LLM interactions. Mainly justified if DiaPython proves too slow or unreliable for streaming.

---

### Candidate 4: Multi-Step Agentic Loop
**Home module/system:** New `DiaChatPlugin` editor plugin (extends C2)
**Size:** L
**Description:** Extends C2 with a planning/execution loop: the AI can chain multiple tool calls in a single user request ("create entity Hero, add SpriteComponent, set position to 100,200"). The orchestrator executes tool calls in sequence, feeding each result back before deciding the next step, up to a configurable `max_steps` guard (default 10). Includes a user approval gate — before executing any destructive action, the panel shows a confirmation card.

Requires more sophisticated prompt engineering (system prompt must explain the multi-step pattern), a step counter, and UI for showing a running action log. Also requires that DiaEditorAPI actions have a `destructive` flag so the approval gate knows what to guard.

**Primary value:** "Do X for me" workflows instead of just "what is X" answers. Meaningful for complex editor tasks that currently require many manual steps.

---

### Candidate 5: Knowledge Context System (Standalone)
**Home module/system:** No new module — documentation + runtime loader utility
**Size:** S
**Description:** Focused entirely on the AI knowledge problem, independent of the chat plugin itself. Three deliverables: (1) audit of existing `docs/` tree to identify what's useful AI context vs. what's too verbose/stale, (2) a set of curated `ai_context/` knowledge files (`engine_overview.md`, `module_apis.md`, `editor_actions.md`, `coding_conventions.md`) written specifically for LLM consumption (dense, factual, no padding), and (3) an authoring guide (`docs/reference/ai-guides/knowledge-authoring.md`) that defines how new files are written, when they're updated, and how they fit into the system prompt budget.

Runtime side: a utility that assembles the system prompt from the knowledge files at chat startup, measuring token count against a configurable budget and trimming lower-priority files if needed.

This work is useful regardless of which chat plugin candidate is chosen, and regardless of whether the backend is Ollama or Claude. It is also the most likely thing to be skipped if bundled into a larger work item.

**Primary value:** The AI gives accurate, project-aware answers. Without this, all chat plugin candidates produce generic or hallucinated advice.

---

### Candidate 6: Bundled: Chat Plugin + Knowledge System + Tool Transparency UI
**Home module/system:** New `DiaChatPlugin` editor plugin + knowledge context utility
**Size:** L
**Description:** C2 (Python-orchestrated chat + direct tool execution) combined with C5 (knowledge context system). The single work item that delivers a complete, usable AI assistant in CluicheEditor.

Adds a polished tool transparency UX to C2: each tool call is shown inline in the conversation as a collapsible card (`→ entity.create {name: "Hero", template: "base"}`) with a result badge (✓ / ✗). The system prompt is assembled dynamically from the `ai_context/` files at plugin startup with token budget management. Backend selector (Ollama / Claude / Gemini) is a dropdown in the panel header, with the active model shown. Session history is persisted per `.cluicheproj` project file so conversations survive editor restarts.

Depends on DiaEditorAPI Phase 1. Does not need Phase 2. Ships before external MCP agents — the chat plugin is the first real user of the tool surface.

**Primary value:** The complete editor AI experience in one deliverable: accurate answers, editor action execution, and a UI that makes the AI's behaviour transparent and trustworthy.

---

### Candidate 7: Backend Abstraction Module First
**Home module/system:** New `DiaLLMBackend` Dia module, then `DiaChatPlugin` consumes it
**Size:** M
**Description:** Build a reusable `DiaLLMBackend` Dia module first — a C++/Python hybrid that defines `ILLMBackend` (send messages, receive streaming tokens, define tools, receive tool calls) with concrete implementations for Ollama, Claude, and Gemini. The chat plugin then becomes thin: it owns the UI and marshaling but delegates all LLM logic to `DiaLLMBackend`.

The module is reusable by future systems (e.g. a scene generation assistant, an AI-driven test harness) without re-implementing the backend glue. But it adds a speccing and scaffolding step before the chat plugin can be built, and there are currently no other known consumers to justify the extraction.

**Primary value:** Reusable LLM backend abstraction for the whole platform. Mainly justified if a second consumer is already on the roadmap.

---

## Coverage Map

The candidates span the design axes from explore.md as follows:

| Axis | C1 | C2 | C3 | C4 | C5 | C6 | C7 |
|------|----|----|----|----|----|----|-----|
| LLM client placement | Python | Python | C++ | Python | N/A | Python | C++/Python hybrid |
| Tool execution | None | Direct C++ | Direct C++ | Direct C++ | N/A | Direct C++ | Direct C++ |
| Backend abstraction | Ollama only | OpenAI-compat shim | ILLMBackend C++ | OpenAI-compat | N/A | OpenAI-compat | Dia module |
| Knowledge context | None | Minimal | None | Minimal | Full | Full | Minimal |
| Streaming UX | Yes | Yes | Yes | Yes | N/A | Yes (polished) | Yes |
| Tool display | None | Inline | Inline | Log view | N/A | Collapsible cards | Inline |
| Multi-step | No | No | No | Yes | N/A | No | No |
| Scope | Narrow | Sweet spot | Heavy | Extended | Focused | Complete | Modular |

Size range: S (C1, C5) → M (C2, C7) → L (C3, C4, C6). C2 and C6 are the natural pivot points — C2 is the minimum useful feature, C6 is the complete story.
