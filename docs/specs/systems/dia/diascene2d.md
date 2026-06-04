# System Spec: DiaScene2D

## Parent Application
@docs/specs/applications/dia.md

**Status:** `Approved`

**Research:** @docs/research/diascene2d/design-decisions.md

---

## Purpose

DiaScene2D is the engine library for 2D scene management. It owns the `.diascene` file format (reflected struct), the layer table, and the SceneLoader that populates camera/light registries and spawns entities from scene files.

A scene is **purely spatial content** — what's placed where. It contains no gameplay config (gravity, clear_colour), no name (identity from catalogue/stage), and no rendering policy (render techniques live in stage config). The scene file is a reflected struct serialized via `JsonArchive` — no custom serializer.

### Key design principles

- **Everything is an entity** — weight determined by blueprint, not scene format
- **Cameras/lights are scene-owned** — not entities; loaded into registries via proxy pattern (blueprint + instance_data)
- **Programmatic path is always available** — DiaCamera2D/DiaLighting2D registries work without any scene file
- **No DiaScene base** — DiaScene2D and future DiaScene3D are independent peers

```
.diascene file (reflected struct)
    ↓  SceneLoader2D reads
DiaCamera2D::CameraRegistry2D  ← cameras hydrated
DiaLighting2D::LightRegistry2D ← lights hydrated
diaentitytemplate::Domain              ← entities spawned from blueprints + instance_data
LayerTable                     ← layer definitions resolved
```

**Dependency chain:**
`DiaScene2D → DiaCamera2D, DiaLighting2D, diaentitytemplate, DiaReflect, DiaGeometry2D, DiaMaths, DiaCore`

---

## Responsibilities

- Own `Scene2D` reflected struct (world_bounds, layers, cameras, lights, entities)
- Own `LayerTable` — layer definitions, sort order, parallax, bitmask resolution (name → bit index)
- Own `SceneLoader2D` — reads `.diascene`, populates camera registry, light registry, spawns entities into Domain
- Define layer schema (id, sort_order, parallax Vec2, sort_policy, enabled, optional render_technique reference)
- Resolve `affects_layers` names to uint32 bitmask at load time
- Provide a `"default"` layer fallback so entities always render somewhere
- Validate scene constraints (exactly one active camera)
- Provide `DiaScene2D.vcxproj` static library project registered in `Cluiche.sln`
- Provide `dia.scene2d.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Camera runtime behaviour (DiaCamera2D)
- Light runtime management (DiaLighting2D)
- Entity component systems / ECS (diaentitytemplate)
- Blueprint definition or component reflection (diaentitytemplate, DiaReflect)
- Rendering / draw calls (DiaBgfx)
- Gameplay config: gravity, clear_colour, render techniques (`.diastage` config)
- Scene identity / display name (catalogue asset ID / `.diastage` name)
- 3D scenes (future DiaScene3D — independent peer)
- PU/Module integration (application-side)
- Sub-scene nesting / scene composition

---

## Public Interfaces

### Scene2D (reflected struct)

```cpp
namespace Dia::Scene2D
{
    struct LayerDef
    {
        Dia::Core::StringCRC id;
        int sortOrder = 0;
        Dia::Maths::Vector2D parallax = {1.0f, 1.0f};
        Dia::Core::StringCRC sortPolicy;  // v1: "insertion" only
        bool enabled = true;
        Dia::Core::StringCRC renderTechnique;  // optional, StringCRC::kEmpty if unset
    };

    struct CameraEntry
    {
        Dia::Core::StringCRC id;
        bool active = false;
        Dia::Core::StringCRC blueprint;
        // instance_data applied via diaentitytemplate reflection (Component.Field → value)
    };

    struct LightEntry
    {
        Dia::Core::StringCRC id;
        bool enabled = true;
        Dia::Core::StringCRC blueprint;
        Dia::Core::DynamicArrayC<Dia::Core::StringCRC, 32> affectsLayers;
        // instance_data applied via diaentitytemplate reflection
    };

