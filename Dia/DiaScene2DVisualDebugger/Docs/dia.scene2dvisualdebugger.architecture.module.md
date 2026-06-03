---
schema: dia.module.v1
id: scene2dvisualdebugger
display_name: DiaScene2DVisualDebugger
layer: domain/visual/tools
parent: diavisualdebugger
description: Visual debug drawers for DiaScene2D — cameras, lights, and layer bands.
dependent_modules:
  - diavisualdebugger
  - diageometry2dvisualdebugger
  - diascene2d
  - diacamera2d
  - dialighting2d
  - diagraphics
  - diamaths
  - diacore
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
