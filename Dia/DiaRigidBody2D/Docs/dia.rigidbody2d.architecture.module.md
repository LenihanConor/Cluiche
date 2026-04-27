---
schema: dia.module.v1
module_id: dia.rigidbody2d
name: DiaRigidBody2D
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaRigidBody2D
language: cpp
parent_module_id: dia.root

summary: >
  2D rigid body physics simulation — bodies, forces, collision detection/response, constraints, and spatial queries.

intent: >
  Provides a deterministic fixed-timestep 2D physics simulation. Manages two body pools
  (PointBody2D for translation-only, RigidBody2D for full rotation + constraints).
  Collision detection is broad-phase via injected ISpatialStructure + narrow-phase via
  DiaGeometry2D IntersectionTests. Collision response uses impulse-based resolution with
  Baumgarte positional correction. Constraints (pin, distance, spring, hinge) are solved
  via sequential impulses with warm-starting. Collision events are emitted via DiaCore
  ObserverSubject for enter/stay/exit notifications.

responsibilities:
  - Own PointBody2D (translational) and RigidBody2D (translational + rotational + constraints) data types
  - Provide PhysicsWorld with fixed-timestep accumulator loop and StepOnce execution
  - Integrate forces and velocities (semi-implicit Euler)
  - Perform broad-phase collision detection via injected ISpatialStructure
  - Perform narrow-phase collision detection via DiaGeometry2D IntersectionTests
  - Resolve collisions via impulse-based response with Coulomb friction and Baumgarte correction
  - Emit collision enter/stay/exit events via ObserverSubject
  - Provide spatial queries (Raycast, QueryRegion, QueryCircle)
  - Implement body sleeping with dual velocity thresholds
  - Enforce collision layers and masks with bilateral filtering
  - Solve constraints/joints (PinJoint, DistanceConstraint, SpringConstraint, HingeJoint)

non_responsibilities:
  - 2D geometric primitives and intersection math — DiaGeometry2D
  - Pure math (vectors, matrices) — DiaMaths
  - Rendering or debug drawing — DiaGraphics
  - Scene management — future DiaScene
  - 3D physics — future DiaRigidBody3D
  - Serialization of physics state

dependent_modules:
  - dia.geometry2d
  - dia.maths
  - dia.core

public_api:
  headers: []
  namespaces:
    - Dia::RigidBody2D
  entry_points:
    - PointBody2D
    - RigidBody2D
    - PhysicsWorld
    - IConstraint
    - PinJoint
    - DistanceConstraint
    - SpringConstraint
    - HingeJoint

dependencies:
  required:
    - dia.geometry2d
    - dia.maths
    - dia.core
  forbidden:
    - dia.graphics
    - dia.application
    - dia.window
    - dia.input
---
