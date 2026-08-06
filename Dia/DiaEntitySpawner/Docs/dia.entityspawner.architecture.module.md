---
module_id: dia.entityspawner
display_name: DiaEntitySpawner
parent_module: dia.entity
layer: domain-adapter
version: "1.0"
status: In Progress
dependencies:
  - dia.core
  - dia.entity
  - dia.streams
  - dia.applicationflow
  - dia.observation
  - dia.maths
public_api:
  headers:
    - DiaEntitySpawner/SpawnerTypes.h
    - DiaEntitySpawner/SpawnEmitterComponent.h
    - DiaEntitySpawner/EntitySpawnerModule.h
  namespaces:
    - Dia::EntitySpawner
  entry_points:
    - EntitySpawnerModule
responsibilities:
  - Runtime entity spawning from blueprint IDs
  - SpawnEmitterComponent rate/burst/cap/lifetime/radius policies
  - Publishes EntitySpawnedEvent and EntityDespawnedEvent on sim stream
not_responsibilities:
  - Blueprint authoring (DiaEntityBlueprintEditor)
  - Scene placement (DiaScene2D/3D)
  - Object pooling (future child feature)
  - Visual debugging overlays (DiaEntitySpawnerVisualDebugger)
---
# DiaEntitySpawner

Runtime entity creation and lifecycle management. Provides `IEntitySpawner` for programmatic spawning and `SpawnEmitterComponent` for data-driven rate/burst/cap/lifetime/radius policies.
