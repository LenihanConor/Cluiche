# Research Summary — Entity System

**Session folder:** docs/research/entity_system/
**Date:** 2026-05-09

## One-Line Answer

A layered ECS where components are config + asset trigger + gameplay interface into independent systems, with editor-first inspection, deferred mailbox communication, and cached queries — all housed in a standalone DiaEntity module.

## Journey

1. **Explored:** Surveyed the problem space of game entity systems. Found the existing IComponent infrastructure is barely used (2 consumers) and philosophically opposed to the target architecture (entity-owns-data vs. system-owns-data). DiaApplicationFlow v2's Stage model provides the natural lifecycle boundary.
2. **Ideated:** 10 candidates generated spanning minimal handle registries to full archetype ECS to reactive signal graphs. Ranged from S (1 week) to L (2 months).
3. **Evaluated:** Candidates 3 (Component Bindings + Interfaces) and 8 (Editor-First Inspection) tied at 4.15. Archetype ECS and Signal Graph scored lowest due to complexity/cost vs. stated priorities.
4. **Chose:** Merged 3+8 confirmed by user. Extended with decisions on identity (generational index), communication (typed mailbox + structured addressing), queries (dynamic + cached, end-of-frame rebuild), typed reference slots, and blueprint schema (versioned JSON).

## Chosen Work Item

**Name:** DiaEntity — Layered ECS with Component Interfaces + Editor-First Inspection
**Home module:** New `Dia/DiaEntity/` (standalone, no DiaApplicationFlow dependency)
**Suggested spec type:** System (with multiple feature specs for each layer)
**Estimated size:** M (1–3 weeks for foundation; queries and editor inspection layer on after)

## Key Insights from Exploration

- **Entity is gameplay glue, not a universal base class.** Systems own their data. Entity holds handles. This is the foundational constraint.
- **Components are three things:** config (JSON values), asset trigger (config drives loads), and interface (gameplay API into a system). Plus opt-in DoUpdate for self-contained behavior.
- **USD validates the model** but the library is overkill. Applied API Schemas = components. Prims = entities. Take the concepts, keep the door open via swappable loader interface.
- **Deferred everything:** messages async over one frame, query caches rebuild end-of-frame, references resolve in a pass after creation. Consistency over immediacy.
- **Editor-first means introspectable data design:** generational IDs, named types (StringCRC), structured addresses, versioned schemas. Not bolted on after.
- **Old IComponent must die first** — it's philosophically opposed and barely used. Clean slate.
- **Generic systems extracted:** HandlePool<T> belongs in DiaCore (already has Handle<T>). DiaMailbox is a standalone module (any system can use deferred messaging). DiaEntity depends on both but neither depends on entities.

## Dependency Chain

```
DiaCore (Handle<T> exists + HandlePool<T> new)
  ↑
DiaMailbox (new module — typed deferred messaging, structured addressing)
  ↑
DiaEntity (new module — World, components, queries, IEntityInspectable)
  ↑
Application code (EntityModule adapter plugging World into DiaApplicationFlow v2 stages)
```

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| Handle Registry (Minimal) | Foundation layer only — no communication, no editor visibility |
| Archetype-Based ECS | Optimizes for wrong priorities (parallelism, cache). 2-month effort. Over-engineered for <1000 entities. |
| Entity-as-Module | Conflates gameplay entities with infrastructure modules. Doesn't scale. |
| World + Blueprints (SystemAdapter) | Adapter abstraction adds unnecessary middleman. Component calls system directly. |
| Signal Graph (Reactive) | Too complex, exotic pattern, 2-month effort, hard to debug |
| Entity Table (Spreadsheet) | Too rigid (fixed columns per stage). No communication. |

## References

- docs/research/entity_system/explore.md
- docs/research/entity_system/ideate.md
- docs/research/entity_system/evaluate.md
- docs/research/entity_system/choose.md
