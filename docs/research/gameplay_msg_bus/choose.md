# Research: Choice — Gameplay Message Bus

**Date:** 2026-08-10
**Chosen candidate:** Candidate 1 + Candidate 4 bundle

## Rationale

Candidate 1 (full system: bus + both routers + schema browser visualization + frame ledger) was chosen because the design is fully settled through pre-research discussion — no open architectural questions remain that would justify starting smaller. The bus, router model, two-pass flush, and editor surface all fell out naturally from first principles.

Candidate 4 (EventDispatcher removal + DiaInput migration) was bundled because EventDispatcher is already redundant — it solves the same problem as DiaMessageBus with STL internals and no flush model. Leaving it alive after DiaMessageBus ships would mean two messaging systems and a deferred migration TODO. Clean to remove in the same body of work.

Candidate 7 (schema code-gen via DiaPython) was explicitly deferred as a named future upgrade path — "DiaMessageBus tooling v2" alongside the live inspector.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| C2 (core only, no viz) | Visualization is a first-class goal; deferring it would mean designing the bus without knowing if the schema browser data model is correct |
| C3 (broadcast only) | EntityRouter and BroadcastRouter share the same Mailbox instance and flush — no reason to split the work |
| C5 (extend DiaMailbox directly) | DiaMailbox is a settled primitive (49 tests GREEN); SD-MBX-003 explicitly scoped callbacks out; fighting the existing spec adds risk for no gain |
| C6 (application layer only) | Bus mechanism is generic engine infrastructure — any game should be able to use it; locking it to CluicheTest is premature |
| C7 (schema code-gen) | Deferred — designer-authored message types are a future need, not a current one |

## Full Decision Record

| # | Decision |
|---|---------|
| 1 | Observer mechanism, bus is sole subscriber, no system-to-system subscriptions |
| 2 | Physics/animation excluded; flush adapters at fixed pre-Primary sync point |
| 3 | Shared schema module `DiaGameplayMessages` owns all message types (application layer, not Dia engine) |
| 4 | Batched delivery, two named passes: Primary + Reaction. Reaction messages cannot re-queue |
| 5 | DiaMessageBus builds on DiaMailbox — adds callback dispatch, two-pass flush, producer registration, sim loop wiring |
| 6 | BroadcastRouter + EntityRouter in one Mailbox instance, same flush, same ledger |
| 7 | All four patterns work via Address choice alone: system↔system (broadcast), entity↔entity (entity), system↔entity (entity), entity↔system (broadcast) |
| 8 | DiaInput crosses thread boundary — EventStream + InputBusAdapter, not direct bus participant |
| 9 | EventDispatcher removed from DiaCore, DiaInput migrated to InputBusAdapter (same spec) |
| 10 | Schema browser is design-time only — graph view + list view + payload inspector + duplicate analysis |
| 11 | Frame ledger ring buffer lives game-side, 60 seconds history, stripped in Release. Exposed via ServiceStream for editor |
| 12 | Live inspector deferred — seam is ring buffer + DiaObservation tap interface. "DiaMessageBus tooling v2" |
| 13 | Task 0 in spec: audit all existing observer/event usage before implementation |

## Pre-Spec Commitments

- `DiaGameplayMessages` lives in the application layer (CluicheTest or game-specific), NOT as a Dia engine module — message schemas are game content
- `DiaMessageBus` itself is a Dia engine module (reusable by any game)
- Frame ledger ring buffer sized to 60 seconds of gameplay messages — expected to be kilobytes; compile-flag stripped in Release
- Live inspector is explicitly out of scope — seam must be designed in (ring buffer + tap) but UI is deferred
- Spec Task 0 is a mandatory audit before any implementation: identify all `IXxxObserver`, `ObserverSubject`, and `EventDispatcher` usages that will migrate to DiaMessageBus
- Candidate 7 (schema code-gen) noted as future milestone alongside live inspector

## Next Step

Run `/spec-system` with this choice as input.
Suggested parent: `docs/specs/applications/dia/dia.md`
Suggested system name: `DiaMessageBus`
