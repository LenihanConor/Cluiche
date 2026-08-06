# System Spec: DiaEntitySpawner

## Parent Application
@docs/specs/applications/dia/dia.md

**Status:** Approved
**Plan:** @docs/specs/applications/dia/systems/diaentityspawner/diaentityspawner.plan.md
**Gameplay Domains:** entity, simulation

---

## Summary

DiaEntitySpawner is the runtime entity creation and lifecycle management system for Dia. It handles dynamic entity instantiation from blueprints — distinct from DiaScene2D/3D which owns static, edit-time scene placement. Two complementary surfaces: a direct programmatic API (`SpawnRequest` / `Despawn`) for systems like DiaEconomy and DiaHTN that create entities by code, and a data-driven `SpawnEmitterComponent` that authors place on scene entities to produce rate-based, burst, or wave spawning behaviour without code.

Spawned entities are full first-class domain citizens — they carry all components declared in their blueprint and interact with DiaEntitySpatial, DiaBlackboard, DiaRigidBody2D, and every other domain system immediately after their end-of-frame commit.

---

## Goals

1. Provide a clean, minimal API for one-shot entity spawning from a blueprint ID and position.
2. Support data-driven `SpawnEmitterComponent` for rate/burst/wave spawn policies without bespoke C++ per emitter.
3. Own despawn: explicit `Despawn(handle)`, and automatic despawn conditions (lifetime, distance, cap overflow).
4. Publish observable events (`OnEntitySpawned`, `OnEntityDespawned`) on the sim stream.
5. Full DiaObservation coverage: logs, traces, metrics, health.
6. Exhaustive GoogleTest suite covering all policies and despawn conditions.
7. CluicheTest E2E visual stage demonstrating spawning and despawning in action.

## Non-Goals

- **Object pooling** — reuse of despawned entity slots is deferred; DiaEntitySpawner always uses `Domain::CreateEntity` / `Domain::DestroyEntity`. A pooling extension can be added as a child feature spec.
- **Editor visualisation** — spawn radius, live count overlays, emitter gizmos are a follow-up system (`DiaEntitySpawnerVisualDebugger`).
- **Wave tables** — named wave definitions (unit-type ratios, difficulty scaling) are game-layer concerns, not engine. Game code configures `SpawnEmitterComponent` fields directly or via DiaRules.
- **Blueprint authoring** — `DiaEntityBlueprintEditor` owns that.
- **Scene placement** — DiaScene2D/3D continues to own static scene entity hydration; `SpawnEmitterComponent` is what makes a placed entity dynamic.

---

## Architecture

### SpawnRequest API

```cpp
namespace Dia::Entity {

struct SpawnRequest {
    StringCRC       blueprintId;   // resolves via IBlueprintLoader
    Vec2f           position;      // world-space spawn origin
    Entity          parent;        // Entity::Invalid() for no parent
    StringCRC       tag;           // optional label for query/filter
};

struct SpawnResult {
    Entity          entity;        // Entity::Invalid() on failure
    SpawnError      error;         // None / BlueprintNotFound / DomainFull
};

class IEntitySpawner {
public:
    virtual SpawnResult Spawn(const SpawnRequest& request) = 0;
    virtual void        Despawn(Entity entity) = 0;
};

} // namespace Dia::Entity
```

`IEntitySpawner` is exposed as a module service on `EntitySpawnerModule`. Call sites obtain it via the standard `ModuleRef<EntitySpawnerModule>` pattern — no singleton, no service locator.

### SpawnEmitterComponent

Data-driven component; placed on any domain entity (including scene-loaded ones):

```cpp
DIA_COMPONENT(SpawnEmitterComponent) {
    FIELD(StringCRC,  blueprintId);
    FIELD(float,      rate);          // entities per second; 0 = manual/burst only
    FIELD(int,        burstCount);    // entities emitted on Activate(); 0 = none
    FIELD(int,        cap);           // max live children; 0 = unlimited
    FIELD(float,      lifetime);      // seconds before auto-despawn; 0 = infinite
    FIELD(float,      despawnRadius); // despawn if child > radius from emitter; 0 = off
    FIELD(bool,       active);        // runtime toggle
};
```

