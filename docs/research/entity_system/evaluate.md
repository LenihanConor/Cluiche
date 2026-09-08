# Research: Evaluate — Entity System

**Input:** docs/research/entity_system/ideate.md

## Refined Design Model (from discussion)

Before scoring, the discussion converged on a specific ECS model:

- **Entity** = generational ID + set of components, stage-scoped
- **Component** = config (per-entity JSON data) + asset trigger (references drive loads) + interface (gameplay API delegating to system via handle)
- **Systems** = independent (physics, rendering, animation) — own their data, don't know entities exist
- **Entity prefabs are assets** — loaded by AssetService, parsed into entities, component configs trigger further asset loads
- **Stage-boundary lifecycle** — entities created on stage enter, destroyed on stage exit
- **Editor-first** — all entity state introspectable at runtime
- **Layered** — candidates stack: foundation → typed components → communication → blueprints → queries → editor inspection

## Scoring Criteria

- **Engine Value (0.25)**: Improves Dia module reusability or capability — how well does this serve as a general-purpose entity system any game could use?
- **Game Value (0.20)**: Improves CluicheTest as a demo or testbed — can DummyStage use this immediately?
- **Implementation Cost (0.25)**: Inverse of effort — 5 = very cheap, 1 = very expensive
- **Risk (0.15)**: Inverse of uncertainty — 5 = well-understood, 1 = highly uncertain
- **Cluiche Fit (0.15)**: Aligns with module structure, PD-001 through PD-007, DiaApplicationFlow v2, and the refined ECS model above

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| 1. Handle Registry (Minimal) | 3 | 2 | 5 | 5 | 4 | 3.70 |
| 2. Handle Registry + Mailbox | 4 | 3 | 4 | 4 | 4 | 3.80 |
| 3. Component Bindings + Interfaces | 5 | 4 | 3 | 4 | 5 | 4.15 |
| 4. Archetype-Based ECS | 4 | 3 | 1 | 2 | 2 | 2.55 |
| 5. Entity-as-Module | 3 | 3 | 4 | 3 | 3 | 3.30 |
| 6. World + Blueprints (Data-Driven) | 4 | 4 | 3 | 4 | 5 | 3.90 |
| 7. Signal Graph (Reactive) | 3 | 2 | 1 | 2 | 2 | 2.15 |
| 8. Handle Registry + Inspector (Editor-First) | 5 | 4 | 3 | 4 | 5 | 4.15 |
| 9. Slim ECS with Queries | 4 | 3 | 3 | 3 | 4 | 3.45 |
| 10. Entity Table (Spreadsheet) | 2 | 2 | 5 | 4 | 3 | 3.20 |

## Top 3 Candidates

### Rank 1 (tied): Candidate 3 — Component Bindings + Interfaces (score: 4.15)

**Why:** This IS the refined model from discussion. Components as config + asset trigger + interface maps directly to what was described. Each component type is a typed adapter with a narrow gameplay API over a system handle. Dependencies between components are declared and validated. JSON prefabs list component types and their config. Fits perfectly with PD-001 (StringCRC component type IDs), PD-004 (DiaCore containers), and DiaApplicationFlow v2 (EntityModule is stage-scoped, returns kLoading until assets resolve).

**Watch out for:** Interface design per component type requires thoughtful API decisions — how much does `PhysicsComponent` expose vs. hide? Need to define what "narrow interface" means in practice. Also needs a registration pattern for component types (similar to DIA_MODULE).

### Rank 1 (tied): Candidate 8 — Handle Registry + Inspector (Editor-First) (score: 4.15)

**Why:** Same core as Candidate 3 but explicitly designed around editor visibility from day one. IEntityInspectable gives the editor: entity list with search/filter, per-entity handle bindings with validity, message log, blueprint diff. All metadata (debug names, prefab source, tags) is first-class. Matches the stated priority of debugging & editor visibility being key.

**Watch out for:** Slightly more metadata overhead at runtime (debug names, timestamps, message history). Conditional compilation for release builds adds #ifdef complexity — but this is a known solved pattern.

### Rank 3: Candidate 6 — World + Blueprints (score: 3.90)

**Why:** The SystemAdapter pattern provides the cleanest boundary between entity system and independent systems. Each system registers an adapter that knows how to `CreateFromConfig(json) → handle` and `Destroy(handle)`. This is exactly the "config + asset trigger" model — the adapter IS the component factory. Blueprint files are the prefabs. Editor shows blueprint vs. instance diff.

**Watch out for:** Adapter pattern adds one more abstraction layer. Is the Adapter a separate thing from the Component, or are they the same? If separate, there's a question of where gameplay interface logic lives (component or adapter?).

## Recommendation

**Candidates 3 and 8 should be merged** — they're the same system with different emphasis. The result is a layered ECS with:

- **Foundation**: Generational entity IDs, stage-scoped World owned by EntityModule
- **Components**: Typed adapters (config + asset trigger + interface) registered via macro, resolved via StringCRC
- **Blueprints**: JSON prefabs that are assets themselves; component configs trigger further asset loads
- **Communication**: Deferred mailbox for safe inter-entity messaging
- **Inspection**: IEntityInspectable for editor — entity list, component state, message log, prefab diff
- **SystemAdapters** (from Candidate 6): Clean boundary — systems register an adapter that creates/destroys objects from config

This merged candidate satisfies all stated priorities:
1. Simple — components are just config + interface, no archetype machinery
2. Editor-first — IEntityInspectable baked in, not bolted on
3. Safe communication — deferred mailbox, generational handles prevent dangling
4. JSON data — prefabs ARE assets, component configs are JSON
5. Stage-boundary reload — World destroyed on stage exit, recreated on enter
6. Systems stay independent — adapters are the only coupling point (PD-003 replacement)

It's implementable in layers (foundation first, then communication, then editor) which de-risks the M-sized effort.
