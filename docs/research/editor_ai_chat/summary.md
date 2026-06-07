# Research Summary — Editor AI Chat Plugin

**Session folder:** docs/research/editor_ai_chat/
**Date:** 2026-06-06

## One-Line Answer

Build `DiaChatPlugin` — a dockable CluicheEditor panel that connects to Ollama (or Claude/Gemini) via a DiaPython orchestrator, executes editor actions directly through `DiaEditorAPI::ExecuteAction()`, and grounds AI answers in curated engine knowledge files.

## Journey

1. **Explored:** The problem has three distinct parts — LLM backend abstraction, tool execution path, and knowledge context. DiaPython (already embedded) eliminates the need for a bespoke C++ HTTP streaming client. The tool execution path question — MCP roundtrip vs direct call — was resolved in favour of direct: cloud backends can't reach localhost:7777, so the plugin must orchestrate tool calls regardless; Ollama runs through the same loop.
2. **Ideated:** 7 candidates generated; range from S (minimal chat shell, knowledge system standalone) through M (Python chat + tools, backend module) to L (bundled, agentic loop, C++ native client).
3. **Evaluated:** C5 (Knowledge System) scored highest on the model (3.70) due to low cost/risk, but is incomplete without a chat UI. C6 (Bundled) scored 3.20 and is the only candidate that delivers a complete, usable feature.
4. **Chose:** C6 confirmed by user. Hybrid UI (classic chat + detail panel) validated via interactive mockup.

## Chosen Work Item

**Name:** DiaChatPlugin
**Home module:** New `DiaChatPlugin` editor plugin (IEditorPlugin, lives in CluicheEditor)
**Suggested spec type:** System (child of CluicheEditor application spec)
**Estimated size:** L

## Key Insights from Exploration

- **DiaPython is the right LLM client layer** — `ollama`, `anthropic`, and `google-generativeai` libraries give you all three backends in ~50 lines of Python; writing a C++ streaming HTTP client is significant engineering effort with no benefit until Python proves too slow
- **Direct ExecuteAction() beats MCP roundtrip** — cloud backends (Claude, Gemini) cannot call localhost:7777; the plugin must be the orchestrator regardless; routing Ollama through MCP would be a self-connection to localhost for no gain
- **DiaEditorAPI Phase 1 is the only hard dependency** — the chat plugin needs `ExecuteAction()` available; it does not need the MCP server (Phase 2); this means the chat plugin can ship before external agent workflows
- **Knowledge files are load-bearing** — without curated, LLM-optimised context docs, the AI hallucinates Dia API names and gives generic game-engine advice; the knowledge system is not optional polish
- **Context window is a first-class constraint** — Ollama local models typically have 4k–32k context windows; token budget management and Tools-only mode are necessary features, not nice-to-haves
- **Multi-step agentic loop is Phase 2** — single tool call per message is sufficient for the initial release; chaining requires approval gates and prompt-engineering work that adds risk without being necessary to prove the concept

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| C1 Minimal Chat Shell | No tool calling — proves streaming pipeline but delivers nothing useful |
| C2 Python Chat + Tools | Missing knowledge system; AI gives generic answers without it; C2 is just C6 minus the useful half |
| C3 C++ Native LLM Client | Escape hatch only — build if DiaPython streaming proves too slow, not proactively |
| C4 Multi-Step Agentic Loop | Phase 2 once C6 foundation is proven; adds prompt-engineering risk and approval gate complexity |
| C5 Knowledge System standalone | Zero interactive value alone; fully bundled into C6 |
| C7 Backend Abstraction Module | No second consumer on the roadmap; premature extraction |

## References

- docs/research/editor_ai_chat/explore.md
- docs/research/editor_ai_chat/ideate.md
- docs/research/editor_ai_chat/evaluate.md
- docs/research/editor_ai_chat/choose.md
- docs/research/editor_ai_chat/mockup_c1_classic_chat.html
- docs/research/editor_ai_chat/mockup_c2_command_palette.html
- docs/research/editor_ai_chat/mockup_c3_timeline.html
- docs/research/editor_ai_chat/mockup_c4_hybrid.html
- docs/research/editor_script_api_ai/summary.md (DiaEditorAPI — Phase 1 prerequisite)
- docs/specs/systems/cluicheeditor/diaeditorapi.md (DiaEditorAPI spec — ExecuteAction dependency)
