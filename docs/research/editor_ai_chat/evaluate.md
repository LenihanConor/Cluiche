# Research: Evaluate — Editor AI Chat Plugin

**Input:** docs/research/editor_ai_chat/ideate.md

## Scoring Criteria

- **Engine Value** (0.25): Improves Dia module reusability or platform capability
- **Game Value** (0.20): Improves CluicheTest as a demo or developer testbed
- **Implementation Cost** (0.25): Inverse of effort — 5 = very cheap, 1 = very expensive
- **Risk** (0.15): Inverse of uncertainty — 5 = well-understood, 1 = highly uncertain
- **Cluiche Fit** (0.15): Aligns with module structure and PD-001 through PD-007

C3 (C++ Native LLM Client) excluded from scoring — ruled out in exploration as an escape hatch, not a proactive choice.

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| C1 Minimal Chat Shell | 2 | 1 | 5 | 4 | 4 | 3.15 |
| C2 Python Chat + Tools | 3 | 2 | 4 | 3 | 4 | 3.20 |
| C4 Multi-Step Agentic | 4 | 2 | 2 | 2 | 3 | 2.65 |
| C5 Knowledge System | 3 | 1 | 5 | 5 | 5 | 3.70 |
| C6 Bundled (C2+C5+UI) | 5 | 2 | 2 | 3 | 4 | 3.20 |
| C7 Backend Module First | 4 | 1 | 3 | 3 | 4 | 3.00 |

## Top 3 Candidates

### Rank 1: C5 Knowledge Context System (score: 3.70)
**Why:** Cheapest path to meaningful AI quality improvement — a curated set of `.md` files written for LLM consumption costs one sprint of docs work and pays dividends for every AI interaction, regardless of which chat client or backend is used. Zero architectural risk; fits cleanly without touching any C++ or vcxproj. The authoring guide (how to write and maintain knowledge files) is the part that creates durable value.
**Watch out for:** C5 alone delivers no UI and no tool calling. Its score is inflated by the scoring model favouring small, certain items. As a standalone deliverable it is incomplete — a developer would use it once and have nothing to interact with. It only makes sense bundled into C6.

### Rank 2: C6 Bundled Chat + Knowledge + UI (score: 3.20)
**Why:** The complete story in one work item. C2 (tool execution) + C5 (knowledge) + polished tool transparency UI gives you an AI assistant that can answer project-specific questions accurately AND execute editor actions with visible confirmation. DiaPython handles all three backends (Ollama, Claude, Gemini) via the OpenAI-compat shim — no new Dia module needed, no C++ HTTP client. Depends only on DiaEditorAPI Phase 1 (`ExecuteAction()`), not Phase 2 (no MCP server required). Aligns with EAPI-003 (protocol-agnostic registry) and AED-005 (React + CEF) by construction.
**Watch out for:** Three moving parts (streaming IPC, Python orchestrator, knowledge assembly) each have their own first-time risk. The streaming token path — from a background Python thread through C++ main-thread marshal into a CEF JS call — is the highest-risk single piece and should be spiked first before committing to the full scope.

### Rank 3: C2 Python Chat + Tools (score: 3.20, ranked lower than C6 on Engine Value tiebreak)
**Why:** M-sized sweet spot if C6 feels too large. Delivers tool-augmented chat without the knowledge system or polished UI. Proves the streaming path and the tool execution loop before committing to documentation work.
**Watch out for:** Without the knowledge system (C5) the AI gives generic answers and frequently hallucinates engine API names. C2 alone would likely feel underwhelming in practice — the knowledge gap is noticeable. Most teams that ship C2 end up wanting C5 within the same sprint.

## Recommendation

**C6 is the right work item.** C5 scoring highest is an artifact of the scoring model preferring small, certain items — as a standalone it delivers nothing interactive and would be shelved immediately. C6 is C5 bundled with everything needed to actually use it. The DiaPython path for the LLM client removes the main implementation risk (no bespoke C++ HTTP streaming client), and the direct `ExecuteAction()` call keeps the tool execution path simple and independent of DiaEditorAPI Phase 2. Per EAPI-003, the registry is already protocol-agnostic — the chat plugin is simply the first internal consumer of that protocol-agnostic manifest, using it to build tool definitions for whichever LLM backend is active. C4 (multi-step agentic loop) is the natural Phase 2 once C6 is proven; it should be flagged in the spec but not included in scope.
