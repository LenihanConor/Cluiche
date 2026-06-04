---
schema: dia.module.v1
module_id: dia.debugdraw
name: DiaDebugDraw
owner_team: TBD
layer: foundation/services
status: active
maturity: dev

path: Dia/DiaDebugDraw
language: cpp

summary: >
  Abstract debug draw registration, layer management, and primitive dispatch.
  Extracted from DiaVisualDebugger so domain modules (Physics, Animation, Entity)
  can register debug layers without depending on the Visual rendering domain.

intent: >
  Provide the debug layer registry and abstract interfaces without requiring
  concrete rendering knowledge.

responsibilities:
  - Debug layer registration and priority dispatch (DebugLayerManager)
  - Abstract draw interfaces (IVisualDebugger, IObjectRenderer, IFixedPrimitiveBuffer)
  - Fixed-topology object registry and primitive buffering
  - Layer name constants and colour palette constants
  - Optional DiaAPI command registration for layer toggle/scale

non_responsibilities:
  - Concrete draw implementations (DiaVisualDebugger)
  - Coordinate-space drawers (DiaVisualDebugger/Coord2D)
  - Spatial structure renderers (DiaVisualDebugger/Renderers)

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.graphics
    - dia.api
  optional:
    - dia.debugserver
    - dia.editor
---
