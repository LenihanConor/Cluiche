---
schema: dia.module.v1
module_id: dia.animation2dvisualdebugger
name: DiaAnimation2DVisualDebugger
owner_team: TBD
layer: debug
status: active
maturity: dev

path: Dia/DiaAnimation2DVisualDebugger
language: cpp
parent_module_id: dia.diavisualdebugger

summary: >
  Debug draw classes for DiaAnimation2D — clip cursor, blend weights, and spring chain physics.

intent: >
  Provides three IVisualDebugger implementations that expose animation state
  visually in the game world: clip playback progress, blend layer weights,
  and spring chain angular velocity coloured by activity level.

responsibilities:
  - AnimClipCursorDrawer: text labels per AnimClipPlayer showing ID and normalised time
  - AnimBlendWeightsDrawer: text labels per blend layer showing weight and priority
  - AnimSpringDrawer: coloured circles per spring node; gravity ray indicator

non_responsibilities:
  - Modifying animation state
  - Running on separate threads
  - Screen-space UI overlays (handled by DiaVisualDebuggerConsole)

dependent_modules:
  - dia.core
  - dia.maths
  - dia.rig2d
  - dia.animation2d
  - dia.graphics
  - dia.diavisualdebugger

public_api:
  headers:
    - DiaAnimation2DVisualDebugger/AnimClipCursorDrawer.h
    - DiaAnimation2DVisualDebugger/AnimBlendWeightsDrawer.h
    - DiaAnimation2DVisualDebugger/AnimSpringDrawer.h
  namespaces:
    - Dia::Animation2D
  entry_points:
    - AnimClipCursorDrawer
    - AnimBlendWeightsDrawer
    - AnimSpringDrawer

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.rig2d
    - dia.animation2d
    - dia.graphics
    - dia.diavisualdebugger
  forbidden: []
---