    struct EntityInstance
    {
        Dia::Core::StringCRC id;
        Dia::Core::StringCRC name;  // optional (kEmpty if unset)
        Dia::Core::StringCRC blueprint;
        bool enabled = true;
        // instance_data applied via diaentitytemplate reflection
    };

    struct Scene2D
    {
        Dia::Geometry2D::AARect worldBounds;  // optional (zero-area = unbounded)
        Dia::Core::DynamicArrayC<LayerDef, 32> layers;
        Dia::Core::DynamicArrayC<CameraEntry, 4> cameras;
        Dia::Core::DynamicArrayC<LightEntry, 16> lights;
        Dia::Core::DynamicArrayC<EntityInstance, 256> entities;
    };
}
```

### LayerTable

```cpp
namespace Dia::Scene2D
{
    class LayerTable
    {
    public:
        void Build(const Dia::Core::DynamicArrayC<LayerDef, 32>& layers);

        unsigned int GetBitIndex(Dia::Core::StringCRC layerId) const;
        uint32_t ResolveMask(const Dia::Core::DynamicArrayC<Dia::Core::StringCRC, 32>& layerNames) const;

        unsigned int GetCount() const;
        const LayerDef& GetByIndex(unsigned int index) const;
        const LayerDef& GetById(Dia::Core::StringCRC id) const;

        bool Has(Dia::Core::StringCRC id) const;

        // Default layer — always exists (bit 0 if not explicitly defined)
        static constexpr Dia::Core::StringCRC kDefaultLayerId = "default"_crc;
    };
}
```

### SceneLoader2D

```cpp
namespace Dia::Scene2D
{
    struct SceneLoadContext
    {
        Dia::Camera2D::CameraRegistry2D& cameraRegistry;
        Dia::Lighting2D::LightRegistry2D& lightRegistry;
        Dia::Entity::Domain& entityDomain;
        // Blueprint resolution (catalogue lookup)
        // instance_data field patching
    };

    class SceneLoader2D
    {
    public:
        // Load .diascene file, populate all target systems
        bool Load(const char* filePath, SceneLoadContext& context, LayerTable& outLayers);

