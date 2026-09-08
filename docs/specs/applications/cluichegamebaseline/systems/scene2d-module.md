# Feature Spec: Scene2DModule

**Parent:** No parent system spec yet — CluicheGameBaseline application spec is pending. This feature belongs to CluicheGameBaseline, the shared baseline library consumed by CluicheTest and future games.

**Status:** `Approved`

---

## Problem

Loading a `.diascene` file into `CameraRegistry2D`, `LightRegistry2D`, `LayerTable`, and `EntityDomain` is generic gameplay infrastructure — any game stage that declares a scene should get it for free. Currently `Scene2DTestStageModule` owns all of this boilerplate, making it impossible for other stages or games to reuse the pattern without copy-pasting.

---

## Summary

Add `Scene2DModule` to `CluicheGameBaseline`. It owns the scene-loading lifecycle: reads the `stage_scene` path alias registered by `AssetServiceModule`, loads the `.diascene` file on start, exposes the populated registries and domain to sibling modules via read-only accessors, and unloads cleanly on stop. `Scene2DTestStageModule` is refactored to consume `Scene2DModule` via `ModuleRef` and retain only its checkpoint/validation logic.

---

## Acceptance Criteria

| ID | Criterion |
|----|-----------|
| AC1 | `Scene2DModule` lives in `Cluiche/CluicheGameBaseline/Modules/` and is registered via `DIA_MODULE` |
| AC2 | On `DoStart`, reads the `stage_scene` alias from `PathStore`; if absent, logs an error and returns `StartResult::kFailed` |
| AC3 | On `DoStart`, calls `SceneLoader2D::Load()` with the resolved path; if load fails, logs an error and returns `StartResult::kFailed` |
| AC4 | Exposes read-only accessors: `GetCameraRegistry()`, `GetLightRegistry()`, `GetLayerTable()`, `GetEntityDomain()` |
| AC5 | On `DoStop`, calls `SceneLoader2D::Unload()` and clears all owned state |
| AC6 | `Scene2DTestStageModule` drops `LoadScene()`, all loader/registry/domain ownership, and its `PathStore` include; gains `ModuleRef<Scene2DModule>` |
| AC7 | `Scene2DTestStageModule` checkpoint validators (`ValidateCameras`, `ValidateLights`, `ValidateEntities`, `ValidateLayers`) read from `Scene2DModule` accessors via the `ModuleRef` |
| AC8 | `Scene2DTestStageModule` declares `Scene2DModule` as a dependency in its `.diaapp` entry |
| AC9 | `dia run cluichetest` — `Scene2DTestStage` passes all checkpoints |
| AC10 | `dia run googletest` — all `TestScene2D*` tests pass |

---

## Design

### Scene2DModule public interface

Scene2DModule is a pure loader/unloader. It owns only `LayerTable` and `SceneLoader2D`; cameras, lights, and entities live in their dedicated sibling modules.

```cpp
class Scene2DModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;

    explicit Scene2DModule(const Dia::Core::StringCRC& instanceId);

    const Dia::Scene2D::LayerTable& GetLayerTable() const;
    bool                            IsLoaded()      const;

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    Dia::ApplicationFlow::StopResult  DoStop()  override;

private:
    Dia::Scene2D::SceneLoader2D mSceneLoader;
    Dia::Scene2D::LayerTable    mLayerTable;
    bool                        mLoaded = false;

    // Sibling modules that own the registries/domain scene content loads into
    Dia::ApplicationFlow::ModuleRef<EntityModule>   mEntityRef{this};
    Dia::ApplicationFlow::ModuleRef<Camera2DModule> mCameraRef{this};
    Dia::ApplicationFlow::ModuleRef<Light2DModule>  mLightRef{this};
};
```

### Module ownership table

| Module | Owns | Drives |
|--------|------|--------|
| `EntityModule` | `Entity::Domain` | `Update(dt)`, `EndOfFrame()` |
| `Camera2DModule` | `CameraRegistry2D` | `UpdateAll(dt)`, viewport sync |
| `Light2DModule` | `LightRegistry2D` | (no-op update; future: animate lights) |
| `Scene2DModule` | `LayerTable`, `SceneLoader2D` | Load/unload scene content into the above three |

### Scene2DTestStageModule after refactor

```cpp
class Scene2DTestStageModule : public TestStageModuleBase
{
    // Checkpoint/validation logic only.
    Dia::ApplicationFlow::ModuleRef<Scene2DModule>   mSceneRef{this};   // IsLoaded(), GetLayerTable()
    Dia::ApplicationFlow::ModuleRef<EntityModule>    mEntityRef{this};  // entity validation
    Dia::ApplicationFlow::ModuleRef<Camera2DModule>  mCameraRef{this};  // camera validation
    Dia::ApplicationFlow::ModuleRef<Light2DModule>   mLightRef{this};   // light validation
};
```

### .diaapp wiring

`Scene2DModule` depends on `EntityModule`, `Camera2DModule`, and `Light2DModule` to guarantee they start first. `Camera2DModule` and `Light2DModule` are declared in the global manifest (`stages: all`). `EntityModule` is declared in the stage-local manifest for `Scene2DTestStage`.

---

## Binding Decisions

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | `kTypeId` is a `StringCRC`; all registry lookups use `StringCRC` keys |
| PD-002 | PU/Phase/Module architecture | `Scene2DModule` is a standard `Module`; no new architectural level introduced |
| PD-004 | No STL in public APIs | All accessors return engine type references; no `std::` in the public interface |
| PD-010 | `.diastage` is the stage metadata source | Module reads from `stage_scene` alias, which is registered from `.diastage` by `AssetServiceModule` — consistent with the established alias pattern |

---

## Files

| File | Action |
|------|--------|
| `Cluiche/CluicheGameBaseline/Modules/Scene2DModule.h` | New |
| `Cluiche/CluicheGameBaseline/Modules/Scene2DModule.cpp` | New |
| `Cluiche/CluicheGameBaseline/CluicheGameBaseline.vcxproj` | Add new files |
| `Cluiche/CluicheGameBaseline/CluicheGameBaseline.vcxproj.filters` | Add new files |
| `Cluiche/CluicheTest/Modules/TestStages/Scene2DTestStageModule.h` | Remove loader/registry fields; add `ModuleRef<Scene2DModule>` |
| `Cluiche/CluicheTest/Modules/TestStages/Scene2DTestStageModule.cpp` | Remove `LoadScene()`; update validators to use `ModuleRef` |
| `Cluiche/Assets/CluicheTest/Stages/Scene2DTestStage/misc/ApplicationFlow/scene2d_test_stage.diaapp` | Add `Scene2DModule` to `SimPU`; add as dependency of `Scene2DTestStageModule` |
| Deployed copies of above `.diaapp` | Update |
