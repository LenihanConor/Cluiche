# Refactor Plan — Scene2D Module Ownership

**Input:** docs/refactors/scene2d_module_ownership/outputs/audit.json

## Refactor Goal

Scene2DModule becomes a pure loader/unloader that populates sibling modules' registries — it no longer owns cameras, lights, or entities.

## Current Problem

Scene2DModule owns duplicate `CameraRegistry2D`, `LightRegistry2D`, and `Entity::Domain` instances. Scene-loaded objects live in a parallel world, invisible to the modules that update and expose them to the rest of the application (EntityModule, CameraModule). There is no Light module at all.

## Target Shape

```
EntityModule        — owns Entity::Domain, drives Update/EndOfFrame
Camera2DModule      — owns CameraRegistry2D, drives UpdateAll, viewport sync
Light2DModule (new) — owns LightRegistry2D, exposes mutable/const access
Scene2DModule       — owns SceneLoader2D + LayerTable only; holds ModuleRefs to the above three;
                      builds SceneLoadContext from their registries; loads/unloads scene content
```

All scene-loaded cameras, lights, and entities live in the **single authoritative** registry/domain owned by their respective module. Scene2DModule tracks what it loaded for cleanup but does not own the storage.

### Boundary Changes

| What | Moves from | Moves to |
|------|-----------|----------|
| `CameraRegistry2D` ownership | Scene2DModule | Camera2DModule (already in CameraModule, just renamed) |
| `LightRegistry2D` ownership | Scene2DModule | Light2DModule (new) |
| `Entity::Domain` ownership | Scene2DModule | EntityModule (already owns one) |
| `LayerTable` ownership | Scene2DModule | Stays in Scene2DModule |
| `SceneLoader2D` | Scene2DModule | Stays in Scene2DModule |

### Responsibilities Before vs After

| Responsibility | Before | After |
|----------------|--------|-------|
| Own CameraRegistry2D | Scene2DModule + CameraModule (duplicate) | Camera2DModule only |
| Own LightRegistry2D | Scene2DModule (no updater) | Light2DModule only |
| Own Entity::Domain | Scene2DModule + EntityModule (duplicate) | EntityModule only |
| Drive camera updates | CameraModule (on its own registry) | Camera2DModule (same registry scene loads into) |
| Drive entity Update/EndOfFrame | EntityModule (on its own domain) | EntityModule (same domain scene loads into) |
| Load .diascene | Scene2DModule | Scene2DModule (unchanged) |
| Own LayerTable | Scene2DModule | Scene2DModule (unchanged) |
| Track loaded items for cleanup | Scene2DModule via SceneLoader2D | Scene2DModule via SceneLoader2D (unchanged) |

## Affected Systems

- `VisualDebuggerModule` — `ModuleRef<CameraModule>` becomes `ModuleRef<Camera2DModule>`
- `PickingModule` — same rename
- `Scene2DTestStageModule` — accesses domain/cameras/lights via new module refs
- `Scene2DTestDrawer` — receives refs from the correct modules
- `.diaapp` manifests (global + scene2d_test_stage) — type_id and dependency updates
- `CluicheGameBaseline.vcxproj` / `.filters` — file additions/renames
- Feature spec `docs/specs/features/cluichegamebaseline/scene2d-module.md` — design section update

## API / Interface Changes

### Camera2DModule (renamed from CameraModule)

```cpp
// Class rename only. Public API unchanged.
class Camera2DModule : public Dia::ApplicationFlow::Module { ... };
// kTypeId changes from "CameraModule" to "Camera2DModule"
```

### Light2DModule (new)

```cpp
class Light2DModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr PUAffinity kAllowedPUs = PUAffinity::kSim;

    Dia::Lighting2D::LightRegistry2D&       GetRegistry();
    const Dia::Lighting2D::LightRegistry2D& GetRegistry() const;

protected:
    StartResult DoStart()  override;
    void DoUpdate(float)   override;  // no-op initially; future: animate lights
    StopResult DoStop()    override;

private:
    Dia::Lighting2D::LightRegistry2D mRegistry;
};
```

