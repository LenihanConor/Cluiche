# Research Summary — State Machines

**Session folder:** docs/research/state_machine/
**Date:** 2026-05-01

## One-Line Answer

Build `DiaStateMachine` as a dual-implementation library — lightweight flat FSM for simple/high-volume cases and hierarchical state machine for complex behaviors — with a shared inspection interface enabling universal tooling.

## Journey

1. **Explored:** Surveyed the existing DiaApplicationFlow phase/module system (sophisticated but lifecycle-coupled, not reusable for gameplay/AI/animation), mapped 10 design axes (topology, triggering, guards, history, concurrency, etc.), and identified 14 Dia modules that provide integration foundations (Events, Graph containers, StringCRC, Logger, Editor plugins, WebUIBridge).
2. **Ideated:** 10 candidates generated spanning S to XL scope — from core library variants to entity integration, logging, visual debugging, design-time editing, phase system refactor, and animation specialization.
3. **Evaluated:** Most candidates were a delivery stack, not competing alternatives. The real decision was core topology: flat-only vs HSM-only vs dual. Dual scored 4.30 (highest) on weighted criteria — best engine value, game value, and Cluiche fit.
4. **Chose:** Dual implementation confirmed. Editor tooling scoped as a separate spec. Phase system refactor deferred.

## Chosen Work Item

**Name:** DiaStateMachine — Dual Implementation (Flat FSM + HSM)
**Home module:** New module: `Dia/DiaStateMachine/`
**Suggested spec type:** System (with child feature specs per phase)
**Estimated size:** L (1–2 months for full delivery stack)

## Key Insights from Exploration

- DiaApplicationFlow's Phase system is a bespoke state machine — powerful but not reusable. The generic FSM should integrate with it (operate within phases/modules), not replace it.
- DiaCore already has `Graph<N,E>` containers that could represent transition topologies, and an `Event` system that naturally triggers state transitions.
- StringCRC (PD-001) is the natural state/transition identity mechanism — consistent with the rest of the engine.
- The shared `IStateMachineInspectable` interface is the critical design decision — it's what makes logging, editor tooling, and visual debugging work across both flat and hierarchical types.
- C++20 concepts (PD-007) can constrain state types at compile time, avoiding cryptic template errors.
- Thread safety should be caller's responsibility (single-threaded FSMs), matching how gameplay code works — don't bake thread-safety overhead into the core.

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| Flat FSM Only | Would need hierarchy eventually; risk of inconsistent bolt-on design |
| HSM Only | Forces overhead on simple cases; higher implementation risk |
| Phase System Refactor | Deferred — discuss when core library is proven |
| Animation Specialization | Future work, not v1 scope |
| Design-Time Visual Editor | Separate spec, not part of core library |

## References

- docs/research/state_machine/explore.md
- docs/research/state_machine/ideate.md
- docs/research/state_machine/evaluate.md
- docs/research/state_machine/choose.md
