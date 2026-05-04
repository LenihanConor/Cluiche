---
schema: dia.module.v1
module_id: dia.ik2dvisualdebugger
name: DiaIK2DVisualDebugger
owner_team: TBD
layer: debug
status: active
maturity: dev

path: Dia/DiaIK2DVisualDebugger
language: cpp
parent_module_id: dia.ik2d

summary: >
  Stack of four focused IVisualDebugger draw classes for visualising IK chain state.

intent: >
  Provides independent, toggleable debug layers for DiaIK2D: chain bone lines,
  chain joint circles, bone direction arrows, and chain reach circles. All data
  is read from an IKSolver held by const reference.

responsibilities:
  - Draw cyan lines for each bone in each registered IK chain (IKChainBonesDrawer)
  - Draw colour-coded circles at each joint in IK chains (IKChainJointsDrawer)
  - Draw bone direction rays for IK chain bones (IKChainArrowsDrawer)
  - Draw reach-radius circles at chain root positions (IKReachCirclesDrawer)

non_responsibilities:
  - Storing or modifying IKSolver state
  - Drawing FK (non-IK) bones — that is DiaRig2DVisualDebugger's responsibility
  - Showing IK target positions — those are known only by game code

dependent_modules:
  - dia.ik2d
  - dia.rig2d
  - dia.visualdebugger
  - dia.graphics
  - dia.maths
  - dia.core

public_api:
  headers:
    - Dia/DiaIK2DVisualDebugger/IKChainBonesDrawer.h
    - Dia/DiaIK2DVisualDebugger/IKChainJointsDrawer.h
    - Dia/DiaIK2DVisualDebugger/IKChainArrowsDrawer.h
    - Dia/DiaIK2DVisualDebugger/IKReachCirclesDrawer.h
  namespaces:
    - Dia::IK2D
  entry_points:
    - IKChainBonesDrawer
    - IKChainJointsDrawer
    - IKChainArrowsDrawer
    - IKReachCirclesDrawer

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.rig2d
    - dia.ik2d
    - dia.graphics
    - dia.visualdebugger
  forbidden:
    - dia.application  # debug draw is not lifecycle-aware
---