        // Unload — unregister cameras/lights, destroy entities
        void Unload(SceneLoadContext& context);
    };
}
```

### Scene file format (`.diascene`)

```json
{
  "scene2d": {
    "world_bounds": { "min": [0, 0], "max": [1920, 1080] },
    "layers": [
      { "id": "background", "sort_order": -10, "parallax": [0.5, 0.0], "sort_policy": "insertion", "enabled": true },
      { "id": "midground", "sort_order": 0, "parallax": [1.0, 1.0], "sort_policy": "insertion", "enabled": true },
      { "id": "foreground", "sort_order": 10, "parallax": [1.0, 1.0], "sort_policy": "insertion", "enabled": true }
    ],
    "cameras": [
      { "id": "gameplay", "active": true, "blueprint": "camera_2d_follow", "instance_data": { "Camera2D.position": [400, 300], "FollowBehaviour.offset": [0, -50] } }
    ],
    "lights": [
      { "id": "campfire", "blueprint": "point_light_warm", "enabled": true, "instance_data": { "PointLight2D.position": [300, 200] }, "affects_layers": ["midground", "foreground"] }
    ],
    "entities": [
      { "id": "player_spawn", "blueprint": "player", "instance_data": { "Transform2D.position": [400, 300] } },
      { "id": "torch_01", "name": "left_torch", "blueprint": "torch", "enabled": false, "instance_data": { "Transform2D.position": [200, 100], "Renderable2D.layer": "foreground" } }
    ]
  }
}
```

**File format rules:**
- Extension: `.diascene`
- Top-level key `"scene2d"` discriminates type (future: `"scene3d"`)
- Serialized via `JsonArchive` (reflected struct, no custom serializer)
- `world_bounds` optional — zero-area AARect means unbounded
- Exactly one camera must have `active: true` (validated at load)
- `instance_data` uses diaentitytemplate reflection keys: `Component.Field: value`
- `render_technique` on layers is optional (no-op until render technique assets exist)

---

## Dependencies

| Dependency | What's used |
|---|---|
| DiaCamera2D | CameraRegistry2D, Camera2D, CameraBehaviourRegistry (factory for loading behaviours) |
| DiaLighting2D | LightRegistry2D, PointLight2D |
| diaentitytemplate | Domain (entity spawning), blueprint resolution, instance_data field patching |
| DiaReflect | JsonArchive (Scene2D struct serialization/deserialization) |
| DiaGeometry2D | AARect (world_bounds) |
| DiaMaths | Vector2D |
| DiaCore | StringCRC, containers, serialization primitives |

### Does NOT depend on

- DiaGraphics (no FrameData, no rendering concepts)
- DiaBgfx (no renderer code)
- DiaApplicationFlow (no PU/Module coupling)
- DiaObservation (no logging — caller logs)

---

## Features

| Feature | Description | Spec |
|---|---|---|
| scene2d-format | Scene2D reflected struct, LayerTable, `.diascene` file schema, serialization roundtrip | TBD |
| scene2d-loader | SceneLoader2D — hydrate camera/light registries, spawn entities, resolve instance_data and layer masks | TBD |

---

## Inherited Binding Decisions

| ID | Decision | How it applies |
|---|---|---|
| PD-001 | StringCRC for all IDs | Layer IDs, camera IDs, light IDs, entity IDs, blueprint references |
| PD-004 | No STL containers in public APIs | All arrays are DynamicArrayC, maps are HashTable |
| PD-005 | x64 only | Single build target |
| PD-007 | C++20 required | Module uses C++20 features |
| PD-008 | Directory.Build.props owns build paths | Library outputs to `bin/sharedlibs/<Config>/<Platform>/` |
| PD-010 | `.diagame` is project root; `.diastage` declares stage metadata | Scene referenced from `.diastage`; never directly from `.diagame` |
| AD-001 | Module system with YAML frontmatter | `dia.scene2d.architecture.module.md` required |
| AD-002 | No STL in public APIs | Same as PD-004 |
| AD-003 | Namespace convention `Dia::<Module>::` | `Dia::Scene2D::` namespace |

---

## Resolved Design Questions

1. **`.diascene` not `.diascene2d`** — Single extension, top-level key discriminates 2D vs 3D. Less format proliferation.

2. **No sub-scene references** — Composition via entity blueprints. Render-to-texture for 2D-in-3D (entity with SceneViewport2D component).

3. **Everything is an entity** — Weight determined by blueprint. Terrain = tile blueprint entities. Props = prop blueprint entities. No "static renderable" second class.

4. **Cameras/lights scene-owned** — Not entities. Blueprint/instance_data format for reflection-friendly serialization, hydrated into registries (not ECS).

5. **No gameplay config in scene** — Gravity, clear_colour, render techniques all in `.diastage` config. Scene is purely spatial.

6. **`instance_data` not `overrides`** — Neutral term, aligned with diaentitytemplate reflection. Position is `Transform2D.position`, not special-cased.

7. **Layer render technique as reference** — Optional StringCRC pointing to a future render technique asset. No-op until that asset system exists (backlogged).

8. **Parallax is Vec2** — Independent X/Y for side-scrollers, top-down, and vertical shooters.

9. **Default layer** — Implicit. If no layers defined, a `"default"` layer exists at bit 0, sort_order 0, parallax [1,1]. If layers are defined but entity doesn't specify one, it lands in `"default"` (which must be in the layers array or is auto-injected).

10. **World bounds optional** — Zero-area AARect signals unbounded. Runtime warns if BoundsClampBehaviour attached and no bounds exist.

11. **Active camera validation** — Exactly one camera must be `active: true`. Loader asserts on violation. Pipeline can pre-validate.

---

## Status

`Done`

**Plan:** [diascene2d.plan.md](diascene2d.plan.md)
