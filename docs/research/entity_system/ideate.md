# Research: Ideate — Entity System

**Input:** docs/research/entity_system/explore.md

## Candidates

### Candidate 1: Handle Registry (Minimal)

**Home module/system:** New `DiaEntity` module in Dia/
**Size:** S (≤1 week)

**Description:** The simplest possible entity system. An entity is a generational ID. A `World` class (owned by an EntityModule, stage-scoped) maps entity IDs to named typed handles. Each handle is an opaque {system_id, index, generation} tuple. Systems don't know entities exist — gameplay code manually allocates system objects and registers the returned handles with an entity. JSON prefabs list handle types and creation params. Destruction walks the handle map and calls system-specific destroy functions.

No communication, no querying, no hierarchy. Entities are dumb bags of handles with debug names. Editor inspects via a flat list of entities + their handle bindings.

**Primary value:** Absolute minimum complexity — a starting point that can grow later without rework.

---

### Candidate 2: Handle Registry + Mailbox Communication

**Home module/system:** New `DiaEntity` module in Dia/
**Size:** M (1–3 weeks)

**Description:** Builds on Candidate 1 by adding a deferred **mailbox** system for safe inter-entity communication. Each entity has an inbox (a small ring buffer). Messages are {sender_id, message_type (StringCRC), payload_ptr, payload_size}. Messages are delivered end-of-frame — no mid-update mutation. Entities process their inbox at the start of their update. Sending to a dead entity silently drops the message (generational ID check).

Editor shows message flow: who sent what to whom last frame. Prefabs still JSON. Systems still own their data. Communication is one-frame-delayed but safe.

**Primary value:** Safe, inspectable inter-entity communication without coupling systems together.

---

### Candidate 3: Component Bindings with Interface Contracts

**Home module/system:** New `DiaEntity` module in Dia/
**Size:** M (1–3 weeks)

**Description:** Entities hold **component bindings** — typed wrappers around system handles that expose a narrow interface. Example: `PhysicsBinding` wraps a physics body handle and exposes `GetPosition()`, `ApplyForce()`. The binding is the gameplay interface; the system owns the data. Bindings are registered with the entity via StringCRC type IDs.

Bindings declare **dependencies** ("I require a TransformBinding"). The entity validates all deps are satisfied at creation. JSON prefabs list binding types + config. Editor shows the binding graph per entity.

Inter-entity communication: entities query each other's bindings by type. Safe because bindings validate their handle is still alive (generational check) and return null/default if not.

**Primary value:** Typed, safe gameplay interfaces over system data — editor can show the dependency graph and binding state.

---

### Candidate 4: Archetype-Based ECS (Data-Oriented)

**Home module/system:** New `DiaECS` module in Dia/
**Size:** L (1–2 months)

**Description:** Full archetype-based ECS. Entities are IDs. Components are plain structs stored in archetype tables (contiguous arrays grouped by component combination). Systems are functions that iterate over entities matching a component query. World manages archetype transitions (add/remove component moves entity between tables).

JSON serialization of component data. Editor shows archetype breakdown, entity component values, system iteration order. Communication via shared component data or events dispatched through the World.

**Primary value:** Cache-friendly iteration over large entity counts; well-understood pattern from Unity DOTS / flecs.

---

### Candidate 5: Entity-as-Module (Heavyweight)

**Home module/system:** `DiaApplicationFlow` extension — entities ARE modules
**Size:** M (1–3 weeks)

**Description:** Each entity is a lightweight Module subclass that participates in the DiaApplicationFlow v2 lifecycle. Entity-modules have DoStart (create system handles from prefab config), DoUpdate (gameplay tick), DoStop (release handles). They're declared in stage manifests alongside regular modules. Inter-entity communication uses EventStream (same as inter-module).

Editor already inspects modules via IApplicationInspectable — entities get inspection for free. JSON config per entity-module in the manifest.

**Primary value:** Zero new infrastructure — reuses DiaApplicationFlow v2 lifecycle, inspection, and streams entirely.

---

### Candidate 6: World + Blueprints (Data-Driven)

**Home module/system:** New `DiaEntity` module in Dia/
**Size:** M (1–3 weeks)

**Description:** A `World` (stage-scoped, owned by EntityModule) manages entities. Entities are created from **Blueprints** — JSON files that declare a set of named component slots with their system type and config. At runtime, the World reads a blueprint, allocates system objects via registered **SystemAdapters** (one per system: physics adapter, render adapter, etc.), and stores the returned handles.

