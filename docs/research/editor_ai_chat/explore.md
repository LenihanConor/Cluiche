# Research: Explore — Editor AI Chat Plugin

**Session date:** 2026-06-06
**Folder:** docs/research/editor_ai_chat/

## Problem Space Overview

CluicheEditor needs a first-class AI assistant experience that lives inside the editor as a dockable plugin panel. The DiaEditorAPI system (Phase 2, not yet built) will expose all scriptable editor actions as MCP tool definitions — but that only solves the **tool surface**. What's missing is the **chat orchestrator**: the layer that connects a conversational LLM (Ollama, Claude, Gemini) to that tool surface, renders the conversation in the editor UI, and feeds the AI enough knowledge about the codebase that its answers and actions are accurate.

The core loop is: user types a message → plugin sends it to an LLM with tool definitions available → LLM optionally calls tools (editor actions) → results flow back into the conversation → LLM replies. This is a standard tool-augmented LLM conversation loop, but embedded inside a C++/CEF desktop editor rather than a web app or CLI.

There are three distinct design problems bundled together: (1) which LLM backends to support and how to abstract them, (2) how tool call execution flows from the LLM back into DiaEditorAPI without redundant network hops, and (3) how engine/editor knowledge is packaged into the system prompt so the AI gives accurate, project-aware answers rather than generic game-engine advice.

## Existing Approaches

- **Ollama native tool calling** — Ollama 0.3+ supports tool use in the `/api/chat` endpoint; models like `llama3.2`, `mistral-nemo`, `qwen2.5` have good tool-calling capability; streaming supported via SSE
- **OpenAI-compatible API** — Ollama exposes an OpenAI-compatible endpoint at `/v1/chat/completions`; Claude and Gemini also have compatible adapters; single client code covers all three if you target this format
- **Claude API native** — Anthropic's `tool_use` content blocks; requires API key + internet; strongest tool-calling reliability; no local option
- **Gemini API** — Google's function calling API; similar to Claude; requires API key; good for when user wants cloud quality without Anthropic billing
- **MCP (Model Context Protocol)** — Anthropic's open protocol for LLM ↔ tool server communication; DiaEditorAPI Phase 2 already specced to serve `tools/list` + `tools/call` on port 7777; Ollama can connect to MCP servers natively in recent versions
- **LangChain / LlamaIndex** — Python orchestration frameworks; heavy dependency but handle multi-step agent loops, RAG, context windows well; possible via DiaPython
- **Embedded chat in game editors** — Unity Muse, Unreal AI Assistant, Godot Copilot all embed chat panels with project-aware context; all are cloud-only; none expose a pluggable backend

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **LLM client placement** | C++ HTTP in plugin · DiaPython library · External proxy process | Python has `ollama`, `anthropic`, `google-generativeai` libs; C++ would need bespoke HTTP streaming |
| **Tool execution path** | Direct C++ call to EditorActionRegistry · Roundtrip via MCP server (localhost:7777) | Direct is faster and avoids self-connection; MCP roundtrip is architecturally consistent with external agent pattern |
| **Backend abstraction** | Deep `ILLMBackend` interface (one impl per provider) · OpenAI-compat shim (single client) · Both | OpenAI-compat covers Ollama + many others; Claude/Gemini need thin wrappers or their own adapters |
| **Knowledge context strategy** | Static bundled `.md` file · Runtime-assembled from `docs/` tree · RAG / vector search · On-demand per message | RAG is most accurate but needs a vector store; static bundle is simplest but stale; runtime assembly is middle ground |
| **Streaming** | Server-Sent Events (SSE) streaming · Polling / batch response | Streaming is essential UX; Ollama, Claude, OpenAI-compat all support SSE; adds async complexity |
| **Tool call display** | Hidden (just show final reply) · Inline in chat (show tool name + params) · Expandable detail | Inline display builds user trust and debuggability; especially important when AI makes editor changes |
| **Context selection** | Always full context · User selects files to include · Plugin auto-selects per message intent | User selection is safest; auto-select is smarter but requires intent classification |
| **Multi-session** | Single session per editor launch · Named persistent sessions · Ephemeral per message | Sessions matter for long workflows; named sessions enable resume across editor relaunches |

## Known Tradeoffs

