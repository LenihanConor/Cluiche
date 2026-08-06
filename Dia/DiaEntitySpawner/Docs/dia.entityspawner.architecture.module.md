---
module_id: dia.entityspawner
display_name: DiaEntitySpawner
parent_module: dia.entity
layer: domain-adapter
version: "1.0"
status: Active
dependencies:
  - dia.core
  - dia.maths
  - dia.entity
  - dia.entityspatial
  - dia.applicationflow
  - dia.observation
public_api:
  headers:
    - DiaEntitySpawner/SpawnerTypes.h
    - DiaEntitySpawner/SpawnEmitterComponent.h
    - DiaEntitySpawner/EntitySpawnerModule.h
    - DiaEntitySpawner/EntitySpawnerImpl.h
  namespaces:
    - Dia::Entity::
    - Dia::EntitySpawner::
  entry_points:
    - EntitySpawnerModule
    - IEntitySpawner
    - SpawnEmitterComponent
responsibilities:
  - Runtime entity spawning from blueprint IDs via IBlueprintLoader
  - SpawnEmitterComponent rate/burst/cap/lifetime/radius despawn policies
  - Tracks spawned children per emitter; enforces cap via FIFO overflow
  - Publishes EntitySpawnedEvent and EntityDespawnedEvent on sim-thread streams
  - Handles external entity destruction via EntityDestroyedMessage subscription
  - Full DiaObservation coverage: logs, traces, metrics, health reporter
not_responsibilities:
  - Blueprint authoring (DiaEntityBlueprintEditor)
  - Scene placement (DiaScene2D/3D)
  - Object pooling (future child feature)
  - Visual debugging overlays (DiaEntitySpawnerVisualDebugger)
---
# DiaEntitySpawner

Runtime entity creation and lifecycle management. Provides `IEntitySpawner` for programmatic spawning and `SpawnEmitterComponent` for data-driven rate/burst/cap/lifetime/radius policies.

`EntitySpawnerModule` runs on SimPU and drives all active `SpawnEmitterComponent` instances each tick. Spawned entities are tracked internally; cap enforcement uses FIFO oldest-child eviction. Lifetime and radius despawn conditions are evaluated every update. External domain destroys are detected via `EntityDestroyedMessage` subscription to keep cap counters accurate.
