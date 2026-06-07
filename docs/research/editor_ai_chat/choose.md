# Research: Choice — Editor AI Chat Plugin

**Date:** 2026-06-06
**Chosen candidate:** C6 — Bundled Chat Plugin + Knowledge System + Tool Transparency UI

## Rationale

C6 delivers the complete AI assistant experience in one work item. The DiaPython path for LLM calls removes the biggest implementation risk (no bespoke C++ HTTP streaming client). Direct `DiaEditorAPI::ExecuteAction()` keeps tool execution simple and independent of DiaEditorAPI Phase 2. The hybrid UI (classic chat + collapsible detail panel with clickable tool call inspection) was validated via mockup. Context control via chips + `@file` injection + Tools-only mode toggle gives the user full visibility without magic.

Ollama is the primary backend. Claude and Gemini are supported via thin adapters with no architecture change — switching is a config dropdown.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|-----------------|
| C1 Minimal Chat Shell | No tool calling — incomplete as a standalone deliverable |
| C2 Python Chat + Tools | Missing knowledge system; AI gives generic/hallucinated answers without it |
| C3 C++ Native LLM Client | Heavy engineering effort; only justified if DiaPython proves too slow — defer until that's a measured problem |
| C4 Multi-Step Agentic Loop | Natural Phase 2 once C6 is proven; too much prompt-engineering risk to bundle into Phase 1 |
| C5 Knowledge System standalone | Zero interactive value alone; fully included in C6 |
| C7 Backend Abstraction Module | Premature extraction; no second consumer on the roadmap yet |

## Pre-Spec Commitments

- **Plugin name:** `DiaChatPlugin` — IEditorPlugin subclass, lives in CluicheEditor
- **LLM client:** DiaPython orchestrator (`dia_chat.py`); OpenAI-compat endpoint for Ollama + thin adapters for Claude/Gemini
- **Tool execution:** Direct C++ call to `DiaEditorAPI::ExecuteAction()` — no MCP roundtrip
- **UI:** Hybrid — classic chat (Option 1) + collapsible detail panel (Option 3); confirmed via `mockup_c4_hybrid.html`
- **Context controls:** Chip toggles (persistent per-session), `@file` inline injection (per-message), Tools-only mode toggle (strips system prompt to tool definitions only)
- **Knowledge files:** Curated `ai_context/` directory; four initial files (`engine_overview.md`, `module_apis.md`, `editor_actions.md`, `coding_conventions.md`); authoring guide in `docs/reference/ai-guides/knowledge-authoring.md`
- **Phase 2 (out of scope):** Multi-step agentic loop (C4) — flagged in spec as planned follow-on
- **Dependency:** Requires DiaEditorAPI Phase 1 (`ExecuteAction()` available); does not require DiaEditorAPI Phase 2 (MCP server)

## Next Step

Run `/spec-system` with this candidate as input.
Suggested parent: CluicheEditor application spec (`docs/specs/applications/cluicheeditor.md`)
Suggested system name: `DiaChatPlugin`
