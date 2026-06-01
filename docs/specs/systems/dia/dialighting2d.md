# System Spec: DiaLighting2D

## Parent Application
@docs/specs/applications/dia.md

**Status:** `Approved`

**Research:** @docs/research/diascene2d/design-decisions.md (section 6)

---

## Purpose

DiaLighting2D is the engine library for 2D point light management. It owns the PointLight2D value type and a named light registry with layer-mask affinity.

Like DiaCamera2D, this is a **library** — no PU/Module coupling. Application-side code owns the registry instance. DiaScene2D's SceneLoader populates it from `.diascene` files as an optional data-driven path.

v1 ships the data model and registry without rendering output. Visual effect (normal-map lighting, ambient/diffuse shader) requires future DiaBgfx shader work. The registry is ready to be consumed by the renderer once shaders land.

```
DiaLighting2D (engine library)
    ↑ used by
Application-side module (owns registry)
    ↑ used by
DiaScene2D SceneLoader (optional — populates registry from file)
    ↑ consumed by
DiaBgfx renderer (future — reads lights, applies to draw passes)
```

**Dependency chain:**
`DiaLighting2D → DiaMaths → DiaCore`

---

## Responsibilities

- Own `PointLight2D` value type (position, radius, colour, intensity, layer mask, enabled)
- Provide `LightRegistry2D` — named light storage with add/remove/get/query
- Provide layer-mask resolution (names → bitmask) via a `LayerTable` reference at query time
- Provide `DiaLighting2D.vcxproj` static library project registered in `Cluiche.sln`
- Provide `dia.lighting2d.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Rendering lights visually (DiaBgfx shader work — future)
- Normal-map generation or asset pipeline (DiaAssetPipeline)
- Light behaviours (flicker, pulse — v2 backlog)
- PU/Module integration (application-side)
- Scene file loading (DiaScene2D)
- 3D lights (future DiaLighting3D — independent peer)

---

## Public Interfaces

### PointLight2D

```cpp
namespace Dia::Lighting2D
{
    struct PointLight2D
    {
        Dia::Maths::Vector2D position = {0.0f, 0.0f};
        float radius = 100.0f;
        float colour[4] = {1.0f, 1.0f, 1.0f, 1.0f};  // RGBA
        float intensity = 1.0f;
        uint32_t layerMask = 0xFFFFFFFF;  // affects all layers by default
        bool enabled = true;
    };
}
```

### LightRegistry2D

```cpp
namespace Dia::Lighting2D
{
    class LightRegistry2D
    {
    public:
        void Register(Dia::Core::StringCRC id, PointLight2D light);
        void Unregister(Dia::Core::StringCRC id);

        PointLight2D& Get(Dia::Core::StringCRC id);
        const PointLight2D& Get(Dia::Core::StringCRC id) const;

        bool Has(Dia::Core::StringCRC id) const;

        // Iteration for renderer consumption
        unsigned int GetCount() const;
        const PointLight2D& GetByIndex(unsigned int index) const;

        // Query: lights affecting a given layer index
        // Caller provides layer bit index, registry filters by layerMask
        void GetLightsForLayer(unsigned int layerBitIndex,
                               Dia::Core::DynamicArrayC<const PointLight2D*, 32>& outLights) const;
    };
}
```

### Layer mask resolution

Layer mask is stored as a `uint32_t` bitmask on each light. The mapping from layer names (StringCRC) to bit indices is owned by the layer table (lives in DiaScene2D or application code). DiaLighting2D doesn't depend on DiaScene2D — it just stores and queries by bitmask.

The loader resolves names to bitmask at load time:

```cpp
// Application/SceneLoader code (not in DiaLighting2D):
uint32_t mask = layerTable.ResolveMask(entry.affectsLayers);  // names → bits
light.layerMask = mask;
registry.Register(entry.id, light);
```

---

## Dependencies

| Dependency | What's used |
|---|---|
| DiaMaths | Vector2D |
| DiaCore | StringCRC, containers (HashTable, DynamicArrayC) |

### Does NOT depend on

- DiaGraphics
- DiaCamera2D
- DiaScene2D
- DiaEntity
- DiaGeometry2D (no spatial queries in v1)
- DiaApplicationFlow
- DiaBgfx

---

## Features

| Feature | Description | Spec |
|---|---|---|
| lighting2d-system | PointLight2D type, LightRegistry2D, layer mask storage, query by layer | TBD |

---

## Inherited Binding Decisions

| ID | Decision | How it applies |
|---|---|---|
| PD-001 | StringCRC for all IDs | Light IDs are StringCRC |
| PD-004 | No STL containers in public APIs | Registry uses DynamicArrayC/HashTable |
| PD-005 | x64 only | Single build target |
| PD-007 | C++20 required | Module uses C++20 features |
| PD-008 | Directory.Build.props owns build paths | Library outputs to `bin/sharedlibs/<Config>/<Platform>/` |
| AD-001 | Module system with YAML frontmatter | `dia.lighting2d.architecture.module.md` required |
| AD-002 | No STL in public APIs | Same as PD-004 |
| AD-003 | Namespace convention `Dia::<Module>::` | `Dia::Lighting2D::` namespace |

---

## Resolved Design Questions

1. **v1 without rendering** — Ship data model + registry now. Unblocks DiaScene2D scene loading. Renderer consumes the registry once DiaBgfx 2D lighting shaders land.

2. **No behaviours in v1** — Flicker, pulse, etc. are v2. When added, they follow the same factory pattern as DiaCamera2D behaviours (self-registering, generic loader).

3. **Layer mask vs layer reference** — Stored as uint32_t bitmask (max 32 layers). Name resolution happens at load time outside this module. DiaLighting2D is layer-name-agnostic.

---

## Status

`Approved`

**Plan:** @docs/specs/systems/dia/dialighting2d.plan.md
