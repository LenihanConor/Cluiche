---
schema: dia.module.v1
module_id: dia.ik2d
name: DiaIK2D
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaIK2D
language: cpp
parent_module_id: dia.root

summary: >
  2D inverse kinematics post-process solvers: analytic two-bone, iterative FABRIK, and look-at constraint.

intent: >
  DiaIK2D operates as a post-process pass on top of DiaRig2D. It reads bone transforms from
  a Skeleton, runs one or more IK solvers to position an end effector at a target, and writes
  the modified bone rotations back to the Pose. DiaRig2D owns all joint data; DiaIK2D is
  stateless between frames.

responsibilities:
  - Define IKChainDef, JointLimitDef, PoleVector data types
  - Provide IKSolver: wraps Skeleton + Pose, manages named chains, dispatches solve calls
  - Implement analytic two-bone solver (law of cosines, pole vector, reach weight)
  - Implement iterative FABRIK solver (N-joint chains, joint limit projection, convergence check)
  - Implement look-at constraint (single-bone rotation toward world-space target)
  - Trigger FK propagation on modified bone range after each solve
  - Apply per-chain reach weight blending (0=pure FK, 1=full IK)
  - Emit structured debug warnings to Rig2D DiaLogger channel
  - Provide test utilities under DiaIK2D/Testing/ (header-only, consumer opt-in)

non_responsibilities:
  - Skeleton definition and FK propagation (DiaRig2D)
  - Animation clips, blend trees, playback (future DiaAnimation2D)
  - Physics-driven IK (game code bridges DiaRig2D + DiaRigidBody2D)
  - 3D IK (future DiaIK3D)
  - Visual debug rendering of chains and targets (future DiaIK2DVisualDebugger)
  - IKComponent entity integration (deferred to v2)

dependent_modules:
  - dia.rig2d
  - dia.maths
  - dia.core
  - dia.logger

public_api:
  headers:
    - DiaIK2D/IKChainDef.h
    - DiaIK2D/IKSolver.h
  namespaces:
    - Dia::IK2D
  entry_points:
    - IKSolver
    - IKChainDef
    - JointLimitDef
    - PoleVector

dependencies:
  required:
    - dia.rig2d
    - dia.maths
    - dia.core
    - dia.logger
  forbidden:
    - dia.graphics
    - dia.application
    - dia.statemachine
---
