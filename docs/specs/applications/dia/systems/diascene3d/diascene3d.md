# System Spec: DiaScene3D

## Parent Application
@docs/specs/applications/dia/dia.md

**Status:** `Draft`

---

## Purpose

DiaScene3D is the engine library for 3D scene management. It owns the `.diascene` file format for 3D scenes (reflected struct, `"scene3d"` top-level key), the scene graph (`SceneGraph3D` — flat list with parent-index links), `SceneLoader3D` (reads the file, populates `CameraRegistry3D`, `LightRegistry3D`, spawns entities, builds the graph), and `Submit()` (resolves world transforms, frustum-culls static nodes, emits draw commands into `Mesh3DFrameData`, exposes frustum for entity self-culling).

DiaScene3D covers **placed scene content** — static geometry, lights, cameras, and props loaded from a file. Dynamic entities (player, enemies, projectiles) own their transforms via components and are not scene graph nodes. An entity may optionally anchor to a scene node to inherit its world transform (e.g. an effect mounted to a static pillar), but entities are not required to participate in the scene graph at all.

DiaScene3D is an **independent peer of DiaScene2D** — no shared base, no shared file format beyond the `.diascene` extension and top-level key discriminator.

```
.diascene file ("scene3d" key)
    ↓  SceneLoader3D reads
DiaCamera3D::CameraRegistry3D   ← cameras hydrated
DiaLighting3D::LightRegistry3D  ← lights hydrated
diaentitytemplate::Domain       ← entities spawned from blueprints + instance_data
SceneGraph3D                    ← node hierarchy built (static content)
    ↓  Submit(context)
SceneSubmitContext3D::frustum   ← derived from active camera (readable by entity components)
Dia::Graphics3D::Mesh3DFrameData ← static draw commands emitted
```

**Dependency chain:**
`DiaScene3D → DiaCamera3D, DiaLighting3D, diaentitytemplate, DiaGraphics3D, DiaGeometry3D, DiaMaths, DiaCore`

---

## Responsibilities

- Own `Scene3D` reflected struct (worldBounds, nodes, cameras, lights, entities)
- Own `SceneNode3D` — id, name, local transform, visible flag, type tag, ref ID, parent index
- Own `SceneGraph3D` — flat list with parent-index links, world transform resolution, node CRUD
- Own `SceneLoader3D` — reads `.diascene`, populates camera registry, light registry, spawns entities, builds scene graph
- Own `Submit()` — derives frustum from active camera via `ViewportTransform3D`, frustum-culls static nodes, resolves world transforms, emits `Mesh3DDrawCommand` per visible static node into `Mesh3DFrameData`, exposes `SceneSubmitContext3D` (frustum + frameData ref) for entity self-culling
- Validate scene constraints (exactly one active camera)
- Provide a `"root"` group node if no nodes defined, so the graph always exists
- Provide `DiaScene3D.vcxproj` static library project registered in `Cluiche.sln`
- Provide `dia.scene3d.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Camera runtime behaviour (DiaCamera3D)
- Light runtime management (DiaLighting3D)
- Entity component systems / ECS (diaentitytemplate)
- Dynamic entity transform management — entities own their transforms via components
- Depth sorting — render side (DiaBgfx3D uses bgfx view sort modes)
- Entity frustum culling — each entity's `Renderable3DComponent` self-culls using the frustum from `SceneSubmitContext3D`
- Rendering / draw calls (DiaBgfx3D)
- Gameplay config (gravity, clear_colour) — `.diastage` config
- Scene identity / display name — catalogue asset ID / `.diastage` name
- 2D scenes (DiaScene2D — independent peer)
- Sub-scene nesting / scene composition
- PU/Module integration (application-side)
- Skinning palette resolution — DiaSkinning3D writes `skinningPaletteIndex` before submission

---

## Public Interfaces

### `SceneNode3D`

```cpp
namespace Dia::Scene3D
{
    enum class SceneNodeType : uint8_t { Group, Entity, Camera, Light };