`EntitySpawnerModule::Update(dt)` iterates all `SpawnEmitterComponent` instances each sim tick, accumulates `rate * dt` tokens, fires spawns when tokens ≥ 1, and enforces `cap`. Children are tracked in an internal table keyed by emitter entity.

### Despawn Conditions

Evaluated in `EntitySpawnerModule::Update(dt)` for all tracked children:

| Condition | Trigger |
|-----------|---------|
| Lifetime  | Child age exceeds `SpawnEmitterComponent::lifetime` |
| Radius    | Child distance from emitter exceeds `despawnRadius` |
| Cap overflow | New spawn would exceed `cap` — oldest child despawned first (FIFO) |
| Explicit  | Caller invokes `IEntitySpawner::Despawn(entity)` |

### Events

Published to the sim-thread `FrameStream` so downstream systems (DiaObjective, DiaEconomy, test automation) can react without coupling:

```cpp
struct EntitySpawnedEvent  { Entity entity; StringCRC blueprintId; StringCRC tag; };
struct EntityDespawnedEvent { Entity entity; DespawnReason reason; };  // reason: Lifetime/Radius/Cap/Explicit
```

### Module Integration

`EntitySpawnerModule` is an `IModule` running on SimPU:

- `Init()` — caches `IBlueprintLoader` ref, registers metrics.
- `Update(dt)` — ticks all active `SpawnEmitterComponent` instances; evaluates despawn conditions.
- `Shutdown()` — despawns all tracked children, clears tables.

---

## Public Interface

| Symbol | Kind | Description |
|--------|------|-------------|
| `SpawnRequest` | struct | Blueprint ID + position + optional parent + optional tag |
| `SpawnResult` | struct | Resulting `Entity` handle + `SpawnError` code |
| `SpawnError` | enum | `None`, `BlueprintNotFound`, `DomainFull` |
| `IEntitySpawner` | interface | `Spawn()` + `Despawn()` |
| `SpawnEmitterComponent` | component | Rate/burst/cap/lifetime/radius policy; `active` toggle |
| `EntitySpawnedEvent` | event | Fired on sim stream after each successful spawn |
| `EntityDespawnedEvent` | event | Fired on sim stream on each despawn with reason |
| `EntitySpawnerModule` | module | `IModule` entry point; exposes `IEntitySpawner` service |
| `DespawnReason` | enum | `Lifetime`, `Radius`, `Cap`, `Explicit` |

---

## Dependencies

| System | Role |
|--------|------|
| **diaentitytemplate** | `Domain::CreateEntity`, `Domain::DestroyEntity`, `QueueAddComponent`, `EndOfFrame`, `IBlueprintLoader` |
| **DiaReflect** | Component field deserialization (blueprint JSON → `SpawnEmitterComponent` fields) |
| **DiaSerializer** | `JsonMetadataHelpers` used by blueprint loader |
| **DiaApplicationFlow** | `EntitySpawnerModule` is an `IModule` registered on SimPU |

