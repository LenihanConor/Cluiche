# Research: Choice — Editor Script API for Automation and AI Tooling

**Date:** 2026-06-05
**Chosen candidate:** C5 — Layered Registry (DiaEditorAPI)

## Rationale

All three decision questions pointed to C5:
- Weeks-fine scope with fast execution → L is manageable
- Both automation testing and Ollama AI equally important → only C5 handles both without compromise
- Platform capability goal → three-tier adapter design is built to extend without throwaway work

C5 respects the DiaAPI/DiaEditorAPI layer boundary (DiaAPI stays engine-layer plumbing; DiaEditorAPI owns the editor action contract), produces a machine-readable manifest for AI agents, and auto-generates the Python surface so there is no drift as plugins are added.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| C1 Dual-Registration | Good but narrower — no AI adapter tier, needs DeregisterCommand prereq on DiaAPI |
| C2 WebSocket Server | Better as a component of C5's AI adapter than a standalone choice |
| C3 Extend DiaAPI | Architectural violation — inverts DiaAPI/DiaEditor dependency direction |
| C4 MCP-first | Fastest Ollama path but weaker on automation testing; becomes C5's AI adapter instead |
| C6 Minimal Stubs | Right starting point but grows into C5 anyway; phase 1 of the build plan |
| C7 Headless Mode | Deferred — consumer of a working script API, not the API itself |

## Pre-Spec Commitments

1. **Module name:** `DiaEditorAPI` — mirrors DiaAPI naming pattern; sets precedent for `DiaGameAPI` etc.
2. **Python adapter:** Auto-generated from the action registry (no hand-authored stubs that can drift)
3. **AI adapter:** Protocol-agnostic registry; MCP is the first serialisation target — swap to OpenAI function-calling or others by writing a new adapter, not changing the registry
4. **Phased delivery:** Phase 1 = C++ registry + Python auto-gen (automation testing unblocked); Phase 2 = MCP/AI adapter (Ollama use case unblocked)
5. **DiaAPI boundary:** DiaAPI remains engine-layer (debug, asset queries, app lifecycle); DiaEditorAPI owns editor actions (project, plugins, pipeline, game connection)
6. **WebUIBridge** stays for push notifications (UI ↔ C++ events); all request/response moves to DiaEditorAPI over time
7. **Thread policy per action:** each registered action declares its dispatch thread so the marshal queue is safe by construction

## Next Step

Run `/spec-system` with this candidate as input.
Suggested parent: CluicheEditor application spec
Suggested system name: DiaEditorAPI
