# Research: Choice — State Machines

**Date:** 2026-05-01
**Chosen candidate:** Dual Implementation — Flat FSM + HSM

## Rationale

The dual implementation provides the right abstraction for every complexity level without forcing overhead tradeoffs. A lightweight `FlatStateMachine` handles high-volume simple cases (AI agents, gameplay triggers) while a `HierarchicalStateMachine` handles complex single-instance machines (player controllers, animation state). The shared `IStateMachineInspectable` interface makes logging, editor tooling, and visual debugging work universally across both types. Scored highest on engine value, game value, and Cluiche platform fit (4.30 weighted total).

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| Flat FSM Only | Leaves hierarchy unsolved; would need a second implementation later with risk of inconsistent design |
| HSM Only (flat = depth 1) | Forces hierarchy overhead on trivial use cases; higher implementation risk for a single monolithic type |

## Pre-Spec Commitments

- **Two specs:** One system spec for the core state machine library (DiaStateMachine — covering flat FSM, HSM, pushdown variant, component wrapper, logging/tracing). A separate spec for the editor plugin (visual debugger + design-time editor).
- **Phase system refactor (Candidate 8)** is explicitly deferred — to be discussed when the core library is proven.
- **Animation specialization (Candidate 9)** is future work, not in scope for v1.

## Next Step

Run `/spec-system` for `DiaStateMachine` — the core library covering:
- Shared inspection interface (`IStateMachineInspectable`)
- Flat FSM
- Hierarchical State Machine
- Pushdown Automaton variant
- Component wrapper (IComponent integration)
- Logging channel + transition tracing

Then separately, `/spec-system` for the state machine editor plugin.
