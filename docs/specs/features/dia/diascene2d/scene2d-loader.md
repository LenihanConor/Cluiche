# Feature Spec: scene2d-loader

## Parent System
@docs/specs/systems/dia/diascene2d.md

**Status:** `Done`

---

## Summary

`SceneLoader2D` reads a `.diascene` file and populates `CameraRegistry2D`, `LightRegistry2D`, and `Dia::Entity::Domain` from it. It also builds the `LayerTable` and owns the cleanup path (Unload).

---

## Goals

- `Load()` parses a `.diascene` JSON file, builds the `LayerTable`, hydrates camera/light registries, and spawns entities into the Domain
- `Unload()` reverses all registrations made by the last `Load()` call
- Cameras: blueprint → Camera2D value + behaviours via `CameraBehaviourRegistry`, registered into `CameraRegistry2D`
- Lights: blueprint stub → `PointLight2D` with `affectsLayers` resolved to bitmask, registered into `LightRegistry2D`
- Entities: spawned via `Domain::CreateEntity()` + component instantiation; `instanceData` patched via component field reflection
- Validation: exactly one camera `active: true` → hard error; missing blueprints logged + skipped; invalid layer names → logged warning, bit 0 fallback
- `Load()` returns bool; error details via `SerializeResult`-style accumulation returned by out-param

---

## Interfaces

```cpp
namespace Dia::Scene2D {

    struct SceneLoadContext {
        Dia::Camera2D::CameraRegistry2D&     cameraRegistry;
        Dia::Lighting2D::LightRegistry2D&    lightRegistry;
        Dia::Entity::Domain&                 entityDomain;
    };

    struct SceneLoadErrors {
        bool hasErrors = false;
        // Per-entry error accumulation (camera/light/entity id + description)
        // v1: simple bool flag; detailed list deferred
    };

    class SceneLoader2D {
    public:
        // Load .diascene — populates all target systems.
        // Returns false on hard errors (parse failure, zero active cameras, >1 active camera).
        bool Load(const char* filePath,
                  SceneLoadContext& context,
                  LayerTable& outLayers,
                  SceneLoadErrors* outErrors = nullptr);

        // Unload — unregister cameras/lights, destroy spawned entities.
        void Unload(SceneLoadContext& context);

    private:
        // Tracking for Unload
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4>   mRegisteredCameras;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16>  mRegisteredLights;
        Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, 256>  mSpawnedEntities;
    };
}
```

---

## Load Algorithm

1. Parse JSON file with `Json::Reader`; fail hard if parse error
2. Check top-level key `"scene2d"` is present; fail hard if missing
3. Deserialize `Scene2D` struct via `JsonReadArchive` on the `scene2d` sub-node
4. Call `LayerTable::Build(scene.layers)` → `outLayers`
5. Validate cameras: count `active: true` entries; hard error if != 1
6. **Hydrate cameras** — for each `CameraEntry`:
   - Resolve blueprint (v1: treated as a named behaviour type; see note below)
   - Construct `Camera2D` with defaults from `instanceData["Camera2D.*"]` patches
   - For each behaviour key in blueprint (v1: skip behaviour construction — register camera only)
   - Register in `cameraRegistry`; call `SetActive` if `active: true`
   - Track id in `mRegisteredCameras`
7. **Hydrate lights** — for each `LightEntry`:
   - Construct default `PointLight2D`; apply `instanceData["PointLight2D.*"]` patches
   - Resolve `affectsLayers` names → bitmask via `LayerTable::ResolveMask`
   - Set `light.layerMask`; set `light.enabled`
   - Register in `lightRegistry`; track id in `mRegisteredLights`
8. **Spawn entities** — for each `EntityInstance`:
   - Skip if `enabled: false` (v1: dormant entities not spawned)
   - Call `domain.CreateEntity()`
   - For each `instanceData` key `"ComponentType.fieldName": value`, apply field patch via component reflection (v1: best-effort, unknown keys silently ignored)
   - Track handle in `mSpawnedEntities`

**Blueprint resolution note (v1):** Full blueprint asset loading requires `DiaAssetRuntime`. For v1, camera blueprints are used as a named Camera2D config shortcut only — `instanceData` is applied directly to a default `Camera2D`. Behaviour construction from blueprint is deferred to v2 when asset pipeline integration exists.

---

## Unload Algorithm

1. For each id in `mRegisteredCameras`: `cameraRegistry.Unregister(id)`
2. For each id in `mRegisteredLights`: `lightRegistry.Unregister(id)`
3. For each handle in `mSpawnedEntities`: `domain.QueueDestroyEntity(handle)`; flush
4. Clear all three tracking arrays

---

## Tasks

| # | Task |
|---|------|
| 1 | `SceneLoadContext.h` — context struct |
| 2 | `SceneLoader2D.h` — class declaration + `SceneLoadErrors` |
| 3 | `SceneLoader2D.cpp` — `Load()` + `Unload()` + validation |
| 4 | GoogleTests `DiaScene2D/TestScene2DLoader.cpp` |

---

## Binding Constraints

- **PD-001** — StringCRC for all IDs
- **PD-004 / AD-002** — No STL in public APIs
- **AD-003** — Namespace `Dia::Scene2D::`

## Open Design Questions

1. **Entity instanceData patching** — v1 applies `instanceData` as best-effort field patches via component reflection. The exact API path through DiaEntity's field descriptor system needs to be confirmed during implementation; the spec says "supported field patching" but if DiaEntity doesn't expose a public field-set-by-name API, v1 may need to skip instanceData for entities and log a warning.

2. **Camera blueprint vs programmatic Camera2D** — v1 treats blueprints as camera config presets applied via instanceData only. Full blueprint loading (behaviours, component composition) is deferred to v2 when DiaAssetRuntime integration exists.