**Indirect (not dependencies — activated automatically once spawned entity's components are in place):**
DiaBlackboard, DiaEntitySpatial, DiaRigidBody2D, DiaOrder, DiaSteering — spawned entities that carry those components participate automatically.

**Relationship with DiaScene2D/3D:** No dependency. Scene loaders hydrate `SpawnEmitterComponent` like any other component. DiaEntitySpawner activates and drives it at runtime.

---

## Observability

| Pillar | Coverage |
|--------|----------|
| **Logs** | `DIA_LOG_INFO` on each spawn (blueprint, position, entity ID) and despawn (entity ID, reason); `DIA_LOG_ERROR` on `BlueprintNotFound` / `DomainFull` |
| **Traces** | `DIA_TRACE_ZONE` on `Update(dt)` inner loop and on `Spawn()` critical path |
| **Profiling** | `DIA_PROFILE_SCOPE` on `EntitySpawnerModule::Update` |
| **Metrics** | `spawner.active_count` (gauge — live spawned-entity count), `spawner.spawn_rate` (counter — spawns/frame), `spawner.despawn_reason.*` (per-reason counters) |
| **Health** | `EntitySpawnerModule` reports `Degraded` if `IBlueprintLoader` unavailable at `Update` time |

---

## Testing

### GoogleTest Suite

Exhaustive unit coverage in `GoogleTests/DiaEntity/TestEntitySpawner.cpp`:

| Area | Cases |
|------|-------|
| SpawnRequest API | Valid spawn returns live entity; `BlueprintNotFound` returns invalid entity + correct error; `DomainFull` handled gracefully |
| SpawnEmitterComponent — rate | Fractional token accumulation; correct count over N ticks; rate=0 produces no automatic spawns |
| SpawnEmitterComponent — burst | `burstCount` entities emitted on `active=true`; no additional spawns without rate |
| SpawnEmitterComponent — cap | Cap=1 despawns oldest before new spawn; cap=0 is unlimited |
| Despawn — lifetime | Child auto-despawned at correct tick |
| Despawn — radius | Child auto-despawned when distance exceeds threshold |
| Despawn — explicit | `Despawn(handle)` removes entity and fires `EntityDespawnedEvent` with `Explicit` reason |
| Events | `EntitySpawnedEvent` fired for every successful spawn; `EntityDespawnedEvent` fired for every despawn with correct `DespawnReason` |
| Module lifecycle | `Shutdown()` despawns all tracked children |
| Tag | Spawned entity queryable by tag via `Domain::Query` |
| Parent | Spawned entity with parent is destroyed when parent is destroyed (hierarchy feature) |

### CluicheTest E2E Stage

`SpawnerStage` — a visually interesting integration stage:

- Scene has 4 `SpawnEmitterComponent` entities at the corners of the screen, each configured with a different policy (rate, burst, cap, lifetime).
- Spawned entities use a coloured sprite blueprint and move outward via a simple `VelocityComponent` — creating expanding rings of entities that pop in and out.
- DiaVisualDebugger overlay shows live entity count and despawn events.
- Automated checkpoints (via DiaAutomation):
  1. At t=1s: at least 1 entity spawned from each emitter.
  2. At t=3s: cap-limited emitter never exceeds its cap.
  3. At t=5s: lifetime emitter has recycled at least once (spawn then despawn observed).
  4. At t=8s: explicit-despawn call clears all children.

---

## Inherited Binding Decisions

| ID | Decision | Applies Because |
|----|----------|-----------------|
| PD-001 | Use `StringCRC` for all entity/component IDs | `blueprintId` and `tag` on `SpawnRequest` use `StringCRC` |
| PD-002 | ProcessingUnit/Phase/Module architecture | `EntitySpawnerModule` is an `IModule` on SimPU |
| PD-004 | No STL containers in public APIs | `SpawnRequest`, `SpawnResult`, and all public structs use engine types |
| PD-005 | x64 only | Build target |
| PD-007 | C++20 required | All source files compile under `/std:c++20` |
| PD-008 | `Directory.Build.props` owns OutDir/IntDir | No per-project build overrides |
| AD-001 | Module documented with YAML frontmatter | `EntitySpawnerModule` needs `dia.entity.entityspawner.architecture.module.md` |
| AD-002 | No STL in public APIs | Enforced on all public structs |
| AD-003 | Namespace: `Dia::Entity::` | All public symbols live in `Dia::Entity::` |
| AD-004 | ProcessingUnit/Phase/Module | `EntitySpawnerModule` registered on SimPU |

---

## Design Decisions

| # | Decision | Rationale |
|---|----------|-----------|
| 1 | `SpawnRequest::position` is world-space only | Caller already has the emitter's world position; emitter-relative offset is a one-liner at the call site. Keeps the API stateless. Defer offset support to a child feature if a real use case emerges. |
| 2 | `IBlueprintLoader` resolved lazily on first `Spawn()`, then cached | `Init()`-time resolution is fragile if the blueprint loader initialises after the spawner. Full lazy-per-call is noisy. Lazy-with-cache gives hot-reload safety (invalidate on module reload) with no per-call overhead. Same pattern as DiaEntitySpatial's module ref. |
| 3 | External child destruction detected via `EntityDestroyedMessage` subscription | `EntitySpawnerModule` subscribes to `EntityDestroyedMessage` (published by the hierarchy system). Keeps cap counters accurate and prevents stale handles when game logic, physics, or scene unload destroys a tracked child. |
