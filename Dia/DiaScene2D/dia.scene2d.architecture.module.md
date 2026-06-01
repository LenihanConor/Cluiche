---
schema: dia.module.v1
module_id: dia.scene2d
name: Scene2D
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaScene2D
language: cpp
parent_module_id: dia.root

summary: >
  2D scene management library. Owns the .diascene file format (reflected struct),
  LayerTable (layer definitions + bitmask resolution), and SceneLoader2D
  (populates CameraRegistry2D, LightRegistry2D, and Entity::Domain from a scene file).

intent: >
  Provide a standalone, data-driven scene loading library. Code can populate camera/light
  registries and spawn entities from a .diascene JSON file without any PU/Module coupling.
  Programmatic registration via DiaCamera2D and DiaLighting2D works without any scene file.

responsibilities:
  - Own Scene2D reflected struct (world_bounds, layers, cameras, lights, entities)
  - Own LayerTable — layer definitions, sort order, parallax, bitmask resolution (name → bit index)
  - Own SceneLoader2D — reads .diascene, populates camera registry, light registry, spawns entities
  - Define layer schema (id, sort_order, parallax Vec2, sort_policy, enabled, optional render_technique ref)
  - Resolve affects_layers names to uint32 bitmask at load time
  - Provide a "default" layer fallback so entities always render somewhere
  - Validate scene constraints (exactly one active camera)

non_responsibilities:
  - Camera runtime behaviour (DiaCamera2D)
  - Light runtime management (DiaLighting2D)
  - Entity component systems / ECS (DiaEntity)
  - Blueprint asset loading from catalogue (DiaAssetRuntime — v2)
  - Rendering / draw calls (DiaBgfx)
  - Gameplay config: gravity, clear_colour, render techniques (.diastage config)
  - Scene identity / display name (catalogue asset ID / .diastage name)
  - 3D scenes (future DiaScene3D — independent peer)
  - PU/Module integration (application-side)
  - Sub-scene nesting / scene composition

dependent_modules: []

public_api:
  headers:
    - DiaScene2D/Scene2D.h
    - DiaScene2D/DiaScene2DSerializers.h
    - DiaScene2D/LayerTable.h
    - DiaScene2D/SceneLoadContext.h
    - DiaScene2D/SceneLoader2D.h
  namespaces:
    - Dia::Scene2D
  entry_points:
    - SceneLoader2D::Load
    - SceneLoader2D::Unload
    - LayerTable::Build
    - LayerTable::ResolveMask

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.geometry2d
    - dia.camera2d
    - dia.lighting2d
    - dia.entity
  forbidden:
    - dia.graphics
    - dia.bgfx
    - dia.applicationflow
    - dia.observation
---
