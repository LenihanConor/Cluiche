# Research: Explore — Entity System

**Session date:** 2026-05-09
**Folder:** docs/research/entity_system/

## Problem Space Overview

Game entities are the runtime representation of "things" in a game world — characters, projectiles, pickups, triggers. The challenge is designing a system that lets gameplay code compose these things from independent subsystem objects (a physics body, a sprite, a state machine) while keeping subsystems decoupled from each other and from the entity layer itself.

The Cluiche engine already has a component architecture (`IComponent`/`IComponentObject`/`IComponentFactory`) but it's low-level infrastructure, not a gameplay entity system. It lacks inter-entity communication, serialization, querying, and — critically — it couples component storage to the entity rather than letting systems own their own data. The goal is a gameplay-level entity system that orchestrates across independent systems without creeping into them.

The key architectural insight driving this research: **entities are gameplay wiring, not a universal base class**. Physics keeps its own rigid bodies. Rendering keeps its own draw objects. The entity system maps handles between them so gameplay can say "the thing with this physics body is also the thing with that sprite." Each system remains testable, usable, and ownable without knowing entities exist.

### Design Priorities (user-stated)

**High priority:**
1. Simplicity — easy to understand and use
2. Debugging & editor visibility — see entity state, handles, and wiring at runtime in the editor
3. Safe inter-entity communication — no dangling references, no torn state
4. JSON data format — prefabs and scene data in human-readable JSON
5. Stage-boundary reload — no hot reload complexity; fresh load on stage transition
6. Gameplay wiring — components are interfaces into systems, not data stores

**Low priority:**
- Parallelism (single-threaded entity logic is fine)
- Raw performance (can miss a frame for safe communication)
- Complexity to debug (but editor tooling makes this less relevant)

## Existing Approaches

- **Classic ECS (Archetype-based)**: Unity DOTS, flecs, EnTT. Entities are IDs; components are plain data stored in archetype tables; systems iterate over component combinations. Optimized for cache-coherent iteration over thousands of entities.
- **Classic ECS (Sparse-set)**: EnTT's model. Entity→component mapping via sparse arrays. Fast add/remove, good iteration, simpler than archetypes.
- **Handle-based composition**: Each subsystem issues opaque handles. A registry maps entity IDs to a set of handles. No subsystem knows about entities — only the registry does.
- **Object model with components**: Unity (MonoBehaviour era), Unreal (AActor/UActorComponent). Entity is a concrete object that owns component instances. Components can reference each other via the owning entity.
- **Reactive/event-driven**: Entities communicate via message bus or signals. Components subscribe to events rather than polling shared state.
- **Data-oriented with relations**: flecs relations, Bevy's entity relations. First-class parent-child, "likes", "targets" relationships between entities.
- **Prefab/archetype templates**: Define entity templates (prefabs) as data, instantiate at runtime. Common in data-driven engines.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **Ownership model** | Entity owns components / Systems own data, entity holds handles / Hybrid | User wants systems to own their own data — entity is orchestration glue |
| **Identity** | Integer ID / Generational index / StringCRC | Generational index prevents dangling references; StringCRC fits PD-001 for named entities |
| **Component storage** | Inline on entity / System-local pools / Archetype tables / Sparse sets | Systems owning data means system-local storage |
| **Communication** | Direct query / Event bus / Mailbox / Deferred queue | Safety matters more than latency; deferred is safest |
| **Serialization** | JSON per-entity / JSON scene graph / Binary | User wants JSON; stage-boundary reload not hot reload |
| **Lifecycle** | Immediate create/destroy / Deferred (end-of-frame) / Stage-boundary only | Stage-boundary reload simplifies lifecycle enormously |
| **Querying** | By component type / By tag / By archetype / No query (explicit wiring) | Explicit wiring is simplest; querying adds power but complexity |
| **Template/prefab** | JSON prefab files / Code-defined archetypes / None | JSON prefabs align with data format preference |
| **Hierarchy** | Flat / Parent-child transforms / Arbitrary relations | Flat is simplest; parent-child common for scene graphs |
| **Inspectability** | Opaque runtime only / Named+typed handles / Full reflection | Editor needs to display entity state — must be introspectable at minimum |
| **Editor integration** | Read-only inspection / Live editing / Prefab editing + inspection | CluicheEditor uses IApplicationInspectable pattern; entity system needs equivalent |

## Known Tradeoffs

- **Handle indirection vs. direct pointers**: Handles are safe (can validate) but add a lookup cost per access. Acceptable given "can miss a frame" tolerance.
- **System independence vs. convenience**: Keeping systems ignorant of entities means more boilerplate wiring in gameplay code, but much better modularity and testability.
- **Deferred communication vs. immediate**: Deferred is safer (no re-entrant mutation) but adds latency. One-frame delay is acceptable per user constraints.
- **JSON serialization vs. performance**: JSON is human-readable and debuggable but slow to parse for large scenes. Fine for stage-boundary reload.
- **Simplicity vs. query power**: A minimal system (entity = bag of handles) is easy to understand but requires gameplay code to know what it's looking for. Query systems ("give me all entities with Physics+Sprite") add expressiveness but implementation complexity.
- **Flat entity list vs. hierarchy**: Flat is dead simple but forces manual parent-child management for things like attached weapons, particle emitters on characters.
- **Inspectability vs. encapsulation**: Making all entity state visible to editor/debugger means the data model must be introspectable (type-tagged handles, named bindings). This is a feature, not a cost — it forces good data design.
- **Editor-first vs. runtime-first**: Designing for editor visibility from day one means entities carry metadata (names, prefab source, debug tags) that pure-runtime wouldn't need. Small memory cost, huge iteration speed gain.

## Known Pitfalls (C++ / game engine context)

