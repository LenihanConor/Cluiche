# Dia Asset & Entity Lifecycle

## Entity Lifecycle Overview

An entity goes through these phases from definition to running in-game:

```
Template (YAML/JSON) → Component Registration → Pool Setup → Instantiation → Mutation Apply → Running
```

## Phase 1: Component Registration (Static Init)

At application startup, each component type self-registers via the `DIA_COMPONENT_REGISTER` macro:

```cpp
// In ParentComponent.cpp
DIA_COMPONENT_REGISTER(ParentComponent, "dia.hierarchy.parent", false, false,
    s_ParentComponent_fields, DIA_ARRAY_COUNT(s_ParentComponent_fields),
    nullptr, 0, nullptr, 0)
```

This registers a `ComponentTypeDesc` in the global `ComponentRegistry`:
- Type CRC (hashed from string ID like `"dia.hierarchy.parent"`)
- Field metadata (names, types, offsets)
- Serialization thunks (JSON load/save)
- Requirements (other components this one depends on)

## Phase 2: Component Pool Setup (Module Init)

When a module initializes (via ProcessingUnit startup):

1. Module creates `ComponentPool<TComponent>` for each component type it owns
2. Calls `Domain::RegisterPool(IComponentPool*)` to register with the ECS container
3. Pools are indexed by component type CRC
4. Memory managed via `HandlePool` (slot index + generation for safe reuse)

## Phase 3: Blueprint/Scene Loading

Entity templates are defined in JSON blueprints:

```json
{
  "version": 1,
  "entities": {
    "player": {
      "transform": { "x": 0.0, "y": 0.0 },
      "sprite": { "assetId": "player.png" }
    }
  }
}
```

The `JsonBlueprintLoader` processes these:

1. For each entity in the blueprint:
   - `Domain::CreateEntity(name)` — allocates entity slot, returns `Entity(index, generation)`
2. For each component on the entity:
   - Looks up `ComponentTypeDesc` via `ComponentRegistry::Find(typeId)`
   - Collects into `PendingComponent` array with JSON config
   - **Topologically sorts by REQUIRES** (dependency order)
3. For each component in sorted order:
   - `Domain::QueueAddComponentByTypeId(entity, typeId, config)` — queued, NOT applied yet

## Phase 4: Mutation Application (EndOfFrame)

`Domain::EndOfFrame()` applies all queued mutations in 3 passes:

**Pass 1 — Add Components:**
1. Lookup `ComponentPool` for typeId
2. Validate REQUIRES (all required components must already exist)
3. Allocate slot: `pool->Allocate(entity.index)` (default-constructs component)
4. Deserialize: `desc->loadFromJson(component, config)` via DIA_SERIALIZE thunks
5. Call `component->OnAttach(domain, entity)` lifecycle hook
6. Validate single-writer rule (DIA_WRITES)
7. Invalidate query caches containing this type

**Pass 2 — Remove Components:**
1. Call `component->OnDetach()` hook
2. Return slot to pool

**Pass 3 — Destroy Entities:**
1. Broadcast `EntityDestroyedMessage` (subscribers can inspect state)
2. Detach all components (OnDetach on each)
3. Return entity slot to pool
4. Invalidate all query caches

**Post-mutations:** Rebuild all dirty query caches, update metrics.

## Phase 5: Running (Per-Frame)

```
Each frame:
  1. domain.Update(dt)
     → walks entities, calls DoUpdate() on components with kFlagOverridesDoUpdate
     → used for per-frame logic (physics step, animation tick, etc.)

  2. Game code queues mutations during frame
     → QueueAddComponent, QueueRemoveComponent, QueueDestroyEntity

  3. domain.EndOfFrame()
     → applies queued mutations (same 3-pass as Phase 4)
     → new components visible in next frame's queries
```

## Phase 6: Querying Entities

Game systems query for entities by component signature:

```cpp
auto view = domain.Query<TransformComponent, SpriteComponent>();
for (auto [entity, comps] : view) {
    auto [transform, sprite] = comps;
    // render sprite at transform position
}
```

- Query signature computed from sorted component type CRCs
- Results cached; cache rebuilt only when relevant components mutated
- O(1) cache hit after rebuild

## Key Design Patterns

| Pattern | Purpose |
|---------|---------|
| **Queued mutations** | Changes deferred until EndOfFrame; deterministic, no iterator invalidation mid-frame |
| **Dependency resolution** | REQUIRES declarations; blueprint loader topologically sorts attach order |
| **Type-erased pools** | Each component type gets its own pool; Domain holds them in flat array keyed by CRC |
| **Handle-based entities** | Entity = index + generation; enables safe slot reuse without stale references |
| **Query caching** | Per-signature cache; rebuilt only when relevant types mutated |
| **Lifecycle hooks** | OnAttach / OnDetach / DoUpdate let components react to structural changes and frame ticks |

## Component Access Patterns

- **DIA_READONLY** — data-driven: module owns data, provides read-only access to entity
- **Behaviour** — entity signals module via mailbox (immutable messages)
- No service locators or statics for component access (use ModuleRef or streams)

## Scene Loading Flow

Scenes (`.diascene`) are loaded by `SceneLoader2D`:

```
.diagame manifest → stage reference → .diastage → scene_path → .diascene → entities
```

1. Game manifest lists stages
2. Stage manifest references a scene file
3. Scene file contains entity placements (template + position + component overrides)
4. SceneLoader2D instantiates entities via Domain using blueprint loader patterns

## Asset Pipeline

Assets go through the DiaCLI pipeline:

```
Source file → Validate → Transform → Deploy → Runtime load
```

| Stage | What happens |
|-------|-------------|
| **Validate** | Schema check, required fields, file existence |
| **Transform** | Convert to runtime format (e.g., texture compression) |
| **Deploy** | Copy to `bin/<Target>/<Config>/x64/assets/` |
| **Runtime load** | `AssetHandler` registered per type loads asset into memory |

Built-in asset handler types: texture, sprite, audio, config, entity, stage, ui, folder.

## Entity Template Files

Templates live in `Assets/<App>/GameData/EntityTemplates/` as YAML:

```yaml
# player.yaml
template_id: "Player"
components:
  - type: "physics.rigid_body"
    mass: 1.0
    gravity_scale: 1.0
  - type: "render.sprite"
    asset: "player.png"
    material: "transparent"
    z_order: 10
  - type: "animation.animator"
    animation: "player_idle"
    loop: true
```

In the editor, templates are managed via the **Entity Template Editor** plugin (`entity_template_editor.*` actions).