    struct SceneNode3D
    {
        Dia::Core::StringCRC     id;
        Dia::Core::StringCRC     name;           // optional (kEmpty if unset)
        Dia::Maths::Transform3D  localTransform;
        bool                     visible   = true;  // propagates to children
        SceneNodeType            type      = SceneNodeType::Group;
        Dia::Core::StringCRC     refId;          // entity/camera/light ID (kEmpty for Group)
        int                      parentIndex = -1;  // -1 = root
    };
}
```

### `Scene3D` (reflected struct)

```cpp
namespace Dia::Scene3D
{
    struct CameraNode
    {
        Dia::Core::StringCRC id;
        bool                 active = false;
        Dia::Core::StringCRC blueprint;
        Dia::Maths::Transform3D localTransform;
        Json::Value          instanceData;  // opaque — captured verbatim
    };

    struct LightNode
    {
        Dia::Core::StringCRC id;
        bool                 enabled = true;
        Dia::Core::StringCRC blueprint;
        Dia::Maths::Transform3D localTransform;
        Json::Value          instanceData;  // opaque — captured verbatim
    };

    struct EntityInstance
    {
        Dia::Core::StringCRC id;
        Dia::Core::StringCRC name;        // optional (kEmpty if unset)
        Dia::Core::StringCRC blueprint;
        bool                 enabled = true;
        Dia::Maths::Transform3D localTransform;
        Dia::Core::StringCRC parentNodeId;  // optional scene node anchor (kEmpty = no anchor)
        Json::Value          instanceData;  // opaque — captured verbatim
    };

    struct StaticMeshEntry
    {
        Dia::Core::StringCRC id;
        Dia::Core::StringCRC name;        // optional
        Dia::Core::StringCRC meshAssetId;
        Dia::Core::StringCRC materialId;
        Dia::Maths::Transform3D localTransform;
        Dia::Core::StringCRC parentNodeId;  // optional
        bool                 visible = true;
    };

    struct Scene3D
    {
        Dia::Geometry3D::AABB                                              worldBounds;   // zero-volume = unbounded
        Dia::Core::Containers::DynamicArrayC<CameraNode,      4>          cameras;
        Dia::Core::Containers::DynamicArrayC<LightNode,       32>         lights;
        Dia::Core::Containers::DynamicArrayC<EntityInstance,  256>        entities;
        Dia::Core::Containers::DynamicArrayC<StaticMeshEntry, 1024>       staticMeshes;
    };
}
```

### `SceneGraph3D`

```cpp
namespace Dia::Scene3D
{
    class SceneGraph3D
    {
    public:
        static constexpr unsigned int kMaxNodes = 2048;

        // Build from a loaded Scene3D (called by SceneLoader3D)
        void Build(const Scene3D& scene);
        void Clear();

        // Node access
        bool               Has         (Dia::Core::StringCRC id) const;
        const SceneNode3D& Get         (Dia::Core::StringCRC id) const;
        SceneNode3D&       Get         (Dia::Core::StringCRC id);
        unsigned int       GetCount    ()                         const;
        const SceneNode3D& GetByIndex  (unsigned int index)       const;

        // World transform resolution (traverses parent chain)
        Dia::Maths::Matrix44 ResolveWorldTransform(Dia::Core::StringCRC id) const;
    };
}
```

### `SceneSubmitContext3D`

```cpp
namespace Dia::Scene3D
{
    struct SceneSubmitContext3D
    {
        Dia::Geometry3D::Frustum          frustum;    // derived from active camera; readable by entity components
        Dia::Graphics3D::Mesh3DFrameData& frameData;
    };
}
```

### `SceneLoader3D`

```cpp
namespace Dia::Scene3D
{
    struct SceneLoadContext3D
    {
        Dia::Camera3D::CameraRegistry3D&    cameraRegistry;
        Dia::Lighting3D::LightRegistry3D&   lightRegistry;
        Dia::Entity::Domain&                entityDomain;
    };

    struct SceneLoadErrors3D
    {
        bool hasErrors = false;
    };

    class SceneLoader3D : public Dia::Observation::Health::HealthReporterBase
    {
    public:
        Dia::Core::StringCRC GetReporterName() const override;

