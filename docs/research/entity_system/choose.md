# Research: Choice — Entity System

**Date:** 2026-05-09
**Chosen candidate:** Merged Candidate 3 + 8 — Layered ECS: Component Interfaces + Editor-First Inspection

## Rationale

The user wants a simple, editor-visible, safe entity system where components are gameplay interfaces (config + asset trigger + API) into independent systems. Candidates 3 (typed component bindings with interface contracts) and 8 (editor-first inspection with IEntityInspectable) together cover all stated priorities: simplicity, editor/debug visibility, safe communication, JSON data, stage-boundary lifecycle, and system independence.

The layered design means foundation ships first, then communication, then editor inspection — each layer adds value without reworking prior layers.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| 1. Handle Registry (Minimal) | Too primitive — no communication, no editor visibility, no typed interfaces |
| 2. Handle Registry + Mailbox | Subset of merged 3+8 — communication without typed components |
| 4. Archetype-Based ECS | Overkill complexity for stated priorities. Optimizes for parallelism/cache (low priority). 2 month effort. |
| 5. Entity-as-Module | Forces every entity into DiaApplicationFlow lifecycle. Doesn't scale. Conflates gameplay entities with infrastructure modules. |
| 6. World + Blueprints | SystemAdapter abstraction rejected — component calls system directly, no middleman |
| 7. Signal Graph (Reactive) | Too complex. 2 month effort. Exotic pattern with debugging challenges. |
| 9. Slim ECS with Queries | Queries are a layer that stacks on top of the chosen design — not an alternative foundation |
| 10. Entity Table (Spreadsheet) | Too rigid (fixed columns). No communication. Doesn't scale to complex entity types. |

## Pre-Spec Commitments

### Architecture

- **Entity** = generational ID + debug name (optional) + set of components
- **Component** = config (per-entity JSON) + asset trigger (config values drive loads) + gameplay interface (API delegating to system via handle) + opt-in DoUpdate
- **World** = stage-scoped container (entity storage, query cache, mailbox). Standalone library concept (no DiaApplicationFlow dependency).
- **EntityModule** = application-level adapter that plugs World into DiaApplicationFlow v2 stage lifecycle. Lives in application code, not DiaEntity.
- **Systems remain independent** — physics, rendering, animation own their data. Components call systems directly (no SystemAdapter middleman).
- **Entity prefabs are assets** — loaded by AssetService, parsed into entities, component configs trigger further asset loads.
- **Old IComponent/IComponentObject/IComponentFactory removed first** (clean slate before building DiaEntity).

### Decisions Locked

| # | Decision | Choice |
|---|----------|--------|
| 1 | Entity identity | Generational index (name is optional metadata) |
| 2 | Registration | DIA_COMPONENT macro (matches DIA_MODULE) |
| 3 | Editor interface | IEntityInspectable (polled, matches IApplicationInspectable) |
| 4 | Component deps | Declared + hard validation at creation |
| 5 | Module location | New DiaEntity in Dia/ (standalone, no DiaApplicationFlow dep) |
| 6 | Component update model | Hybrid opt-in (passive default, DoUpdate opt-in) |
| 7 | Cross-entity references | Typed reference slots (name + required component type on target, validated at resolve) |
| 8 | Old IComponent removal | Before building DiaEntity (clean slate) |
| 9 | Mailbox format | Typed message structs + structured address (discriminated union: Entity/All/ComponentType/Self) |
| 10 | Query cache invalidation | End-of-frame batch rebuild (mutations collected → single rebuild → mailbox delivery) |
| 11 | Blueprint schema | Versioned JSON with separate references block |
| 12 | IEntityInspectable signatures | Defer to spec |
| 13 | DIA_COMPONENT macro scope | Defer to spec |

### Format & Data

- Runtime format: JSON blueprints (entity prefabs)
- USD: concepts adopted (typed entity + applied component bundles), library not taken. Clean path to future USD import via swappable loader interface.
- Blueprint loader is interface-based (JsonBlueprintLoader now, potential UsdBlueprintLoader later)
- No format-specific assumptions in DiaEntity public API

### Frame Timing Model

```
Frame start
  → Process mailbox (deliver last frame's messages)
  → Component DoUpdate (opt-in components tick)
  → Gameplay module update (queries entities, drives logic, sends messages)
  → End-of-frame: collect mutations → rebuild query caches → queue messages for next frame
```

### Integration with DiaApplicationFlow v2

- EntityModule lives in SimPU, stage-scoped
- EntityModule.DoStart() → loads blueprints (assets) → creates entities → resolves references → returns kLoading until all asset loads complete → returns kReady
- Stage transition destroys World (all entities destroyed, all system handles released)
- No cross-stage entity persistence (hard cut)
- No pre-loading next stage's assets

## Extracted Generic Systems

The research identified two generic capabilities that should be spec'd independently of DiaEntity:

### HandlePool<T> — DiaCore addition (not a new module)

Generic freelist + generation bump allocator. Companion to existing `DiaCore/Containers/Handle.h`. Any system that manages pooled objects (physics, render, audio, entities) uses this. DiaEntity's entity storage IS a `HandlePool<Entity>`.

### DiaMailbox — New module

Typed deferred messaging with structured addressing. Not entity-specific — any system can use it (module-to-module, editor-to-game, etc.). DiaEntity is one consumer. Depends on DiaCore only.

**Dependency chain:**
```
DiaCore (Handle<T> + HandlePool<T>)
  ↑
DiaMailbox (typed deferred messaging)
  ↑
DiaEntity (World, components, queries, inspection)
```

## Next Step

Run /spec-system for each in dependency order:

1. **HandlePool<T>** — small DiaCore feature spec (addition to Containers/)
2. **DiaMailbox** — system spec (new module)
3. **DiaEntity** — system spec (new module, depends on above two)

Within DiaEntity, suggested feature implementation order:

1. Remove old IComponent infrastructure (clean slate)
2. Foundation (World, Entity, generational IDs, component base class)
3. Blueprint loading (JSON loader, asset integration)
4. Component deps + reference resolution
5. Mailbox integration (communication via DiaMailbox)
6. Query system (dynamic + cached, end-of-frame rebuild)
7. IEntityInspectable
8. EntityModule adapter (application integration)
