---
schema: dia.module.v1
module_id: dia.softbody2d
name: DiaSoftBody2D
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaSoftBody2D
language: cpp
parent_module_id: dia.root

summary: >
  2D soft body simulation using Position-Based Dynamics — ropes, cloth, tearing, and two-way rigid body coupling.

intent: >
  Provides a PBD-based 2D soft body simulation. Manages a SoftBodyWorld containing Rope
  (1D particle chains) and Cloth (2D particle grids) objects. Constraint types include
  distance (rope), structural, shear, and bend (cloth), all using XPBD for timestep-independent
  stiffness. Supports tearable constraints via per-constraint max-stretch thresholds.
  Collision detection resolves particles against registered static DiaGeometry2D shapes
  (AARect, Circle, Line). Optional two-way coupling with DiaRigidBody2D enables anchor
  pinning (rope endpoints follow rigid bodies) and particle-vs-rigid-body collision with
  back-impulses. A visual debugger draws particles, constraints, and anchor links via
  DiaGraphics (isolated to Debug/ subdirectory).

responsibilities:
  - Own Particle, Rope, and Cloth data types
  - Provide SoftBodyWorld with fixed-timestep accumulator loop and PBD step sequence
  - Apply gravity and external forces to dynamic particles
  - Project XPBD distance constraints (structural, shear, bend) with configurable stiffness
  - Detect and resolve particle-vs-static-geometry collisions (AARect, Circle, Line)
  - Detect and resolve particle-vs-rigid-body collisions with back-impulses
  - Support anchor pinning of rope endpoints to rigid bodies
  - Support tearable constraints with per-constraint max-stretch thresholds
  - Expose particle positions for rendering
  - Emit structured debug logs to Physics DiaLogger channel (debug builds only)
  - Provide DiaSoftBodyVisualDebugger for debug drawing (DiaGraphics isolated to Debug/)

non_responsibilities:
  - Rigid body simulation — DiaRigidBody2D
  - 2D geometric primitives and intersection math — DiaGeometry2D
  - Pure math (vectors, matrices) — DiaMaths
  - Rendering or production drawing — DiaGraphics
  - Application scheduling — caller invokes SoftBodyWorld::Update()
  - Self-collision (cloth vs itself) — deferred
  - Soft body vs soft body collision — deferred
  - 3D soft bodies — future DiaSoftBody3D
  - Serialization of soft body state

dependent_modules:
  - dia.rigidbody2d
  - dia.geometry2d
  - dia.maths
  - dia.core

public_api:
  headers:
    - Dia/DiaSoftBody2D/Particle.h
    - Dia/DiaSoftBody2D/SoftBody.h
    - Dia/DiaSoftBody2D/Rope.h
    - Dia/DiaSoftBody2D/Cloth.h
    - Dia/DiaSoftBody2D/SoftBodyWorld.h
    - Dia/DiaSoftBody2D/Debug/DiaSoftBodyVisualDebugger.h
  namespaces:
    - Dia::SoftBody2D
  entry_points:
    - Particle
    - DeriveVelocity
    - Rope
    - RopeDef
    - Cloth
    - ClothDef
    - SoftBodyWorld
    - WorldDef
    - DiaSoftBodyVisualDebugger

dependencies:
  required:
    - dia.rigidbody2d
    - dia.geometry2d
    - dia.maths
    - dia.core
  forbidden:
    - dia.application
    - dia.window
    - dia.input
---
