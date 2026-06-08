---
schema: dia.module.v1
module_id: dia.lighting3d
name: Lighting3D
owner_team: TBD
layer: domain/visual/core
status: active
maturity: dev

path: Dia/DiaLighting3D
language: cpp
parent_module_id: dia.root

summary: >
  3D light value types (Point, Directional, Spot, Ambient), LightRegistry3D,
  ILightBehaviour3D interface, LightBehaviourRegistry3D factory, and 3 engine
  behaviours (Flicker, Pulse, ColorCycle). Registry is fully usable without
  any scene file.

intent: >
  Provide a standalone, PU-agnostic 3D lighting library. Code can register and query lights
  without any scene file or DiaScene3D dependency. DiaScene3D's SceneLoader is an optional
  data-driven path that populates the same registry from a .diascene file. Light behaviours
  are registered as named factories — implementations are decoupled from the registry.

responsibilities:
  - Own four light value types: PointLight3D, DirectionalLight3D, SpotLight3D, AmbientLight3D
  - Provide LightRegistry3D — named light storage with add/remove/get/query by type
  - Provide type-specific query (light type → filtered list of pointers)
  - Own ILightBehaviour3D interface for light simulation (Flicker, Pulse, ColorCycle)
  - Provide LightBehaviourRegistry3D — self-registering factory for behaviours
  - Ship 3 engine behaviours: Flicker, Pulse, ColorCycle (pre-registered on module load)
  - Ship Testing/LightBuilder — fluent builder for constructing LightRegistry3D in tests

non_responsibilities:
  - Rendering lights visually (DiaBgfx shader work — future)
  - Normal-map generation or asset pipeline (DiaAssetPipeline)
  - PU/Module integration (application-side)
  - Scene file loading (DiaScene3D)
  - Layer-name-to-bitmask resolution (caller responsibility — DiaScene3D LayerTable)
  - 2D lights (separate DiaLighting2D — independent peer)
  - Direct LightRegistry3D mutation from within behaviour code (behaviours observe only, registry owns mutation)

dependent_modules: []

public_api:
  headers:
    - DiaLighting3D/PointLight3D.h
    - DiaLighting3D/DirectionalLight3D.h
    - DiaLighting3D/SpotLight3D.h
    - DiaLighting3D/AmbientLight3D.h
    - DiaLighting3D/Registry/LightRegistry3D.h
    - DiaLighting3D/Behaviours/ILightBehaviour3D.h
    - DiaLighting3D/Behaviours/LightBehaviourRegistry3D.h
    - DiaLighting3D/Testing/LightBuilder.h
  namespaces:
    - Dia::Lighting3D
  entry_points:
    - LightRegistry3D::Register
    - LightRegistry3D::Get
    - LightRegistry3D::GetLightsOfType
    - LightBehaviourRegistry3D::Register
    - LightBehaviourRegistry3D::Create

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.observation
  forbidden:
    - dia.graphics
    - dia.graphics3d
    - dia.entity
    - dia.scene3d
    - dia.applicationflow
    - dia.bgfx
    - dia.geometry3d
---
