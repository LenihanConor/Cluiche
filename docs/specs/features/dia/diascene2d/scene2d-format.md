# Feature Spec: scene2d-format

## Parent System
@docs/specs/systems/dia/diascene2d.md

**Status:** `Done`

---

## Summary

Defines the `Scene2D` reflected struct family, `LayerTable`, and the `.diascene` file schema. This is the pure data-model layer — no I/O, no registry hydration. All types are serializable via `DIA_SERIALIZE` free functions and `JsonReadArchive`/`JsonWriteArchive`.

---

## Goals

- `Scene2D` struct and all sub-structs compile and serialize roundtrip via `JsonArchive`
- `LayerTable` builds from a `DynamicArrayC<LayerDef>`, resolves name→bit index, computes uint32 bitmasks
- Default layer (`"default"`) is auto-injected if absent from the layers array
- All types in `Dia::Scene2D::` namespace, header-only except `LayerTable.cpp`

---

## Types

### LayerDef

```cpp
namespace Dia::Scene2D {
    struct LayerDef {
        Dia::Core::StringCRC id;
        int                  sortOrder         = 0;
        Dia::Maths::Vector2D parallax          = {1.0f, 1.0f};
        Dia::Core::StringCRC sortPolicy;       // v1: "insertion" only
        bool                 enabled           = true;
        Dia::Core::StringCRC renderTechnique;  // optional, kEmpty if unset
    };
}
```

### CameraEntry

```cpp
namespace Dia::Scene2D {
    struct CameraEntry {
        Dia::Core::StringCRC id;
        bool                 active    = false;
        Dia::Core::StringCRC blueprint;
        Json::Value          instanceData;  // opaque — resolved by SceneLoader2D
    };
}
```

### LightEntry

```cpp
namespace Dia::Scene2D {
    struct LightEntry {
        Dia::Core::StringCRC                                       id;
        bool                                                       enabled = true;
        Dia::Core::StringCRC                                       blueprint;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> affectsLayers;
        Json::Value                                                instanceData;  // opaque
    };
}
```

### EntityInstance

```cpp
namespace Dia::Scene2D {
    struct EntityInstance {
        Dia::Core::StringCRC id;
        Dia::Core::StringCRC name;      // optional (kEmpty if unset)
        Dia::Core::StringCRC blueprint;
        bool                 enabled = true;
        Json::Value          instanceData;  // opaque — resolved by SceneLoader2D
    };
}
```

### Scene2D

```cpp
namespace Dia::Scene2D {
    struct Scene2D {
        Dia::Geometry2D::AARect                                           worldBounds;  // zero-area = unbounded
        Dia::Core::Containers::DynamicArrayC<LayerDef, 32>               layers;
        Dia::Core::Containers::DynamicArrayC<CameraEntry, 4>             cameras;
        Dia::Core::Containers::DynamicArrayC<LightEntry, 16>             lights;
        Dia::Core::Containers::DynamicArrayC<EntityInstance, 256>        entities;
    };
}
```

### LayerTable

```cpp
namespace Dia::Scene2D {
    class LayerTable {
    public:
        void Build(const Dia::Core::Containers::DynamicArrayC<LayerDef, 32>& layers);

        unsigned int GetBitIndex(Dia::Core::StringCRC layerId) const;
        uint32_t     ResolveMask(const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& layerNames) const;

        unsigned int      GetCount()                    const;
        const LayerDef&   GetByIndex(unsigned int index) const;
        const LayerDef&   GetById(Dia::Core::StringCRC id) const;
        bool              Has(Dia::Core::StringCRC id)  const;

        static constexpr const char* kDefaultLayerIdStr = "default";
    };
}
```

`Build()` injects a `"default"` layer at bit 0 if the input array doesn't already contain it.

---

## Serialization

`DiaScene2DSerializers.h` provides `DIA_SERIALIZE` free functions for `LayerDef`, `Scene2D`.

`CameraEntry`, `LightEntry`, `EntityInstance` have a custom serialize pattern: standard fields serialized via `DIA_FIELD`, `instanceData` captured raw from the JSON node (stored as `Json::Value`, not deserialized further at this layer).

---

## Tasks

| # | Task |
|---|------|
| 1 | `Scene2D.h` — all struct definitions in `Dia::Scene2D::` |
| 2 | `DiaScene2DSerializers.h` — `DIA_SERIALIZE` for `LayerDef` + Scene2D struct family |
| 3 | `LayerTable.h` / `LayerTable.cpp` — Build, GetBitIndex, ResolveMask, GetByIndex, Has, default injection |
| 4 | GoogleTests `DiaScene2D/TestScene2DFormat.cpp` — roundtrip serialization + LayerTable |

---

## Binding Constraints

- **PD-001** — StringCRC for all IDs (layer id, blueprint, camera id, etc.)
- **PD-004 / AD-002** — No STL in public APIs (`DynamicArrayC`, not `std::vector`)
- **AD-003** — Namespace `Dia::Scene2D::`