        // Load .diascene file (expects "scene3d" top-level key).
        // Populates registries, spawns entities, builds scene graph.
        // Returns false on hard errors (parse failure, wrong key, != 1 active camera).
        bool Load(const char*          filePath,
                  SceneLoadContext3D&  context,
                  SceneGraph3D&        outGraph,
                  SceneLoadErrors3D*   outErrors = nullptr);

        // Unload — unregisters cameras/lights, destroys spawned entities, clears graph.
        void Unload(SceneLoadContext3D& context, SceneGraph3D& graph);

        // Per-frame: resolve transforms, frustum-cull, emit draw commands.
        // Populates ctx.frustum from active camera before emitting.
        void Submit(const Dia::Camera3D::CameraRegistry3D& cameras,
                    const SceneGraph3D&                     graph,
                    Dia::Maths::Vector2D                    windowSize,
                    SceneSubmitContext3D&                    ctx);
    };
}
```

### Scene file format (`.diascene` — `"scene3d"` key)

```json
{
  "scene3d": {
    "world_bounds": { "min": [-500, -500, -500], "max": [500, 500, 500] },
    "cameras": [
      {
        "id": "main",
        "active": true,
        "blueprint": "camera_3d_follow",
        "local_transform": { "position": [0, 5, 10], "orientation": [0, 0, 0, 1], "scale": [1, 1, 1] },
        "instance_data": { "Follow3D.targetId": "player" }
      }
    ],
    "lights": [
      {
        "id": "sun",
        "blueprint": "directional_light",
        "enabled": true,
        "local_transform": { "position": [0, 0, 0], "orientation": [0.7, 0, 0, 0.7], "scale": [1, 1, 1] },
        "instance_data": { "DirectionalLight3D.intensity": 1.2 }
      }
    ],
    "entities": [
      {
        "id": "player_spawn",
        "blueprint": "player",
        "local_transform": { "position": [0, 0, 0], "orientation": [0, 0, 0, 1], "scale": [1, 1, 1] },
        "instance_data": { "Health.max": 100 }
      },
      {
        "id": "torch_01",
        "blueprint": "torch",
        "enabled": true,
        "parent_node_id": "wall_section_a",
        "local_transform": { "position": [1.2, 2.0, 0], "orientation": [0, 0, 0, 1], "scale": [1, 1, 1] },
        "instance_data": {}
      }
    ],
    "static_meshes": [
      {
        "id": "wall_section_a",
        "mesh_asset_id": "mesh_stone_wall",
        "material_id": "mat_stone",
        "local_transform": { "position": [0, 0, -10], "orientation": [0, 0, 0, 1], "scale": [1, 1, 1] }
      }
    ]
  }
}
```

**File format rules:**
- Extension: `.diascene` (shared with DiaScene2D)
- Top-level key `"scene3d"` discriminates from `"scene2d"`
- Serialized via `JsonArchive` (reflected struct, no custom serializer)
- `world_bounds` optional — zero-volume AABB means unbounded
- Exactly one camera must have `active: true` (validated at load)
- `instance_data` uses diaentitytemplate reflection keys: `Component.Field: value`
- `parent_node_id` on entities and static meshes is optional — omit for world-root placement
- `local_transform` is always relative to parent node (or world root if no parent)

---

## Dependencies

| Dependency | What's used |
|---|---|
| DiaCamera3D | CameraRegistry3D, ViewportTransform3D (frustum derivation) |
| DiaLighting3D | LightRegistry3D |
| diaentitytemplate | Domain (entity spawning), blueprint resolution, instance_data field patching |
| DiaGraphics3D | Mesh3DFrameData, Mesh3DDrawCommand |
| DiaGeometry3D | Frustum (culling), AABB (worldBounds, node bounds testing) |
| DiaMaths | Transform3D, Matrix44, Vector3D, Quaternion |
| DiaCore | StringCRC, DynamicArrayC |
| DiaObservation | HealthReporterBase on SceneLoader3D, metrics (nodes loaded, draw commands emitted, culled count) |

### Does NOT depend on

- DiaGraphics (2D FrameData — no 2D rendering concepts)
- DiaBgfx / DiaBgfx3D (no renderer code)
- DiaApplicationFlow (no PU/Module coupling)
- DiaScene2D (independent peer)

---

## Features

| Feature | Description | Spec |
|---|---|---|
| `scene3d-format` | Scene3D reflected struct, SceneNode3D, SceneGraph3D, `.diascene` file schema, serialization roundtrip | TBD |
| `scene3d-loader` | SceneLoader3D — hydrate camera/light registries, spawn entities, build scene graph, resolve instance_data | TBD |
| `scene3d-submit` | Submit() — frustum derivation, world transform resolution, frustum culling, draw command emission, SceneSubmitContext3D | TBD |

---

## Inherited Binding Decisions

| ID | Decision | How it applies |
|---|---|---|
| PD-001 | StringCRC for all IDs | Node IDs, camera IDs, light IDs, entity IDs, mesh/material asset IDs |
| PD-004 | No STL containers in public APIs | All arrays are DynamicArrayC |
| PD-005 | x64 only | Single build target |
| PD-007 | C++20 required | Module uses C++20 features |
| PD-008 | Directory.Build.props owns build paths | Library output to `bin/sharedlibs/<Config>/<Platform>/` |
| PD-010 | `.diagame` is project root; `.diastage` declares stage metadata | Scene referenced from `.diastage`; never directly from `.diagame` |
| AD-001 | Module system with YAML frontmatter | `dia.scene3d.architecture.module.md` required |
| AD-002 | No STL in public APIs | Same as PD-004 |
| AD-003 | Namespace convention `Dia::<Module>::` | `Dia::Scene3D::` namespace |
| G3D-005 | Draw commands carry pre-composed Matrix44 | SceneLoader3D resolves parent chain before emitting `Mesh3DDrawCommand::transform` |

---

## Resolved Design Questions

1. **Independent peer of DiaScene2D** — No shared base class. `.diascene` extension is shared; top-level key (`"scene2d"` vs `"scene3d"`) discriminates. Keeps both systems self-contained.

2. **Model B: scene graph covers placed scene content only** — The scene graph manages static scene objects (meshes, cameras, lights, anchored props). Dynamic entities (player, enemies) own their transforms via components and are not scene graph nodes. An entity can optionally anchor to a scene node via `parent_node_id` to inherit a world transform.

3. **Cameras and lights in the scene graph** — Camera nodes and light nodes are first-class entries in the scene file with `local_transform`. Their world transforms are resolved by the scene graph each frame and pushed into `CameraRegistry3D` / `LightRegistry3D` before `Submit()`.

4. **Flat list with parent-index links** — `SceneGraph3D` stores nodes in a flat `DynamicArrayC`. Each node stores a `parentIndex` (-1 = root). World transform resolution walks the parent chain upward with no pointer indirection. Cache-friendly, fixed capacity, no heap allocation.

5. **Depth sort is render-side** — `Submit()` emits draw commands in scene-graph order. DiaBgfx3D handles opaque/transparent sort modes via bgfx view configuration. DiaScene3D does not sort.

6. **Frustum culling split** — DiaScene3D frustum-culls its own static nodes during `Submit()`. Entity components self-cull by testing their bounding volume against `SceneSubmitContext3D::frustum`. DiaScene3D has no knowledge of entities; entities query the frustum from the context passed by the sim module.

7. **`instanceData` opaque `Json::Value`** — Same pattern as DiaScene2D: camera/light/entity `instance_data` is captured verbatim during deserialization, resolved at load time by SceneLoader3D. Keeps the format layer free of diaentitytemplate dependency.

8. **`StaticMeshEntry` in scene file** — Static geometry (walls, terrain pieces, props) is declared directly in the scene file with `mesh_asset_id` + `material_id` + transform. These are not entities; they have no components. SceneLoader3D registers them as `SceneNode3D` entries of type `Group` (no refId). Submit emits a `Mesh3DDrawCommand` for each visible static mesh.

9. **One active camera** — Matching DiaCamera3D and DiaCamera2D policy. Exactly one camera node has `active: true`; SceneLoader3D validates this.

---

## Status

`Approved`