- **Ollama-first vs. provider-agnostic from day one**: Ollama is free, local, and controllable but tool-calling quality is lower than Claude. Building the abstraction upfront means you can switch when Ollama disappoints, but it's more initial work.
- **DiaPython for LLM calls vs. C++ HTTP**: Python gives you idiomatic library access to all three backends in ~50 lines; C++ gives you tighter integration but you're writing a streaming HTTP client from scratch (significant effort, debugging pain).
- **Direct EditorActionRegistry call vs. MCP roundtrip**: Direct call is simpler for the plugin and avoids a TCP round-trip to localhost. MCP roundtrip means the plugin is architecturally just another MCP client — consistent with external agent workflows, but the self-loop is odd.
- **Static system prompt vs. dynamic context assembly**: Static is fast and predictable but gets stale as the codebase evolves. Dynamic assembly at chat startup is accurate but adds latency and must respect token budgets (Ollama local models typically have 4k-32k context windows).
- **Knowledge file freshness**: AI is only as useful as its context. Stale docs about module APIs lead to hallucinated function names. The knowledge system needs a defined update process, not just a one-time file dump.

## Known Pitfalls (C++ / game engine context)

- **Thread safety**: LLM calls are async/long-running; must not block the editor's 30hz frame loop; responses need to marshal back to the main thread for UI updates (same problem as DiaEditorAPI's EditorActionQueue)
- **Token budget management**: Local Ollama models often have small context windows (4k-8k tokens); naively including all knowledge files will overflow; need to measure and trim
- **Tool call loops**: A poorly prompted LLM can enter a tool-call loop (call → result → call → result → ...) with no termination; need a max-depth guard
- **CEF + streaming**: SSE streaming from a C++ background thread to a React/CEF frontend requires careful IPC (postMessage or WebSocket bridge to the panel); can't call CEF JS from an arbitrary thread
- **Ollama availability**: Ollama may not be running when the editor opens; plugin needs graceful degradation (show "Ollama not detected" state rather than crashing)
- **Model availability**: User may ask for `llama3.2` but only have `mistral` installed; need model discovery (`/api/tags` on Ollama) rather than hardcoding model names
- **Knowledge file maintenance**: If knowledge files are not part of the build/commit workflow, they rot immediately and the AI becomes unreliable; must be first-class, not an afterthought

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaEditorAPI (Phase 2) | Serves `tools/list` + `tools/call` on port 7777 — the tool surface the LLM will call; already specced |
| DiaPython | Embedded Python runtime in the editor; can run `ollama`/`anthropic`/`google-generativeai` libs without writing C++ HTTP client |
| DiaWebSocket | Transport for MCP server and potentially for streaming SSE proxy; already in the editor stack |
| DiaEditor / IEditorPlugin | Chat plugin is an `IEditorPlugin` subclass; gets docked panel, lifecycle hooks, EditorModel access |
| DiaUICEF / React | Chat UI rendered as React component in CEF panel; streaming tokens update the DOM |
| DiaCore / DiaIO | Reading knowledge `.md` files from disk; assembling system prompt at startup |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | Plugin ID, backend IDs, action name lookups all use StringCRC |
| PD-002 PU/Phase/Module | LLM call management should be a Module on EditorPU, not a free-running thread |
| PD-004 No STL in public APIs | LLM response buffering and message list use DiaCore containers in public plugin interface; internal can use std::string |
| PD-007 C++20 | Can use `std::span`, coroutines if helpful for async streaming |
| AED-001 DiaEditor is a pure library | Chat plugin lives in `CluicheEditor` or a new `DiaChatPlugin` Dia module, not embedded in DiaEditor |
| AED-005 React + CEF | Chat UI must be React/TypeScript rendered in CEF; no alternative UI stack |
| EAPI-003 AI adapter is protocol-agnostic | Plugin should not hard-code MCP format; the DiaEditorAPI manifest is the source of tool definitions |
| EAPI-007 MCP server on port 7777 | If plugin routes through MCP, it knows the port; but direct C++ call avoids the network hop entirely |

## Open Questions for Ideation

- Should the LLM client live in C++ (as part of the plugin) or in DiaPython (using existing LLM libraries)? DiaPython is already embedded and has `ollama`/`anthropic` packages available — this could be the fastest path.
- Should the plugin call EditorActionRegistry directly (C++ call, no network) or route through the MCP server (consistent with external agent, but roundtrip to self)? This affects whether Phase 2 of DiaEditorAPI must ship before the chat plugin.
- What is the minimum viable knowledge context? A single curated `editor_knowledge.md` file checked into the repo? Or a structured set of files with a defined update workflow?
- Should the chat plugin support agentic multi-step plans (e.g. "create entity X, add component Y, set field Z") or just single-turn tool calls? Multi-step requires planning/loop logic; single-turn is simpler but less powerful.
- How do streaming tokens from a background Python/HTTP call get to the React UI without blocking the 30hz frame loop? Is a WebSocket from C++ to CEF the right bridge, or can DiaPython call into CEF directly?
- Should backend selection (Ollama / Claude / Gemini) be per-conversation or a global editor setting? Per-conversation is more flexible; global is simpler.
- Is a knowledge file audit + authoring guide in scope for this research, or is that a follow-on task? (User indicated yes — "audit and how to build" — so this must be a deliverable of whatever work item is chosen.)