### Scene2DModule (revised)

```cpp
class Scene2DModule : public Dia::ApplicationFlow::Module
{
public:
    // Removed: GetCameraRegistry, GetLightRegistry, GetEntityDomain (mutable)
    // Kept:
    const Dia::Scene2D::LayerTable& GetLayerTable() const;
    bool IsLoaded() const;

protected:
    StartResult DoStart()  override;
    StopResult  DoStop()   override;

private:
    Dia::Scene2D::SceneLoader2D mSceneLoader;
    Dia::Scene2D::LayerTable    mLayerTable;
    bool                        mLoaded = false;

    ModuleRef<EntityModule>   mEntityRef{this};
    ModuleRef<Camera2DModule> mCameraRef{this};
    ModuleRef<Light2DModule>  mLightRef{this};
};
```

### Scene2DTestStageModule (revised)

```cpp
// Drops: ModuleRef<Scene2DModule> as sole data source
// Adds: ModuleRef<EntityModule>, ModuleRef<Camera2DModule>, ModuleRef<Light2DModule>
// Keeps: ModuleRef<Scene2DModule> for IsLoaded() and GetLayerTable()
```

## Migration Phases

| Phase | Name | Description |
|-------|------|-------------|
| 1 | Rename CameraModule → Camera2DModule | Rename class, file, kTypeId, all ModuleRef sites, all .diaapp manifests. Mechanical. |
| 2 | Create Light2DModule | New header/cpp, vcxproj entries, add to .diaapp manifests for relevant stages. |
| 3 | Rewire Scene2DModule | Remove owned registries/domain, add ModuleRefs, build SceneLoadContext from siblings. Update scene2d_test_stage.diaapp dependencies. |
| 4 | Update consumers | Scene2DTestStageModule, Scene2DTestDrawer access cameras/lights/entities from their owning modules. Update spec. |

## Minimal Viable Slice

Phase 3 alone (rewire Scene2DModule) delivers the core value — single ownership. But phases 1 and 2 are prerequisites since Scene2DModule needs Camera2DModule and Light2DModule to exist with the correct names before it can reference them.

The true MVS is phases 1-3 together. Phase 4 is a follow-on that updates consumers to use the new direct module refs.

## Risks and Rollback Points

| Risk | Mitigation | Rollback |
|------|-----------|----------|
| Module start order — Scene2DModule loads before EntityModule/Camera2DModule are started | .diaapp dependency declarations enforce ordering; Scene2DModule depends on all three | Revert Scene2DModule to owning its registries |
| CameraModule rename breaks string-based lookup | kTypeId string change + manifest update done atomically in phase 1 | `git revert` phase 1 commit |
| Scene entities created before EntityModule registers pools | EntityModule starts first (dependency); hierarchy pools registered in EntityModule::DoStart | Keep current pool-registration order |
| Light2DModule has no UpdateAll — lights don't animate | Acceptable for now; LightRegistry2D has no UpdateAll API today | N/A |

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|-------------|
| A1 | `dia run cluichetest` — Scene2DTestStage passes all 5 checkpoints | Run and check output |
| A2 | `dia run googletest` — all existing tests pass | Run |
| A3 | Only one `Entity::Domain` instance exists at runtime in SimPU | Code inspection: only EntityModule owns one |
| A4 | Only one `CameraRegistry2D` instance exists at runtime in SimPU | Code inspection: only Camera2DModule owns one |
| A5 | Only one `LightRegistry2D` instance exists at runtime in SimPU | Code inspection: only Light2DModule owns one |
| A6 | Scene-loaded cameras visible to PickingModule/VisualDebuggerModule via Camera2DModule | Camera2DModule's registry is the one scene loads into; same ModuleRef chain |
| A7 | Scene2DModule has no `#include <DiaEntity/Domain.h>`, `<DiaCamera2D/Registry/CameraRegistry2D.h>`, or `<DiaLighting2D/Registry/LightRegistry2D.h>` | Grep |
| A8 | CameraModule name does not appear anywhere in code or manifests | Grep |
