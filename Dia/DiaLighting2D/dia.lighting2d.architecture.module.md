---
schema: dia.module.v1
module_id: dia.lighting2d
name: Lighting2D
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaLighting2D
language: cpp
parent_module_id: dia.root

summary: >
  2D point light value type, named registry with layer-mask query. v1 ships data model only;
  visual effect (normal-map lighting, ambient/diffuse) requires future DiaBgfx shader work.

intent: >
  Provide a standalone, PU-agnostic 2D lighting library. Code can register and query lights
  without any scene file or DiaScene2D dependency. DiaScene2D's SceneLoader is an optional
  data-driven path that populates the same registry from a .diascene file.

responsibilities:
  - Own PointLight2D value type (position, radius, colour RGBA, intensity, layerMask, enabled)
  - Provide LightRegistry2D — named light storage with add/remove/get/query
  - Provide layer-mask query (bit index → filtered list of PointLight2D pointers)
  - Ship Testing/LightBuilder — fluent builder for constructing LightRegistry2D in tests

non_responsibilities:
  - Rendering lights visually (DiaBgfx shader work — future)
  - Normal-map generation or asset pipeline (DiaAssetPipeline)
  - Light behaviours (flicker, pulse — v2 backlog)
  - PU/Module integration (application-side)
  - Scene file loading (DiaScene2D)
  - Layer-name-to-bitmask resolution (caller responsibility — DiaScene2D LayerTable)
  - 3D lights (future DiaLighting3D — independent peer)

dependent_modules: []

public_api:
  headers:
    - DiaLighting2D/PointLight2D.h
    - DiaLighting2D/Registry/LightRegistry2D.h
    - DiaLighting2D/Testing/LightBuilder.h
  namespaces:
    - Dia::Lighting2D
  entry_points:
    - LightRegistry2D::Register
    - LightRegistry2D::Get
    - LightRegistry2D::GetLightsForLayer

dependencies:
  required:
    - dia.core
    - dia.maths
  forbidden:
    - dia.graphics
    - dia.entity
    - dia.scene2d
    - dia.applicationflow
    - dia.bgfx
    - dia.geometry2d
---
