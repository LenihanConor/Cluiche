---
schema: dia.module.v1
module_id: dia.sfml
name: SFML
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaSFML
language: cpp
parent_module_id: dia.root

summary: >
  Defines SFML APIs (Conversion, DebugFrameRendererVisitor, InputSource, RenderWindow, RenderWindowFactory) including: Color, DebugFrameData, DebugFrameDataCircle2D, DebugFrameDataLine2D, DebugFrameRendererVisitor, Event, InputSource, RenderTarget, RenderTexture, RenderWindow, RenderWindowFactory, RGBA.

intent: >
  Provide reusable SFML building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: Color, DebugFrameData, DebugFrameDataCircle2D, DebugFrameDataLine2D, DebugFrameRendererVisitor, Event, InputSource, RenderTarget, RenderTexture, RenderWindow, RenderWindowFactory, RGBA
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaSFML/Conversion.h
    - Dia/DiaSFML/DebugFrameRendererVisitor.h
    - Dia/DiaSFML/InputSource.h
    - Dia/DiaSFML/RenderWindow.h
    - Dia/DiaSFML/RenderWindowFactory.h
  namespaces: []
  entry_points:
    - Color
    - DebugFrameData
    - DebugFrameDataCircle2D
    - DebugFrameDataLine2D
    - DebugFrameRendererVisitor
    - Event
    - InputSource
    - RenderTarget
    - RenderTexture
    - RenderWindow
    - RenderWindowFactory
    - RGBA

dependencies:
  required:
    - dia.core.containers.bitflag
    - dia.core.filepath
    - dia.core.memory
    - dia.core.strings
    - dia.graphics.frame
    - dia.graphics.interface
    - dia.graphics.misc
    - dia.input
    - dia.maths.vector
    - dia.sfml.sfml
    - dia.sfml.sfml.system
    - dia.sfml.sfml.window
    - dia.window.interface
  forbidden: []
---
