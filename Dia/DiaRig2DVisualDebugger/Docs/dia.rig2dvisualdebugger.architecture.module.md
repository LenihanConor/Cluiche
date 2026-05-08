---
schema: dia.module.v1
module_id: dia.rig2dvisualdebugger
name: DiaRig2DVisualDebugger
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaRig2DVisualDebugger
language: cpp
parent_module_id: dia.root

summary: >
  Debug visualization bridge for DiaRig2D — draws bones, joints, and hierarchy via DiaGraphics FrameData.

intent: >
  Provides a lightweight debug renderer that takes pre-computed world-space bone transforms
  from DiaRig2D and draws them as lines (bones), circles (joints), and optional name labels
  via the DiaGraphics FrameData system. Exists as a separate project to prevent DiaRig2D
  from depending on DiaGraphics. On/off toggle for runtime control.

responsibilities:
  - Draw bone connections as lines between parent and child joint positions
  - Draw joint positions as circles (green for root, yellow for others)
  - Provide on/off toggle for entire debug visualization
  - Provide optional bone name label toggle

non_responsibilities:
  - Running forward kinematics — caller provides pre-computed world transforms
  - Skeleton or pose management — DiaRig2D concern
  - Actual rendering — DiaGraphics/DiaSFML handles that via FrameData visitors

dependent_modules: []

public_api:
  headers:
    - Dia/DiaRig2DVisualDebugger/VisualDebugger.h
  namespaces:
    - Dia::Rig2D
  entry_points:
    - VisualDebugger

dependencies:
  required:
    - dia.rig2d
    - dia.graphics
    - dia.maths
    - dia.core
  forbidden:
    - dia.application
    - dia.window
    - dia.input
---
