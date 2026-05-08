# Research: Evaluate — State Machines

**Input:** docs/research/state_machine/ideate.md

## Scoring Criteria

- **Engine Value (0.25):** Improves Dia module reusability or capability
- **Game Value (0.20):** Improves CluicheTest as a demo or testbed
- **Implementation Cost (0.25):** Inverse of effort — 5 = very cheap, 1 = very expensive
- **Risk (0.15):** Inverse of uncertainty — 5 = well-understood, 1 = highly uncertain
- **Cluiche Fit (0.15):** Aligns with module structure and PD-001 through PD-007

## Scores

These scores evaluate only the **core topology decision** — the foundational choice that everything else builds on.

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| 1. Flat FSM Only | 3 | 3 | 5 | 5 | 4 | 3.85 |
| 2. HSM Only (flat = depth 1) | 4 | 4 | 3 | 3 | 4 | 3.55 |
| 3. Dual (Flat + HSM) | 5 | 5 | 3 | 4 | 5 | 4.30 |

## Top 3 Candidates

### Rank 1: Dual Implementation — Flat + HSM (score: 4.30)
**Why:** Maximum engine value — provides the right abstraction for every use case without forcing overhead tradeoffs. The shared `IStateMachineInspectable` interface means all tooling (logging, editor plugin, visual debugger) works universally. Perfect Cluiche fit: StringCRC IDs (PD-001), DiaCore containers in public API (PD-004), Component system integration ready (PD-003), C++20 concepts to constrain state types (PD-007). The flat implementation is small enough that maintaining two types is not a significant burden.
**Watch out for:** API surface must stay aligned — any feature added to one type should be considered for the other. The shared inspection interface is the critical design contract; get it right early.

### Rank 2: Flat FSM Only (score: 3.85)
**Why:** Cheapest to build, lowest risk, immediately useful for simple gameplay and AI. Could serve as a stepping stone if time-boxed.
**Watch out for:** Leaves the hard problem (hierarchy) unsolved. Building flat first then adding HSM later risks inconsistent designs — better to design both upfront even if HSM ships later.

### Rank 3: HSM Only (score: 3.55)
**Why:** Single implementation covers all cases. Elegant in theory.
**Watch out for:** Hierarchy adds complexity even for trivial use cases. Guard cascade through parent chains, history semantics, and entry/exit propagation are non-trivial to get right. Higher risk of delays.

## Recommendation

**Candidate 3 (Dual Implementation)** is the clear winner. It scores highest on engine value, game value, and Cluiche fit because it provides purpose-built solutions for both ends of the complexity spectrum. The flat FSM stays lean and cache-friendly for high-volume simple machines (AI agents, gameplay triggers), while the HSM handles complex single-instance machines (player controllers, animation state, application flow) without state explosion. The shared inspection interface (PD-001 compliant via StringCRC identification) is what makes the entire tooling stack — logging, editor plugin, visual debugger — work universally across both types.

The implementation cost is moderate (L, 1–2 months for both types) but not significantly more than HSM-only, since the flat FSM is small. The risk is actually lower than HSM-only because the flat type is well-understood and can ship first while HSM is still being refined.

## Delivery Stack

With the core decided, the remaining candidates form a phased delivery plan rather than competing alternatives:

| Phase | Work Item | Size | Depends On |
|-------|-----------|------|------------|
| 1a | Flat FSM core | M | — |
| 1b | HSM core | M | — (parallel with 1a, shared interface designed first) |
| 2 | Pushdown Automaton variant | S | Phase 1a |
| 3 | Component wrapper (IComponent) | M | Phase 1 |
| 4 | Logging channel + transition tracing | S | Phase 1 |
| 5 | Editor plugin (visual debugger) | L | Phase 1 + 4 |
| 6 | Phase system refactor (optional) | L | Phase 1b |
| 7 | Design-time visual editor | XL | Phase 5 |
| 8 | Animation specialization | L | Phase 1b (future) |
