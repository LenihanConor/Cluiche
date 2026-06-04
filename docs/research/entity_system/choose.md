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
- **EntityModule** = application-level adapter that plugs World into DiaApplicationFlow v2 stage lifecycle. Lives in application code, not diaentitytemplate.
- **Systems remain independent** — physics, rendering, animation own their data. Components call systems directly (no SystemAdapter middleman).
- **Entity prefabs are assets** — loaded by AssetService, parsed into entities, component configs trigger further asset loads.
- **Old IComponent/IComponentObject/IComponentFactory removed first** (clean slate before building diaentitytemplate).

### Decisions Locked

| # | Decision | Choice |
|---|----------|--------|
| 1 | Entity identity | Generational index (name is optional metadata) |
| 2 | Registration | DIA_COMPONENT macro (matches DIA_MODULE) |
| 3 | Editor interface | IEntityInspectable (polled, matches IApplicationInspectable) |
| 4 | Component deps | Declared + hard validation at creation |
| 5 | Module location | New diaentitytemplate in Dia/ (standalone, no DiaApplicationFlow dep) |
| 6 | Component update model | Hybrid opt-in (passive default, DoUpdate opt-in) |
| 7 | Cross-entity references | Typed reference slots (name + required component type on target, validated at resolve) |
| 8 | Old IComponent removal | Before building diaentitytemplate (clean slate) |
| 9 | Mailbox format | Typed message structs + structured address. Refined during DiaMailbox spec (2026-05-17): the four address kinds (Entity/All/ComponentType/Self) live in diaentitytemplate's **entity router payload encoding**, not in DiaMailbox itself. DiaMailbox sees opaque `(StringCRC routerId, uint64_t payload)`; the entity router decodes payload into the four kinds. Keeps DiaMailbox a generic primitive. See @docs/specs/systems/dia/diamailbox.md. |
| 10 | Query cache invalidation | End-of-frame batch rebuild (mutations collected → single rebuild → mailbox delivery) |
| 11 | Blueprint schema | Versioned JSON with separate references block |
| 12 | IEntityInspectable signatures | Defer to spec |
| 13 | DIA_COMPONENT macro scope | Defer to spec |

### Format & Data

- Runtime format: JSON blueprints (entity prefabs)
- USD: concepts adopted (typed entity + applied component bundles), library not taken. Clean path to future USD import via swappable loader interface.
- Blueprint loader is interface-based (JsonBlueprintLoader now, potential UsdBlueprintLoader later)
- No format-specific assumptions in diaentitytemplate public API

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

The research identified two generic capabilities that should be spec'd independently of diaentitytemplate:

### HandlePool<T> — DiaCore addition (not a new module)

Generic freelist + generation bump allocator. Companion to existing `DiaCore/Containers/Handle.h`. Any system that manages pooled objects (physics, render, audio, entities) uses this. diaentitytemplate's entity storage IS a `HandlePool<Entity>`.

### DiaMailbox — New module

Typed deferred messaging with structured addressing. Not entity-specific — any system can use it (module-to-module, editor-to-game, etc.). diaentitytemplate is one consumer. Depends on DiaCore only.

**Dependency chain:**
```
DiaCore (Handle<T> + HandlePool<T>)
  ↑
DiaMailbox (typed deferred messaging)
  ↑
diaentitytemplate (World, components, queries, inspection)
```

## Addendum — 2026-05-17

After reviewing the Dragon ECS engineering design doc against this research, the chosen direction holds. The doc describes a production-grade archetype-chunk ECS (Unity DOTS / flecs class) which was explicitly evaluated as candidate 4 and rejected (score 2.55 vs. chosen 4.15). Scale, parallelism, and chunk-cache locality are not our priorities; system-owned data with handle-orchestration is.

Four points from the doc are worth lifting into our model. Each is captured below as an additional locked decision.

### Decision 14 — Reflection metadata for components

| # | Decision | Choice |
|---|----------|--------|
| 14 | Component reflection | Required from foundation. `DIA_COMPONENT` macro emits a static `ComponentTypeDesc` with name, size/alignment, schema version, and field list (name + offset + type + default). Supported field types curated (primitives, StringCRC, math types, asset handles, entity refs). Reflection drives JSON load/save, editor inspection, and prefab schema migration. |

