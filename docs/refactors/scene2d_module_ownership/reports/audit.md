# Refactor Audit — Scene2D Module Ownership

**Session date:** 2026-06-04
**Folder:** docs/refactors/scene2d_module_ownership/

## Subsystem Summary

The `Scene2DModule` (in CluicheGameBaseline) is responsible for loading `.diascene` files and exposing the resulting cameras, lights, entities, and layers to sibling modules. It currently **owns** instances of `CameraRegistry2D`, `LightRegistry2D`, and `Entity::Domain` — duplicating ownership that should live in dedicated per-system modules.

Separately, `CameraModule` already exists and owns its own `CameraRegistry2D` with per-frame updates, viewport sync, and a default camera. No equivalent `Light2DModule` exists yet.

## Strengths

- Scene2DModule's load/unload pattern via `SceneLoader2D` + `SceneLoadContext` is clean and well-scoped
- `SceneLoadContext` is already reference-based — it doesn't copy registries, just holds refs
- ModuleRef pattern already used by consumers (Scene2DTestStageModule, VisualDebuggerModule, PickingModule)
- Clear separation of loader (Scene2DModule) vs validator (Scene2DTestStageModule)
- CameraModule already has the correct ownership/update pattern to follow as a template

## Debt

| # | Issue | Impact |
|---|-------|--------|
| D1 | Scene2DModule owns `mCameraRegistry` while CameraModule also owns one — scene-loaded cameras live in a separate world from the actively-updated registry | Scene cameras invisible to PickingModule/VisualDebuggerModule which read CameraModule |
| D2 | Scene2DModule owns `mEntityDomain` while EntityModule also owns one — scene-spawned entities not updated by EntityModule's `Update(dt)`/`EndOfFrame()` | Entities are static unless Scene2DTestStageModule manually calls `EndOfFrame()` |
| D3 | Scene2DModule owns `mLightRegistry` with no per-frame driver — no module ticks lights | Lights cannot animate or respond to runtime changes |
| D4 | CameraModule is named `CameraModule` but wraps `Dia::Camera2D::*` types — naming mismatch with engine namespace | Confusing when both Camera2D and a hypothetical 3D camera exist |
| D5 | Scene2DModule calls `mEntityDomain.EndOfFrame()` in `DoStop()` — lifecycle method on a domain it shouldn't own | Fragile; if EntityModule is also running, double-flush possible |

## Duplication

- Two `CameraRegistry2D` instances: one in CameraModule, one in Scene2DModule
- Two `Entity::Domain` instances: one in EntityModule, one in Scene2DModule
- `SceneLoadContext` construction duplicated in `DoStart()` and `DoStop()`

## Hidden Coupling

- `Scene2DTestStageModule` accesses the Scene2DModule's entity domain mutably (`GetEntityDomain()` non-const) to register pools and add components — this bypasses EntityModule entirely
- `VisualDebuggerModule` and `PickingModule` use `ModuleRef<CameraModule>` — they will need to update to `Camera2DModule` after rename
- The `.diaapp` manifests reference `CameraModule` by type_id string — renaming requires manifest updates
- `Scene2DTestStageModule` does NOT declare a dependency on `EntityModule` or `CameraModule` — it only depends on `Scene2DModule` because Scene2DModule owns everything

## Missing Tests

- No GoogleTest coverage for Scene2DModule's module lifecycle (start/stop/IsLoaded)
- No test verifying that scene-loaded entities appear in the same domain that EntityModule updates
- No test verifying that scene-loaded cameras are accessible via CameraModule's registry

## Likely Invariants to Preserve

| # | Invariant |
|---|-----------|
| I1 | After scene load, `GetCameraRegistry()` returns a registry containing all cameras declared in the `.diascene` |
| I2 | After scene load, `GetLightRegistry()` returns a registry containing all lights declared in the `.diascene` |
| I3 | After scene load, `GetEntityDomain()` returns a domain with all entities spawned from the `.diascene` |
| I4 | `SceneLoader2D::Unload()` removes exactly what it loaded (tracked cameras/lights/entities) |
| I5 | CameraModule's default camera is always registered and active, even after a scene loads additional cameras |
| I6 | EntityModule's hierarchy component pools (Parent/ChildBuffer) are registered before scene load creates entities |
| I7 | `Scene2DTestStage` passes all 5 checkpoints after the refactor |
| I8 | PickingModule and VisualDebuggerModule continue to access camera via the same runtime module |

## Recommended Cleanup Order

1. **Rename CameraModule -> Camera2DModule** — Mechanical rename across code, manifests, and vcxproj. Establishes the naming convention before new work.
2. **Create Light2DModule** — New module owning `LightRegistry2D`, following CameraModule pattern. No per-frame update logic yet (registry has no `UpdateAll`), but establishes ownership.
3. **Rewire Scene2DModule** — Remove `mEntityDomain`, `mCameraRegistry`, `mLightRegistry`. Add `ModuleRef<EntityModule>`, `ModuleRef<Camera2DModule>`, `ModuleRef<Light2DModule>`. Build `SceneLoadContext` from their registries.
4. **Update Scene2DTestStage .diaapp** — Add `EntityModule`, `Camera2DModule`, `Light2DModule` to stage; update Scene2DModule dependencies.
5. **Update Scene2DTestStageModule** — Access domain via EntityModule instead of Scene2DModule's accessor. Scene2DModule still exposes `GetLayerTable()` and `IsLoaded()`.
6. **Update VisualDebuggerModule/PickingModule references** — ModuleRef<CameraModule> -> ModuleRef<Camera2DModule>.
7. **Update global .diaapp manifest** — Rename CameraModule type_id to Camera2DModule.
