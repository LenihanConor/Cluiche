---
schema: dia.module.v1
module_id: dia.rig2d
name: DiaRig2D
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaRig2D
language: cpp
parent_module_id: dia.root

summary: >
  2D skeletal rig system — skeleton definitions, forward kinematics, pose representation, and pose blending.

intent: >
  Provides the data layer for bone-based 2D character animation. Manages Skeleton objects
  (flat arrays of Bone structs in topological order) with forward kinematics that propagate
  local bone transforms to world space in a single O(n) pass. Pose objects represent mutable
  snapshots of bone transforms with linear blending (shortest-arc rotation interpolation).
  Supports data-driven skeleton construction via JSON, and integrates with the engine's
  component system via SkeletonComponent. Pure data + transforms — no physics coupling.

responsibilities:
  - Define Bone data type with local transform, parent index, StringCRC name, and precomputed length
  - Maintain Skeleton as a flat array of bones in topological order (parent index < child index)
  - Compute forward kinematics propagating local bone transforms to world-space transforms
  - Define Pose as a mutable snapshot of bone local transforms with bind pose initialization
  - Blend poses via per-bone linear interpolation with shortest-arc rotation lerp
  - Load and save skeleton definitions from JSON using DiaCore/Json
  - Provide SkeletonComponent (IComponent) for entity integration
  - Emit structured debug logs to Rig2D DiaLogger channel
  - Provide test utilities (skeleton builders, pose comparison) in Testing/ subdirectory

non_responsibilities:
  - Animation clips, keyframe interpolation, playback timing, blend trees — future DiaAnimation2D
  - Inverse kinematics solvers — future DiaIK2D
  - Physics simulation, ragdoll — DiaRigidBody2D
  - Mesh deformation, skinned mesh rendering — DiaGraphics
  - 3D skeletons — future DiaRig3D
  - Application scheduling — caller invokes rig APIs directly

dependent_modules: []

public_api:
  headers:
    - Dia/DiaRig2D/Bone.h
    - Dia/DiaRig2D/BoneTransform.h
    - Dia/DiaRig2D/Skeleton.h
    - Dia/DiaRig2D/Pose.h
    - Dia/DiaRig2D/BlendPoses.h
    - Dia/DiaRig2D/SkeletonJson.h
    - Dia/DiaRig2D/SkeletonComponent.h
  namespaces:
    - Dia::Rig2D
  entry_points:
    - Bone
    - BoneTransform
    - Skeleton
    - SkeletonDef
    - Pose
    - BlendPoses
    - SkeletonJson
    - SkeletonComponent

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.logger
  forbidden:
    - dia.graphics
    - dia.application
    - dia.window
    - dia.input
---
