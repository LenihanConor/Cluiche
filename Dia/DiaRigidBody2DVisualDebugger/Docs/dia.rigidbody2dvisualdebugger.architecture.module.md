---
schema: dia.module.v1
module_id: dia.rigidbody2dvisualdebugger
name: DiaRigidBody2DVisualDebugger
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaRigidBody2DVisualDebugger
language: cpp
parent_module_id: dia.rigidbody2d

summary: >
  Read-only debug visualization for rigid body physics state — shapes, velocity arrows,
  contact normals, and constraint lines written into FrameData as debug primitives.

intent: >
  Bridges DiaRigidBody2D (pure simulation, no graphics dependency) and DiaGraphics
  (rendering primitives) without polluting either. Game code that does not need debug
  drawing links only DiaRigidBody2D and incurs no graphics overhead.

responsibilities:
  - Draw collision shapes (circle, polygon edges) coloured by body type and sleep state
  - Draw velocity arrows for awake dynamic and kinematic bodies (capped at 10 visual units)
  - Draw contact normal lines from the most recent simulation step
  - Draw constraint anchor-to-anchor lines via IConstraint::GetWorldAnchorA/B
  - Provide an enable/disable toggle that gates all draw output

non_responsibilities:
  - Modifying physics simulation state — strictly read-only
  - Rendering — writes primitives to FrameData; actual rendering is DiaGraphics' concern
  - Text/label rendering — deferred until DiaGraphics adds a text primitive
  - Application scheduling — game code calls Draw() at the appropriate point

dependent_modules:
  - dia.core
  - dia.maths
  - dia.geometry2d
  - dia.rigidbody2d
  - dia.graphics

public_api:
  headers:
    - DiaRigidBody2DVisualDebugger/DiaRigidBodyVisualDebugger.h
  namespaces:
    - Dia::RigidBody2D
  entry_points:
    - DiaRigidBodyVisualDebugger

dependencies:
  required:
    - dia.rigidbody2d
    - dia.graphics
    - dia.geometry2d
    - dia.maths
    - dia.core
  forbidden:
    - dia.application
    - dia.logger
---
