---
schema: dia.module.v1
module_id: dia.graphics.interface
name: Interface
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaGraphics/Interface
language: cpp
parent_module_id: dia.graphics

summary: >
  Defines Interface APIs (ICanvas, IDrawable, IRenderTarget, IView) including: AARect2D, ICanvas, IDrawable, IRenderTarget, IView, Settings, Vector2D.

intent: >
  Provide reusable Interface building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: AARect2D, ICanvas, IDrawable, IRenderTarget, IView, Settings, Vector2D
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaGraphics/Interface/ICanvas.h
    - Dia/DiaGraphics/Interface/IDrawable.h
    - Dia/DiaGraphics/Interface/IRenderTarget.h
    - Dia/DiaGraphics/Interface/IView.h
  namespaces: []
  entry_points:
    - AARect2D
    - ICanvas
    - IDrawable
    - IRenderTarget
    - IView
    - Settings
    - Vector2D

dependencies:
  required:
    - dia.core.core
    - dia.graphics.frame
    - dia.graphics.misc
    - dia.maths.shape.x2d
    - dia.maths.vector
  forbidden: []
---