SystemAdapters are the only coupling point — they implement `CreateFromConfig(json) → handle` and `Destroy(handle)`. Systems never see entities. Editor shows blueprints as templates, live entities as instances with overridden values. Communication via World-mediated queries (ask World for entity X's physics handle).

**Primary value:** Clean data-driven pipeline with explicit system boundary (adapters). Editor can show blueprint vs. instance diff.

---

### Candidate 7: Signal Graph (Reactive)

**Home module/system:** New `DiaEntity` module in Dia/
**Size:** L (1–2 months)

**Description:** Entities are nodes in a reactive signal graph. Each entity has **ports** (typed input/output connections). Outputs from one entity connect to inputs of another. When a port value changes, downstream connections update (deferred to end-of-frame for safety). Systems expose their state as output ports (physics body emits position).

Editor shows the signal graph visually — connections between entities are first-class. JSON defines the graph topology. Debugging shows signal propagation frame by frame.

**Primary value:** Maximum editor visibility — the wiring IS the gameplay logic, fully visual and inspectable.

---

### Candidate 8: Handle Registry + Inspector Interface (Editor-First)

**Home module/system:** New `DiaEntity` module in Dia/
**Size:** M (1–3 weeks)

**Description:** Like Candidate 2 (handle registry + mailbox) but designed **editor-first**. Every entity carries: debug name, prefab source path, creation timestamp, a tag set (StringCRC tags for grouping). All handle bindings are named and typed. The module exposes `IEntityInspectable` — mirrors `IApplicationInspectable` — which the editor polls to display:

- Entity list with search/filter by name/tag/prefab
- Per-entity: all handle bindings with system name + handle validity + last-accessed frame
- Message log: last N messages sent/received per entity
- Blueprint diff: current state vs. prefab defaults

Communication via deferred mailbox (same as Candidate 2). JSON blueprints with named handle slots. Destruction notifies all bound systems. Generational handles for safety.

**Primary value:** Editor gets deep visibility into entity state, communication, and data provenance without runtime performance cost in release (metadata conditionally compiled).

---

### Candidate 9: Slim ECS with Component Queries

**Home module/system:** New `DiaEntity` module in Dia/
**Size:** M (1–3 weeks)

**Description:** A middle ground between full archetype ECS (Candidate 4) and the handle registry (Candidate 1). Entities are generational IDs. Components are lightweight structs that hold a system handle plus optional cached data. Components stored in per-type sparse arrays. The World supports **queries** — "give me all entities with Physics + Sprite" — returning an iterable view.

No archetype tables (simpler). No cache optimization (acceptable per priorities). But queries enable gameplay patterns like "damage all enemies in radius" without maintaining manual lists. JSON prefabs declare component types + config. Editor shows queries and their results.

**Primary value:** Query power without archetype complexity; systems still own their data via handles in components.

---

### Candidate 10: Entity Table (Spreadsheet Model)

**Home module/system:** New `DiaEntity` module in Dia/
**Size:** S (≤1 week)

**Description:** Entities are rows in a table. Columns are handle types (Physics, Sprite, FSM, etc.). Each cell is either empty or holds a handle. The table IS the data structure — a 2D array of optional handles indexed by [entity_id][column_type]. Fixed column set per stage (declared in JSON). No dynamic component add/remove at runtime.

Editor shows the table directly — literally a spreadsheet view. Filter by column occupancy. Sort by any column. Communication via direct table lookup (entity A reads entity B's physics handle from the table). Safe because handles are generational.

**Primary value:** Radically simple mental model — the editor IS the data structure. No abstraction layers to peek through.

## Coverage Map

The candidates span the design axes from explore.md:

- **Ownership**: All candidates keep systems owning data (constraint). Candidate 5 (entity-as-module) is the exception — it reuses framework ownership.
- **Complexity range**: S (Candidates 1, 10) through L (Candidates 4, 7). Most cluster at M.
- **Communication**: None (1, 10) → Mailbox (2, 8) → Query (3, 9) → Signals (7) → Streams (5)
- **Editor visibility**: Low (1, 4) → Medium (2, 3, 5, 6, 9) → High (7, 8, 10)
- **Data-driven**: All support JSON. Candidate 6 (Blueprints) is most explicitly data-pipeline focused.
- **Inspectability**: Candidate 8 is designed editor-first. Candidate 10's table IS the inspector. Candidate 7's graph IS the editor view.
