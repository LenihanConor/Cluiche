# DiaScene2D — Design Decisions

**Date:** 2026-06-01
**Status:** Pre-spec research complete. Ready for `/spec-system`.
**Context:** Gap discovered comparing DiaGraphics3D (which has DiaScene3D) against the 2D pipeline which has no scene abstraction. Research expanded to audit all configuration file formats and establish the content-vs-config boundary.

---

## 1. File Format Hierarchy — Config vs Content

The existing `.diagame → .diastage → .diaapp` chain handles **configuration** (how the engine is wired). The gap is **content** (what's in the game world).

```
.diagame                         ← ROOT: game identity, window, paths, imports
 ├── .diaobservation             ← Logging/trace/metrics/health config
 ├── assets.catalogue.json       ← Asset registry (IDs, types, paths, dependency graph)
 │    └── assets.rules.json      ← Auto-tagging rules for assets
 ├── cluiche_main.diaapp         ← GLOBAL manifest: stages, streams, PUs, modules
 └── *.diastage                  ← Per-stage: name, path aliases, scene ref, stage config
      ├── <stage>.diaapp         ← Per-stage manifest: additive modules (runtime wiring)
      └── <stage>.diascene       ← World content: what to place (NEW)
```

### Content assets (via catalogue)

| Asset type | Extension | Purpose |
|---|---|---|
| Scene | `.diascene` | Spatial content — cameras, lights, layers, entity placements |
| Entity blueprint | `.diaentitytemplatetemplate` | Reusable archetype — component composition with defaults |
| Texture | `.png` | Visual assets |
| Shader | `.frag` / `.vert` | Render programs |

### Stage as the bridge

The `.diastage` bridges config and content:

```json
{
  "name": "ForestLevel",
  "manifest": "stages/Forest/misc/ApplicationFlow/forest.diaapp",
  "scene": "stages/Forest/forest.diascene",
  "config": {
    "path_aliases": { "stage_root": "." },
    "physics": { "gravity": [0, 9.81] },
    "rendering": {
      "clear_colour": [0.1, 0.1, 0.15, 1.0],
      "layer_rendering": {
        "particles": { "render_technique": "additive_bloom" }
      }
    }
  }
}
```

Gameplay tuning (gravity, clear_colour, render techniques) lives in stage config — not in the scene file.

---

## 2. Scene File Format (`.diascene`)

**Extension:** `.diascene` (single extension for both 2D and 3D).
**Discriminator:** Top-level key (`scene2d` or `scene3d`) selects the reflected struct.
**Serialization:** Reflected type via `JsonArchive`. No custom serializer.

### Scene2D shape

```json
{
  "scene2d": {
    "world_bounds": { "min": [0, 0], "max": [1920, 1080] },
    "layers": [
      { "id": "background", "sort_order": -10, "parallax": [0.5, 0.0], "sort_policy": "insertion", "enabled": true },
      { "id": "midground",  "sort_order": 0,   "parallax": [1.0, 1.0], "sort_policy": "insertion", "enabled": true },
      { "id": "foreground", "sort_order": 10,  "parallax": [1.0, 1.0], "sort_policy": "insertion", "enabled": true },
      { "id": "particles",  "sort_order": 20,  "parallax": [1.0, 1.0], "sort_policy": "insertion", "enabled": true, "render_technique": "additive_bloom" }
    ],
    "cameras": [
      { "id": "gameplay", "active": true, "blueprint": "camera_2d_follow", "instance_data": { "Camera2D.position": [400, 300], "FollowBehaviour.offset": [0, -50] } },
      { "id": "cinematic", "blueprint": "camera_2d_basic", "instance_data": { "Camera2D.position": [0, 0] } }
    ],
    "lights": [
      { "id": "campfire", "blueprint": "point_light_warm", "enabled": true, "instance_data": { "PointLight2D.position": [300, 200] }, "affects_layers": ["midground", "foreground"] },
      { "id": "secret_area", "blueprint": "point_light_cool", "enabled": false, "instance_data": { "PointLight2D.position": [800, 400] }, "affects_layers": ["foreground"] }
    ],
    "entities": [
      { "id": "torch_3a7f", "name": "left_torch", "blueprint": "torch", "enabled": false, "instance_data": { "Transform2D.position": [200, 100], "Renderable2D.layer": "foreground", "PointLight2D.colour": [0.3, 0.3, 1.0, 1.0] } },
      { "id": "tile_001", "blueprint": "bg_tile_grass", "instance_data": { "Transform2D.position": [0, 0] } },
      { "id": "player_spawn", "blueprint": "player", "instance_data": { "Transform2D.position": [400, 300] } }
    ]
  }
}
```

### Design rules

- **Purely spatial** — no gameplay config, no name, no rendering policy
- **Everything is an entity** — weight is determined by blueprint, not scene format
- **Cameras and lights are scene-owned** — not entities (too few to justify ECS overhead), but use blueprint/instance_data format for reflection-friendly serialization
- **No sub-scene references** — composition via entity blueprints; render-to-texture for 2D-in-3D
- **No DiaScene base module** — DiaScene2D and DiaScene3D are independent peers

---

## 3. Entity Blueprint Format (`.diaentitytemplatetemplate`)

Blueprints are **assets** (peers to textures in the catalogue). Reusable, scoped global or per-stage.

```json
{
  "id": "torch",
  "components": [
    { "type": "Transform2D", "position": [0, 0] },
    { "type": "Renderable2D", "layer": "foreground" },
    { "type": "Sprite2D", "texture": "torch_spritesheet" },
    { "type": "Animation2D", "default_anim": "flicker" },
    { "type": "PointLight2D", "radius": 80, "colour": [1.0, 0.8, 0.3, 1.0] }
  ]
}
```

### Instance data model

Blueprint defines defaults. Scene instances patch specific fields:

| Concept | What |
|---|---|
| **Blueprint** | Reusable template — what it *is* |
| **Instance** | Placement + delta from template — where it is, how it differs |
| **`instance_data`** | Flat `Component.Field: value` patches via diaentitytemplate reflection |

An instance never adds components the blueprint doesn't have — that's a different blueprint.

---

## 4. Entity Instance Schema

```json
{
  "id": "torch_3a7f",
  "name": "left_torch",
  "blueprint": "torch",
  "enabled": false,
  "instance_data": {
    "Transform2D.position": [200, 100],
    "Renderable2D.layer": "foreground",
    "PointLight2D.colour": [0.3, 0.3, 1.0, 1.0]
  }
}
```

| Field | Required | Default | Notes |
|---|---|---|---|
| `id` | yes | — | StringCRC, auto-generated, stable identifier |
| `name` | no | none | Human-friendly, for gameplay/editor reference |
| `blueprint` | yes | — | StringCRC reference to `.diaentitytemplatetemplate` asset |
| `enabled` | no | true | Dormant until gameplay activates |
| `instance_data` | no | none | Flat `Component.Field: value` patches |

Position is NOT a special field — it's `Transform2D.position` in `instance_data` like everything else.

---

## 5. Camera System — Proxy Pattern

### Principle

The runtime camera is a lightweight value in a registry. The scene file is a *data-driven path* to populate it. Code can always bypass the file.

### Runtime types (no reflection, no serialization)

```cpp
struct Camera2D { Vec2 position; float zoom; Vec4 viewport; bool active; };

class ICameraBehaviour { virtual void Update(Camera2D& camera, float dt) = 0; };

class CameraRegistry2D {
    void Register(StringCRC id, Camera2D cam);
    void AttachBehaviour(StringCRC camId, ICameraBehaviour* behaviour);
    void SetActive(StringCRC id);
    Camera2D& GetActive();
    void UpdateAll(float dt);
};
```

### Code-only path (no scene file needed)

```cpp
auto& reg = cameraModule.GetRegistry();
reg.Register("debug_cam"_crc, Camera2D{...});
reg.AttachBehaviour("debug_cam"_crc, CameraBehaviourRegistry::Get().Create("Follow"_crc, config));
```

### Data-driven path (scene file)

Scene camera entries use the blueprint/instance_data format. The loader resolves the blueprint, applies instance_data patches, constructs the Camera2D + behaviours, and registers them. No live entity in ECS.

### Behaviour factory (generic loader)

Each behaviour self-registers its factory. The loader never mentions specific types:

```cpp
class CameraBehaviourRegistry {
    HashTable<StringCRC, BehaviourFactory> mFactories;
public:
    void Register(StringCRC typeId, BehaviourFactory factory);
    ICameraBehaviour* Create(StringCRC typeId, const ReflectedConfig& config);
};

// Loader — fully generic
for (const auto& comp : bp.components) {
    if (comp.type == "Camera2D"_crc) continue;
    auto config = comp.defaults;
    entry.instanceData.ApplyTo(config);
    auto* behaviour = CameraBehaviourRegistry::Get().Create(comp.type, config);
    registry.AttachBehaviour(entry.id, behaviour);
}
```

Adding a new behaviour = implement class + register factory. Loader unchanged.

### Engine-provided behaviours

| Behaviour | What it does | App wiring |
|---|---|---|
| **Follow** | Track a target position with offset | `SetTarget(pos)` |
| **SmoothDamp** | Exponential smoothing on position | — (automatic) |
| **Deadzone** | Don't move until target leaves central region | — (automatic) |
| **BoundsClamp** | Clamp within world bounds | — (reads world_bounds) |
| **ScreenShake** | Additive trauma-based shake | `Trigger(trauma)` |
| **ZoomToFit** | Auto-zoom to keep N targets in view | `SetTargets(...)` |
| **Pan** | Move camera position by a delta | `SetDelta(vec)` from input |
| **Zoom** | Scale zoom by factor, clamp to min/max | `SetInput(scrollDelta)` from input |

Behaviours don't read input directly. They expose setters — application wires input to them.

---

## 6. Light System — Same Pattern

### Runtime type

```cpp
struct PointLight2D { Vec2 position; float radius; Vec4 colour; float intensity; uint32_t layerMask; bool enabled; };

class LightRegistry2D {
    void Register(StringCRC id, PointLight2D light);
    PointLight2D& Get(StringCRC id);
    const DynamicArrayC<PointLight2D>& GetAll() const;
};
```

### Scene file

```json
{ "id": "campfire", "blueprint": "point_light_warm", "enabled": true, "instance_data": { "PointLight2D.position": [300, 200] }, "affects_layers": ["midground", "foreground"] }
```

### Loader

- Resolves blueprint, applies instance_data
- Resolves `affects_layers` names → bitmask via layer table
- Registers into `LightRegistry2D`

### Constraints

- v1: Point lights only. No behaviours (flicker/pulse deferred to v2).
- `affects_layers` resolved by name at load time, stored as uint32 bitmask at runtime.
- Max 32 layers (uint32 bitmask).
- `enabled` field for dormant lights (defaults true).

---

## 7. Layer Schema

```json
{ "id": "background", "sort_order": -10, "parallax": [0.5, 0.0], "sort_policy": "insertion", "enabled": true }
```

| Field | Required | Default | Notes |
|---|---|---|---|
| `id` | yes | — | StringCRC, referenced by entities and lights |
| `sort_order` | yes | — | Draw order (lower = further back) |
| `parallax` | no | [1.0, 1.0] | Vec2 scroll multiplier (independent X/Y) |
| `sort_policy` | yes | — | v1: `insertion` only. Future: `y_sort`, `explicit` |
| `enabled` | no | true | Toggle entire layer visibility |
| `render_technique` | no | none | Reference to a render technique asset (future — backlogged) |

### Entities and layers

- `Renderable2D` component has a `layer` field (StringCRC)
- Blueprint sets a default layer
- Instance can override via `instance_data`
- Engine provides a `"default"` layer fallback so entities always render somewhere

---

## 8. World Bounds

**Optional.** If omitted, camera moves freely with no clamping.

```json
"world_bounds": { "min": [0, 0], "max": [1920, 1080] }
```

Runtime warns if `BoundsClampBehaviour` is attached and no world_bounds exist.

---

## 9. Module Structure

Three independent modules. DiaScene2D consumes the other two.

```
DiaCamera2D (new)
 ├── Camera2D value type
 ├── CameraRegistry2D
 ├── ICameraBehaviour + factory registry
 ├── FollowBehaviour, SmoothDampBehaviour, DeadzoneBehaviour,
 │   BoundsClampBehaviour, ScreenShakeBehaviour, ZoomToFitBehaviour,
 │   PanBehaviour, ZoomBehaviour
 └── Dependencies: DiaMaths, DiaGeometry2D

DiaLighting2D (new)
 ├── PointLight2D value type
 ├── LightRegistry2D
 └── Dependencies: DiaMaths

DiaScene2D (new)
 ├── Scene2D reflected struct
 ├── SceneLoader2D (populates camera/light registries + spawns entities)
 ├── Layer management (layer table, bitmask resolution)
 └── Dependencies: DiaCamera2D, DiaLighting2D, diaentitytemplate, DiaReflect, DiaMaths, DiaGeometry2D
```

### Key independence guarantee

DiaCamera2D and DiaLighting2D do NOT depend on DiaScene2D. They are usable standalone — programmatic registration works without any scene file, any `.diascene`, or any DiaScene2D dependency.

### Camera2D moves out of DiaGraphics

Camera2D is more than a data format — it has runtime behaviour. Full ownership moves to DiaCamera2D. DiaGraphics depends on DiaCamera2D for the type (or takes a view/projection matrix).

---

## 10. 3D Scene (future)

No `DiaScene` base module. DiaScene2D and DiaScene3D are independent peers:

```
DiaScene2D                DiaScene3D (future)
 ├── DiaCamera2D           ├── DiaCamera3D
 ├── DiaLighting2D         ├── DiaLighting3D
 ├── diaentitytemplate             ├── diaentitytemplate
 ├── DiaReflect            ├── DiaReflect
 ├── DiaMaths              ├── DiaMaths
 └── DiaGeometry2D         └── DiaGeometry3D
```

### 2D-in-3D scenario

A 3D world with an embedded 2D scene (e.g., arcade cabinet screen) is handled via rendering, not scene nesting. An entity in the 3D scene has a `SceneViewport2D` component that renders a 2D scene to a texture. Scenes don't reference each other.

---

## 11. Decisions Summary

| # | Decision | Rationale |
|---|---|---|
| 1 | `.diascene` not `.diascene2d` | Type field inside; less format proliferation |
| 2 | Top-level key discriminator (`scene2d`/`scene3d`) | Each maps to a distinct reflected struct; no dead fields |
| 3 | Reflected struct, no custom serializer | JsonArchive; editor gets free read/write |
| 4 | No sub-scene references | Composition via blueprints; render-to-texture for cross-dimension |
| 5 | Everything is an entity | Weight determined by blueprint; one editor workflow |
| 6 | Cameras/lights scene-owned (not entities) | Too few for ECS overhead; registry pattern instead |
| 7 | Blueprint/instance_data format for cameras and lights | Solves reflection without polymorphic serialization |
| 8 | Behaviour factory registry | Generic loader; new behaviours self-register |
| 9 | No gameplay config in scene | Gravity, clear_colour, render techniques → `.diastage` config |
| 10 | No name in scene | Identity from catalogue/stage |
| 11 | `instance_data` not `overrides` | Neutral term; aligned with diaentitytemplate reflection |
| 12 | Position is not special-cased | Just `Transform2D.position` in instance_data |
| 13 | Layers are 2D-specific | 3D uses depth buffer + render passes; no shared concept |
| 14 | Layer render technique as reference | Keeps scene spatial; render policy lives elsewhere |
| 15 | Parallax is Vec2 | Independent X/Y for all game orientations |
| 16 | Light layer affinity via names | Resolved to uint32 bitmask at load time |
| 17 | sort_policy explicitly declared | v1: insertion only; future policies additive |
| 18 | No DiaScene base module | Nothing meaningful to share; peers with overlapping deps |
| 19 | Camera2D moves to DiaCamera2D | Runtime behaviour belongs with the type |
| 20 | Pan/Zoom as engine behaviours | Capability without input wiring; app feeds them |
| 21 | World bounds optional | Defaults unbounded; runtime warns if BoundsClamp has no bounds |
| 22 | Cameras/lights `enabled` field | Dormant until gameplay activates |
| 23 | Entity instances have id (required) + name (optional) | StringCRC for stable tooling/save; name for human intent |

---

## 12. Open Questions (for `/spec-system` interview)

1. **DiaGraphics dependency direction** — DiaGraphics currently owns Camera2D. Moving it to DiaCamera2D means DiaGraphics either depends on DiaCamera2D (for FrameData) or takes a raw view/projection matrix. Which?

2. **`.diaentitytemplatetemplate` format alignment** — diaentitytemplate already has a blueprint format (version, entities, references). Does the `.diaentitytemplatetemplate` asset format match exactly, or is the scene-referenced format a simplified subset?

3. **Default layer** — Engine provides a `"default"` layer. Is this implicit (always exists even if not in the layers array) or must the scene file declare it?

4. **Camera validation** — Exactly one camera must be `active: true`. Validated at pipeline time and asserted at runtime. Is this a hard error or a recoverable warning (activate the first camera)?

5. **Light2D** — Include in v1 scope or defer? Normal-map lighting requires shader support in DiaBgfx. Is the registry + data format worth shipping without visual output?

---

## Next Step

Run `/spec-system` for DiaScene2D (and potentially DiaCamera2D, DiaLighting2D as separate system specs) using this document as pre-work.
