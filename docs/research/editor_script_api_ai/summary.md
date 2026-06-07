# Research Summary — Editor Script API for Automation and AI Tooling

**Session folder:** docs/research/editor_script_api_ai/
**Date:** 2026-06-05

## One-Line Answer

Build `DiaEditorAPI` — a three-tier layered registry that auto-generates Python test scripts and serves a protocol-agnostic AI manifest, enabling both automated testing and Ollama-driven natural-language editor workflows.

## Journey

1. **Explored:** CluicheEditor has 53 total editor actions split across DiaAPI (22 commands, ~42%) and WebUIBridge-only (31 handlers, ~58%); the most automation-critical actions — open project, connect to game, load plugin — are all WebUIBridge-only and unreachable from scripts today.
2. **Ideated:** 7 candidates generated spanning S→XL scope, from minimal stub migration to full headless CI mode, each taking a different stance on the DiaAPI consolidation question.
3. **Evaluated:** C5 Layered Registry scored highest (3.75); C3 full DiaAPI consolidation ruled out as an architectural violation (breaks engine/editor dependency direction); C3 is the definitive "no" on merging the two layers.
4. **Chose:** C5 confirmed — both automation testing and Ollama AI equally important, platform capability goal, protocol-agnostic registry future-proofs against AI landscape shifts.

## Chosen Work Item

**Name:** DiaEditorAPI — Layered Registry
**Home module:** New module `DiaEditorAPI` (depends on DiaEditor + DiaAPI; sits above both)
**Suggested spec type:** System
**Estimated size:** L (1–2 months), phased delivery

### Phase 1 — Automation Testing (unblocks test scripts)
- C++ `EditorAction` registry with name, description, param schema, thread dispatch policy
- Migrate critical WebUIBridge request handlers to dual-register with DiaEditorAPI
- Auto-generated Python module `dia_editor` (one callable per registered action)
- `.pyi` stub file emitted at startup for IDE completion

### Phase 2 — AI Tooling (unblocks Ollama use case)
- Protocol-agnostic manifest serialiser
- MCP adapter: serves registered actions as MCP tools (`tools/list`, `tools/call`)
- Ollama at-desk workflow: natural language → parameter gather → `dia_editor` function call

## Key Insights from Exploration

- **DiaAPI is the plumbing, DiaEditorAPI is the contract.** DiaAPI handles JSON dispatch, Python auto-gen infrastructure, and CLI mechanics. DiaEditorAPI owns which editor actions exist, what their parameters are, and which thread they run on. Never merge these — the dependency direction is one-way.
- **58% of editor actions are script-unreachable today.** The most important ones for automation (project open/close, game connection, plugin load) are all WebUIBridge-only. Phase 1 fixes this.
- **Thread safety is the hard part.** WebUIBridge handlers run on CEF's thread; DiaAPI commands run on the caller's thread. Every action needs an explicit thread dispatch policy — this is required infrastructure, not optional.
- **Auto-generation prevents drift.** Hand-authored Python facades go stale as plugins are added. The registry must be the single source of truth; Python stubs and AI manifests are derived outputs.
- **Protocol-agnostic beats MCP-specific.** The AI adapter serialises the registry to MCP today. When the next protocol arrives, write a new adapter — the registry never changes. Stability at the contract layer, flexibility at the integration layer.
- **C6 is Phase 1.** The minimal stubs approach is not discarded — it becomes the first milestone of C5. No throwaway work.

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| C3 Extend DiaAPI | Architectural violation — DiaAPI would need to depend on DiaEditor, inverting the current dependency direction and risking circular deps |
| C7 Headless Mode | Deferred — valuable for CI but is a consumer of DiaEditorAPI, not the API itself; revisit after Phase 2 |
| C1 Dual-Registration | Good but narrower than C5 — no AI adapter tier; becomes C5's registration mechanism instead |
| C2 WebSocket Server | Better as C5's AI adapter transport than a standalone choice |
| C4 MCP-first | Fastest Ollama path but weaker on automation; becomes C5's Phase 2 AI adapter |
| C6 Minimal Stubs | Right starting point, becomes C5 Phase 1 |

## References

- docs/research/editor_script_api_ai/explore.md
- docs/research/editor_script_api_ai/ideate.md
- docs/research/editor_script_api_ai/evaluate.md
- docs/research/editor_script_api_ai/choose.md