**Why:** Without reflection, every component needs a hand-written serializer (DiaSerializer is transport, not schema) and a hand-written editor inspector. This compounds badly. The doc makes reflection foundational; we should too.

**Scope discipline:** Lives **inside diaentitytemplate** as a feature, not a generic engine-wide `DiaReflection`. If a second consumer appears later, lift it then. DiaSerializer remains the transport (MetadataValue, JSON helpers); reflection is the schema layer above it.

**Impact on feature order:** Reflection is a foundation-adjacent feature, sequenced after the entity/component skeleton but before blueprint loading and inspection (both depend on it).

### Decision 15 — Hierarchy

| # | Decision | Choice |
|---|----------|--------|
| 15 | Parent/child hierarchy | Supported via `Parent` and `ChildBuffer` components in v1. Destroy-cascade default: destroy subtree. Cycles rejected in debug builds. Re-parenting goes through the same end-of-frame mutation pipeline as other structural changes. |

**Why:** Attached weapons, particle emitters on characters, scene grouping, transform propagation — all need parent/child. Adding it later means retrofitting the mutation pipeline and reference-resolution rules. Cheaper to commit now.

**Scope discipline:** Parent/child only. No general "relationships" graph (the doc's own non-goal for v1 too). No transform propagation system in diaentitytemplate itself — that lives in whichever system owns transforms, diaentitytemplate just provides the hierarchy data.

### Decision 16 — Container rename: `World` → `Realm`

| # | Decision | Choice |
|---|----------|--------|
| 16 | Entity container name | `Realm` (replaces "World" from earlier sections of this research). Avoids overlap with Stage. `EntityModule` owns one `Realm`; `Realm` is a plain value type, not a singleton. |

**Why:** "World" reads ambiguously next to Stage/Module/PU. `Realm` is short, unambiguous, and reads well in API (`realm.CreateEntity()`, `EntityModule::GetRealm()`).

**Multi-realm note (not a decision):** Runtime is single-realm-per-stage. The API shape should not preclude editor-preview, test-fixture, or snapshot realms later — keep `Realm` constructible as a normal object, don't hide it inside `EntityModule`'s private state, don't make it a singleton. No further commitment beyond this.

### Decision 17 — Debug editing tiers

| # | Decision | Choice |
|---|----------|--------|
| 17 | Editor mutation capability | Three tiers: (a) read-only inspection — v1 (already covered by `IEntityInspectable`); (b) live field edit via reflection setters — v1 goal, falls out of Decision 14; (c) live structural edit (add/remove component, spawn/destroy entity, re-parent at runtime) — deferred. The end-of-frame mutation pipeline must remain capable of routing editor-originated commands when (c) is built. |

**Why:** Stage-boundary reload IS the primary content-iteration loop — edit JSON, transition stage, see new state. Live structural edit is for runtime probing (AI tuning, gameplay tweaking), not content iteration. Building (c) too early means designing it before we know what the editor actually needs.

**Stage-boundary reload as primary iteration:** Acknowledged here as the design intent. Live edit serves debugging, not authoring.

### Updated Feature Implementation Order

Feature order revised to surface reflection earlier (it's a foundation-adjacent dependency for blueprints and inspection):

1. Remove old IComponent infrastructure (clean slate)
2. Foundation (Realm, Entity, generational IDs, component base class)
3. **Reflection metadata** (`DIA_COMPONENT` macro emits `ComponentTypeDesc` with field list)
4. Blueprint loading (JSON loader uses reflection — assets)
5. Component deps + reference resolution
6. Hierarchy (`Parent` + `ChildBuffer`, destroy-cascade, cycle rejection)
7. Mailbox integration (communication via DiaMailbox)
8. Query system (dynamic + cached, end-of-frame rebuild)
9. IEntityInspectable (uses reflection for field display + setters → live field edit)
10. EntityModule adapter (application integration)

## Next Step

Run /spec-system for each in dependency order:

1. **HandlePool<T>** — small DiaCore feature spec (addition to Containers/)
2. **DiaMailbox** — system spec (new module)
3. **diaentitytemplate** — system spec (new module, depends on above two)
