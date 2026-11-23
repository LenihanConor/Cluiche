---
schema: dia.module.v1
module_id: dia.scene2dvisualdebugger
name: DiaScene2DVisualDebugger
layer: 4.1-Visual
path: Dia/DiaScene2DVisualDebugger
parent: diavisualdebugger
description: Visual debug drawers for DiaScene2D — cameras, lights, and layer bands.
dependent_modules:
  - dia.camera2d
  - dia.core.debugdraw
  - dia.debug.visualdebugger
  - dia.geometry2d
  - dia.geometry2dvisualdebugger
  - dia.graphics.misc
  - dia.lighting2d
  - dia.scene2d
public_api:
  headers:
    - DiaScene2DVisualDebugger/SceneOverviewDrawer.h
  namespaces:
    - Dia::Scene2DVisualDebugger
responsibilities:
  - Draw camera positions and light positions/radii for a Scene2D
  - Draw layer band visualization
non_responsibilities:
  - Entity rendering (belongs in a future DiaEntityVisualDebugger)
  - Scene simulation or state mutation
---
