---
schema: dia.module.v1
module_id: dia.camera2d
name: Camera2D
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCamera2D
language: cpp
parent_module_id: dia.root

summary: >
  2D camera value type, named registry, composable behaviour system with self-registering factory,
  and 8 engine-provided behaviours. Migrated from DiaGraphics.

intent: >
  Provide a standalone, PU-agnostic 2D camera library. Code can register and tick cameras
  without any scene file or DiaScene2D dependency. DiaScene2D's SceneLoader is an optional
  data-driven path that populates the same registry from a .diascene file.

responsibilities:
  - Own Camera2D value type (position, zoom, rotation)
  - Own ViewportTransform (screen↔world coordinate conversion)
  - Provide CameraRegistry2D — named camera storage with active-camera management
  - Define ICameraBehaviour interface (Update(Camera2D&, float dt))
  - Provide CameraBehaviourRegistry — self-registering factory for behaviour creation
  - Provide CameraRegistry2D::UpdateAll(float dt) — tick all cameras' attached behaviours
  - Ship 8 engine behaviours: Follow, SmoothDamp, Deadzone, BoundsClamp, ScreenShake, ZoomToFit, Pan, Zoom

non_responsibilities:
  - PU/Module integration (application-side CameraModule)
  - Input handling or input→behaviour wiring (application-side)
  - Scene file loading (DiaScene2D)
  - 3D cameras (future DiaCamera3D)
  - Rendering (DiaBgfx consumes Camera2D via FrameData)
  - Window size management (application-side)

dependent_modules: []

public_api:
  headers:
    - DiaCamera2D/Camera2D.h
    - DiaCamera2D/ViewportTransform.h
    - DiaCamera2D/Registry/CameraRegistry2D.h
    - DiaCamera2D/Behaviour/ICameraBehaviour.h
    - DiaCamera2D/Behaviour/CameraBehaviourRegistry.h
    - DiaCamera2D/Behaviour/FollowBehaviour.h
    - DiaCamera2D/Behaviour/SmoothDampBehaviour.h
    - DiaCamera2D/Behaviour/DeadzoneBehaviour.h
    - DiaCamera2D/Behaviour/BoundsClampBehaviour.h
    - DiaCamera2D/Behaviour/ScreenShakeBehaviour.h
    - DiaCamera2D/Behaviour/ZoomToFitBehaviour.h
    - DiaCamera2D/Behaviour/PanBehaviour.h
    - DiaCamera2D/Behaviour/ZoomBehaviour.h
  namespaces:
    - Dia::Camera2D
  entry_points:
    - CameraRegistry2D::Register
    - CameraRegistry2D::UpdateAll
    - CameraRegistry2D::GetActive
    - CameraBehaviourRegistry::Get

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.geometry2d
  forbidden:
    - dia.graphics
    - dia.entity
    - dia.scene2d
    - dia.applicationflow
    - dia.bgfx
---
