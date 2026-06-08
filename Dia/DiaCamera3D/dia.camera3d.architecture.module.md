---
schema: dia.module.v1
module_id: dia.camera3d
name: Camera3D
owner_team: TBD
layer: domain/visual/core
status: active
maturity: dev

path: Dia/DiaCamera3D
language: cpp
parent_module_id: dia.root

summary: >
  3D camera value type (Camera3D), ViewportTransform3D (view+proj matrices, world↔screen, frustum), CameraRegistry3D (named registry with composable per-camera behaviours), ICameraBehaviour3D interface, CameraBehaviourRegistry3D factory, and 6 engine behaviours (Follow3D, SmoothDamp3D, BoundsClamp3D, ScreenShake3D, Orbit, Flythrough).

intent: >
  Provide a standalone, PU-agnostic 3D camera library. Code can register and tick cameras
  without any scene file or DiaScene3D dependency. DiaScene3D's SceneLoader is an optional
  data-driven path that populates the same registry from a .diascene file. Camera behaviours are registered as named factories — implementations are decoupled from the registry.

responsibilities:
  - Own Camera3D value type (position, forward, up, FOV, near/far planes)
  - Own ViewportTransform3D — view and projection matrix management, world↔screen coordinate conversion, frustum accessors
  - Provide CameraRegistry3D — named camera storage with active-camera management
  - Define ICameraBehaviour3D interface (Update(Camera3D&, float dt))
  - Provide CameraBehaviourRegistry3D — self-registering factory for behaviour creation
  - Provide CameraRegistry3D::UpdateAll(float dt) — tick all cameras' attached behaviours
  - Ship 6 engine behaviours: Follow3D, SmoothDamp3D, BoundsClamp3D, ScreenShake3D, Orbit, Flythrough
  - Provide CameraBuilder3D test helper
  - Provide CameraRegistryHealth3D for observability

non_responsibilities:
  - PU/Module integration (application-side Camera3DModule)
  - Input handling or input→behaviour wiring (application-side)
  - Scene file loading (DiaScene3D)
  - 2D cameras (DiaCamera2D)
  - Rendering (DiaBgfx consumes Camera3D via FrameData)
  - Window size management (application-side)
  - Frustum culling (rendering-side responsibility)
  - Euler angle helpers

dependent_modules: []

public_api:
  headers:
    - DiaCamera3D/Camera3D.h
    - DiaCamera3D/ViewportTransform3D.h
    - DiaCamera3D/Registry/CameraRegistry3D.h
    - DiaCamera3D/Behaviour/ICameraBehaviour3D.h
    - DiaCamera3D/Behaviour/CameraBehaviourRegistry3D.h
    - DiaCamera3D/Behaviour/Follow3DBehaviour.h
    - DiaCamera3D/Behaviour/SmoothDamp3DBehaviour.h
    - DiaCamera3D/Behaviour/BoundsClamp3DBehaviour.h
    - DiaCamera3D/Behaviour/ScreenShake3DBehaviour.h
    - DiaCamera3D/Behaviour/Orbit3DBehaviour.h
    - DiaCamera3D/Behaviour/Flythrough3DBehaviour.h
    - DiaCamera3D/Testing/CameraBuilder3D.h
    - DiaCamera3D/Health/CameraRegistryHealth3D.h
  namespaces:
    - Dia::Camera3D
  entry_points:
    - CameraRegistry3D::Register
    - CameraRegistry3D::UpdateAll
    - CameraRegistry3D::GetActive
    - CameraBehaviourRegistry3D::Get

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.geometry3d
    - dia.observation
  forbidden:
    - dia.graphics
    - dia.graphics3d
    - dia.entity
    - dia.scene3d
    - dia.applicationflow
    - dia.bgfx
    - dia.input
---
