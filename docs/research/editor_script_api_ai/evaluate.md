# Research: Evaluate — Editor Script API for Automation and AI Tooling

**Input:** docs/research/editor_script_api_ai/ideate.md

## Scoring Criteria

- **Engine Value (0.25):** Improves Dia module reusability or capability without layer violations
- **Game Value (0.20):** Enables automation, testing, or AI workflows for CluicheTest/CluicheEditor
- **Implementation Cost (0.25):** Inverse of effort — 5=very cheap, 1=very expensive
- **Risk (0.15):** Inverse of uncertainty — 5=well-understood, 1=highly uncertain
- **Cluiche Fit (0.15):** Respects module layer hierarchy, PD decisions, no STL in public APIs

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| C1 Dual-Registration | 4 | 3 | 3 | 3 | 4 | **3.40** |
| C2 WebSocket Server | 3 | 3 | 3 | 4 | 4 | **3.30** |
| C3 Extend DiaAPI (consolidate) | 2 | 2 | 2 | 1 | 1 | **1.70** |
| C4 MCP-first | 3 | 4 | 3 | 3 | 4 | **3.35** |
| C5 Layered Registry | 5 | 4 | 2 | 3 | 5 | **3.75** |
| C6 Minimal Stubs | 2 | 3 | 5 | 5 | 3 | **3.55** |
| C7 Headless Mode | 4 | 3 | 1 | 2 | 3 | **2.60** |

---

## Groups

### ✓ Good Ideas

**Rank 1: C5 — Layered Registry (3.75)**
**Why:** The only candidate that treats the action registry, Python surface, and AI manifest as separate concerns with separate owners. Adapters are swappable — add MCP today, swap to a different AI protocol tomorrow without touching the registry. Scores maximum on Engine Value and Cluiche Fit because it is the only design that perfectly respects the DiaAPI/DiaEditorAPI layer split. Directly enables the Ollama use case via the AI adapter.
**Watch out for:** L-sized. The three tiers need to be built in sequence; the AI adapter is worthless before the registry and Python adapter exist. Needs disciplined scope management to not over-engineer tier 2 while tier 3 is still the goal.

**Rank 2: C6 — Minimal Stubs (3.55)**
**Why:** Highest cost score (5) and risk score (5) — nothing new architecturally, no layer violations possible, shippable in a week. Gets automation unblocked immediately. The `dia_editor.py` facade is the right shape for AI tool functions even if the underlying dispatch is simple. Can be grown into C5 later without throwaway work — the facade becomes the adapter.
**Watch out for:** Only covers ~60% of editor actions. The hand-written facade drifts as new plugins are added unless a generation step is added. Does not produce a machine-readable manifest for Ollama without extra work.

**Rank 3: C1 — Dual-Registration (3.40)**
**Why:** Solves the coverage gap cleanly — one registration, both surfaces. The thread-safe marshal queue is non-trivial but well-understood. Produces a manifest as a first-class output. Better long-term maintainability than C6 because there is no drift by construction.
**Watch out for:** Thread marshaling design must be correct before any plugin authors use the API, or you get subtle race conditions in production. `DeregisterCommand` needs to be added to DiaAPI first.

**Rank 4: C4 — MCP-first (3.35)**
**Why:** The fastest path to the Ollama "add me an entity asset" use case specifically. MCP protocol is already defined — no protocol design needed. Claude and Ollama support it natively. Python test scripts use the same MCP SDK as the AI agent, so there is one surface not two.
**Watch out for:** Ollama MCP support varies by model — Llama 3.2 and Qwen2.5 are solid, smaller models are not. MCP is a process boundary on every call, which adds latency for tight automation loops. Does not auto-generate Python stubs the way DiaPython/DiaAPI do.

**Rank 5: C2 — WebSocket Script Server (3.30)**
**Why:** Fully out-of-process — the most flexible integration point. Any language, any tool, any CI system can connect. JSON-RPC 2.0 is a well-understood standard with good client libraries. Good as the transport layer beneath C4 or C5.
**Watch out for:** Reconnect logic, port management, and process lifecycle are real operational concerns. Better as a component of C4 or C5 than as a standalone choice.

---

### ✗ Wrong Direction / Not Yet

**C3 — Extend DiaAPI (1.70) — WRONG DIRECTION**
DiaAPI is an engine-layer module. Pulling editor plugin lifecycle, CEF threading, and WebUIBridge knowledge into it inverts the dependency direction: DiaEditor currently depends on DiaAPI, not the other way around. Making DiaAPI depend on DiaEditor would create a circular dependency or force a messy optional extension point. Every other candidate avoids this; C3 is the only one that breaks the layer model. Score of 1 on both Risk and Fit reflects this — it is an architectural violation, not just a risk.

**C7 — Headless Mode (2.60) — NOT YET**
Technically valid and genuinely valuable for CI automation, but it is a consumer of a working script API, not a design for one. It cannot be built until C1, C2, or C5 exists. XL scope with CEF headless complexity and process supervision. Right item for a future research session once the script API layer is shipping.

---

## Recommendation

**C5 (Layered Registry)** is the right architecture but C6 (Minimal Stubs) is the right starting point. The pragmatic path is to build C6 first — migrate the 8 critical WebUIBridge handlers to DiaAPI JSON path, generate stubs, write the `dia_editor.py` facade — then grow it into C5 by formalising the registry and adding the MCP/AI adapter. C4's MCP design slides in cleanly as C5's AI adapter. C1's dual-registration becomes C5's registration API. None of the C6 work is thrown away.

C3 is ruled out by architectural constraint, not preference. C7 is deferred until the script surface exists.