- **Dangling handles**: If a system destroys its object but the entity still holds a handle, you get use-after-free. Generational indices solve this.
- **Initialization order**: If entity setup touches multiple systems, order matters. A two-phase init (allocate handles, then wire) avoids partial-init states.
- **Teardown cascades**: Destroying an entity must notify all systems to release their objects. Forgetting one system leaks resources.
- **Over-engineering ECS for small games**: Full archetype ECS (like flecs) is complex machinery optimized for >10k entities. Most indie/small games have <1000. The overhead isn't worth it.
- **Component dependencies**: "Sprite needs a Transform" — implicit dependencies between system objects create hidden coupling if not managed.
- **Thread safety with deferred ops**: If systems run on different threads (Sim vs Render), deferred entity operations need synchronization at phase boundaries.
- **Template/generic explosion in C++**: Heavy template metaprogramming for component type lists creates long compile times and opaque error messages.

## Cluiche-Specific Opportunities

### Existing IComponent System — Removal Candidate

The old `IComponent`/`IComponentObject`/`IComponentFactory` infrastructure in `DiaCore/Architecture/Components/` is **barely used** and can be removed as part of this work:

| Consumer | What it does | Migration path |
|----------|-------------|----------------|
| `DiaStateMachine/StateMachineComponent.h` | Wraps a state machine pointer | System owns its own objects directly — no component wrapper needed |
| `DiaRig2D/SkeletonComponent.h` | Stores Skeleton + Pose | Same — DiaRig2D owns skeletons in its own pool |
| `Tests/GoogleTests/Core/Architecture/TestComponent.cpp` | Unit tests for the pattern | Remove alongside the infrastructure |
| `Tests/GoogleTests/Rig/TestSkeletonComponent.cpp` | Tests SkeletonComponent | Rewrite to test DiaRig2D directly |

The old system couples component storage to the entity object. The new entity system inverts this: **systems own their data, entity holds handles into systems**. The old infrastructure should be removed — it's philosophically opposed to the direction we're going.

### DiaApplicationFlow v2 Integration (Stages, not Phases)

DiaApplicationFlow v2 (approved 2026-05-08) replaces Phases with config-declared **Stages**. Key implications for entity system design:

- **Stage transitions are the entity reload boundary** — when TransitionTo fires, all non-retained modules stop/start. Entity state is created fresh from JSON prefab data per stage.
- **Modules are the integration point** — an `EntityModule` (or `WorldModule`) lives in the SimPU and manages the entity registry for its stage.
- **Streams handle cross-PU data** — entities live in SimPU; rendering info flows to RenderPU via SimToRender stream. No entities in RenderPU.
- **"all" stage modules persist** — infrastructure modules (TimeServer, InputStream) persist across stages. Entity module is stage-scoped (destroyed on transition).
- **Config is source of truth** — entity prefab definitions could be referenced from `.diastage` manifests or loaded by the EntityModule during DoStart.

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaApplicationFlow v2 | PU/Stage/Module lifecycle — entity module lives in SimPU, scoped to a Stage |
| DiaCore/Containers | DynamicArrayC, HashTable — storage for entity registry and handle maps (PD-004) |
| DiaCore/StringCRC | Entity type names, handle type IDs, prefab identifiers (PD-001) |
| DiaRigidBody2D | Physics bodies — independent system that entity layer wires handles into |
| DiaRig2D | Skeleton/animation — independent system, entity maps a handle to a skeleton |
| DiaStateMachine | State machines — independent system, entity maps a handle to an FSM |
| DiaGraphics | Rendering — independent system, draw objects owned by renderer |
| DiaSerializer | JSON loading — entity prefabs loaded via existing serialization |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | Entity type names, handle type IDs, and prefab identifiers should use StringCRC — not raw strings |
| PD-002 (updated) PU/Stage/Module | Entity lifecycle integrates with Stage transitions; EntityModule is stage-scoped in SimPU |
| PD-003 Component system | **Being superseded.** Old IComponent pattern removed. New entity system replaces its role at gameplay level. Systems own their own objects. |
| PD-004 No STL in public APIs | Entity registry, handle maps, component arrays must use DiaCore containers (HashTable, DynamicArrayC) |
| PD-005 x64 Windows only | No portability constraints on memory layout |
| PD-007 C++20 | Can use concepts for component type constraints, std::span for views, constexpr for compile-time type registration |
| PD-010 .diagame/.diastage | Stage files define what gets loaded; entity prefab data could be referenced from .diastage or loaded independently by EntityModule |

## Open Questions for Ideation

- Should the entity registry be a Module (stage-scoped in SimPU) or a standalone service?
- How do entities reference system objects — opaque handle, typed handle, or index + generation?
- Should there be a "World" concept that scopes entities to a stage, or is stage-boundary lifecycle enough?
- How does gameplay code wire entity creation — code-only, JSON prefabs, or both?
- What does inter-entity communication look like — direct handle lookup, event dispatch, mailbox, or something else?
- Should entity "components" (handle bindings) declare dependencies ("I need a physics handle") or is that purely gameplay-code responsibility?
- Old IComponent infrastructure is being removed — does the new system live in DiaCore or a new DiaEntity module?
- Is there a need for entity "tags" or "groups" for bulk operations (e.g., "all enemies")?
- How are entity templates versioned across stage reloads — always recreated from prefab data?
- How do systems (physics, rendering) communicate destruction back to the entity layer — callback, poll, or generational handle invalidation?
- What does editor inspection look like — does EntityModule expose an IEntityInspectable interface similar to IApplicationInspectable?
- Should entity debug names and prefab source be retained at runtime (for editor) or stripped in release?
- How does the editor display entity handle bindings — flat list, graph, or tree?
- Can the editor create/destroy entities at runtime (live edit) or only inspect?
