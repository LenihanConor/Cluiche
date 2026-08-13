# Research Summary — Gameplay Message Bus

**Session folder:** docs/research/gameplay_msg_bus/
**Date:** 2026-08-10

## One-Line Answer

Build `DiaMessageBus` — a typed, two-pass, editor-visible message bus on top of DiaMailbox — as the standard communication layer for all gameplay systems and entity components in Cluiche.

## Journey

1. **Explored:** The problem space was substantially settled through pre-research discussion. Key decisions — observer-mediated bus, shared schema module, batched two-pass delivery, DiaMailbox as transport — were all locked before writing explore.md. An infrastructure audit revealed DiaMailbox (done, 49 tests), EventDispatcher (isolated, STL-contaminated, redundant), and DiaStreams (cross-PU, unaffected) as the relevant existing systems.
2. **Ideated:** 7 candidates generated, ranging from application-layer-only (S) to full system with schema code-gen (XL), covering all combinations of scope: broadcast-only vs both routers, core-only vs visualization, extend-DiaMailbox vs new module.
3. **Evaluated:** Full system (C1) scored highest — no open architectural questions remain, visualization is a first-class goal not an afterthought, and the DiaMailbox foundation makes the full scope tractable. EventDispatcher removal (C4) bundled as it is redundant the moment DiaMessageBus ships.
4. **Chose:** Candidate 1 + 4 bundle. User confirmed after reviewing the schema browser mockup (`inspector-mockup.html`) and live inspector deferral rationale.

## Chosen Work Item

**Name:** DiaMessageBus
**Home module:** New `Dia/DiaMessageBus/` engine module  
**Suggested spec type:** System
**Estimated size:** L

## Key Insights from Exploration

- **DiaMailbox is the right foundation.** It was explicitly designed for "module-to-module, editor-to-game, ad-hoc system-to-system events" — DiaMessageBus is the first non-entity consumer of this primitive. The transport is done; DiaMessageBus adds the policy layer.
- **One bus, multiple routers.** BroadcastRouter (system fan-out) and EntityRouter (addressed component delivery) live in the same Mailbox instance. All four communication patterns (system↔system, entity↔entity, system↔entity, entity↔system) work via Address choice alone — sender identity is irrelevant.
- **EventDispatcher is dead on arrival.** It solves the same problem as DiaMessageBus with STL internals and no flush model. Removing it in the same spec is cleaner than leaving a stranded system.
- **Two-pass flush solves reaction chains.** Primary pass (systems doing work) + Reaction pass (responses to that work). Reaction messages cannot re-queue — hard depth limit, not a counter.
- **Frame ledger is free.** Batched delivery means the flushed batch is the ledger. 60-second ring buffer game-side, stripped in Release, exposed via ServiceStream. The schema browser and future live inspector read the same data.
- **Schema browser is the primary editor surface.** Design-time, static. Graph view (producer → diamond → consumers) + list view + payload inspector + structural duplicate analysis. Live inspector is deferred but the seam (ring buffer + DiaObservation tap) must be designed in from the start.
- **Task 0 is mandatory:** audit all existing `IXxxObserver`, `ObserverSubject`, and `EventDispatcher` usages before implementation to produce the concrete migration list.

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| C2: Core only, no viz | Deferring viz risks designing the bus without validating the schema browser data model |
| C3: Broadcast only | EntityRouter and BroadcastRouter share the same Mailbox instance — no reason to split |
| C5: Extend DiaMailbox | Settled primitive, SD-MBX-003 explicitly scoped callbacks out, adds risk for no gain |
| C6: Application layer only | Bus mechanism is generic engine infrastructure, not game-specific |
| C7: Schema code-gen | Future milestone — "DiaMessageBus tooling v2" alongside live inspector |

## Visual Reference

`docs/research/gameplay_msg_bus/inspector-mockup.html` — interactive schema browser mockup showing graph view, list view, payload inspector, and duplicate analysis panel. Use as visual acceptance gate for the editor feature.

## References

- docs/research/gameplay_msg_bus/explore.md
- docs/research/gameplay_msg_bus/ideate.md
- docs/research/gameplay_msg_bus/choose.md
- docs/specs/applications/dia/systems/diamailbox/diamailbox.md
- Dia/DiaMailbox/dia.mailbox.architecture.module.md
