---
schema: dia.module.v1
module_id: dia.geometry2dvisualdebugger
name: DiaGeometry2DVisualDebugger
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaGeometry2DVisualDebugger
language: cpp
parent_module_id: dia.diavisualdebugger

summary: >
  Debug draw classes for DiaGeometry2D shapes and spatial structures.

intent: >
  Provides IVisualDebugger implementations that visualise DiaGeometry2D shapes
  submitted per-frame (ShapeDrawer) and the internal cell/node structure of
  spatial acceleration structures (SpatialGridDrawer, QuadtreeDrawer,
  BVHDrawer, HexGridDrawer). Classes are registered with DebugLayerManager.

responsibilities:
  - ShapeDrawer: accept and draw Circle, AARect, Line, Ray, Triangle, ConvexPolygon
  - SpatialGridDrawer: draw SpatialGrid cell rects in kInactive colour
  - QuadtreeDrawer: draw Quadtree nodes (leaf=kActive, internal=kInactive)
  - BVHDrawer: draw BVH nodes coloured by depth if IsBuilt()
  - HexGridDrawer: draw HexGrid cell edges (6 lines per hex, kInactive)

non_responsibilities:
  - Does not own or simulate any geometry
  - Does not render to screen directly (FrameData is the intermediary)

dependent_modules:
  - dia.diavisualdebugger
  - dia.diageometry2d
  - dia.diagraphics
  - dia.diamaths
  - dia.diacore

public_api:
  headers:
    - DiaGeometry2DVisualDebugger/ShapeDrawer.h
    - DiaGeometry2DVisualDebugger/SpatialGridDrawer.h
    - DiaGeometry2DVisualDebugger/QuadtreeDrawer.h
    - DiaGeometry2DVisualDebugger/BVHDrawer.h
    - DiaGeometry2DVisualDebugger/HexGridDrawer.h
  namespaces:
    - Dia::Geometry2DVisualDebugger
  entry_points:
    - ShapeDrawer
    - SpatialGridDrawer
    - QuadtreeDrawer
    - BVHDrawer
    - HexGridDrawer

dependencies:
  required:
    - dia.diavisualdebugger
    - dia.diageometry2d
    - dia.diagraphics
    - dia.diamaths
    - dia.diacore
  forbidden: []
---
