---
schema: dia.module.v1
module_id: dia.graphics.frame
name: Frame
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaGraphics/Frame
language: cpp
parent_module_id: dia.graphics

summary: >
  Defines Frame APIs (DebugFrameData, DebugFrameDataBase, DebugFrameDataCircle2D, DebugFrameDataLine2D, DebugFrameDataVisitor, FrameData, UIFrameData) including: DebugFrameData, DebugFrameDataBase, DebugFrameDataCircle2D, DebugFrameDataLine2D, DebugFrameDataVisitor, FrameData, UIFrameData.

intent: >
  Provide reusable Frame building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: DebugFrameData, DebugFrameDataBase, DebugFrameDataCircle2D, DebugFrameDataLine2D, DebugFrameDataVisitor, FrameData, UIFrameData
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaGraphics/Frame/DebugFrameData.h
    - Dia/DiaGraphics/Frame/DebugFrameDataBase.h
    - Dia/DiaGraphics/Frame/DebugFrameDataCircle2D.h
    - Dia/DiaGraphics/Frame/DebugFrameDataLine2D.h
    - Dia/DiaGraphics/Frame/DebugFrameDataVisitor.h
    - Dia/DiaGraphics/Frame/FrameData.h
    - Dia/DiaGraphics/Frame/UIFrameData.h
  namespaces: []
  entry_points:
    - DebugFrameData
    - DebugFrameDataBase
    - DebugFrameDataCircle2D
    - DebugFrameDataLine2D
    - DebugFrameDataVisitor
    - FrameData
    - UIFrameData

dependencies:
  required:
    - dia.core.containers.arrays
    - dia.core.core
    - dia.core.type
    - dia.graphics.misc
    - dia.maths.vector
    - dia.ui
  forbidden: []
---
