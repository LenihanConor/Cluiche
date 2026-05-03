---
schema: dia.module.v1
module_id: dia.animation2d
name: DiaAnimation2D
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaAnimation2D
language: cpp
parent_module_id: dia.root

summary: >
  Animation playback and blending system: damped spring chains, keyframe clip player, pose blend stack, and animation evaluator pipeline.

intent: >
  DiaAnimation2D sits at the top of the animation stack, consuming DiaRig2D skeleton/pose
  data and optionally driving DiaIK2D targets. It provides four core capabilities: secondary
  motion via damped spring chains, authored clip playback from keyframe data, layered pose
  composition via a blend stack, and a full-pipeline orchestrator (AnimationEvaluator) that
  owns intermediate poses and runs FK->clip->spring->blend in the correct order. Game code
  decides what to play and when; DiaAnimation2D provides the machinery.

responsibilities:
  - SpringChainDef, SpringNodeDef, SpringChain: angular spring-damper secondary motion
  - SpringParamsFromFrequency: artist-friendly Hz/damping-ratio to k/d conversion
  - AnimClipDef, KeyframeTrack, Keyframe, AnimClip: keyframe data with per-bone tracks
  - AnimClipPlayer: one-shot/looping playback controller with speed control
  - AnimClipLoader: JSON loader for custom format and Spine2D v4 animation sections
  - BoneMask, PoseLayer, PoseBlendStack: priority-ordered cascading lerp blend stack
  - AnimationEvaluator: pipeline orchestrator, owns intermediate poses
  - Test utilities under DiaAnimation2D/Testing/ (header-only, consumer opt-in)

non_responsibilities:
  - Skeleton definition, FK, pose representation (DiaRig2D)
  - Inverse kinematics solvers (DiaIK2D)
  - Animation state machines (game code uses DiaStateMachine)
  - Physics-driven secondary motion (DiaSoftBody2D)
  - Mesh deformation / skinned mesh rendering (DiaGraphics)
  - Root motion extraction (deferred)
  - 3D animation (future DiaAnimation3D)
  - Additive blending (deferred)
  - Animation events/notifies (deferred)
  - Procedural locomotion oscillator (deferred to Wave 3)

dependent_modules:
  - dia.rig2d
  - dia.maths
  - dia.core
  - dia.logger

public_api:
  headers:
    - DiaAnimation2D/SpringNodeDef.h
    - DiaAnimation2D/SpringChainDef.h
    - DiaAnimation2D/SpringChain.h
    - DiaAnimation2D/SpringParamUtils.h
    - DiaAnimation2D/Keyframe.h
    - DiaAnimation2D/KeyframeTrack.h
    - DiaAnimation2D/AnimClipDef.h
    - DiaAnimation2D/AnimClip.h
    - DiaAnimation2D/AnimClipPlayer.h
    - DiaAnimation2D/AnimClipLoader.h
    - DiaAnimation2D/BoneMask.h
    - DiaAnimation2D/PoseLayer.h
    - DiaAnimation2D/PoseBlendStack.h
    - DiaAnimation2D/AnimationEvaluator.h
  namespaces:
    - Dia::Animation2D
    - Dia::Animation2D::Testing
  entry_points:
    - SpringChain
    - AnimClip
    - AnimClipPlayer
    - AnimClipLoader
    - PoseBlendStack
    - AnimationEvaluator
    - SpringParamsFromFrequency

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
    - dia.ik2d
    - dia.rigidbody2d
    - dia.softbody2d
---
