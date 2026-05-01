---
schema: dia.module.v1
module_id: dia.window.interface
name: Interface
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaWindow/Interface
language: cpp
parent_module_id: dia.window

summary: >
  Defines Interface APIs (IWindow, IWindowFactory) including: Dimensions, IWindow, IWindowFactory, Settings, Style.

intent: >
  Provide reusable Interface building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: Dimensions, IWindow, IWindowFactory, Settings, Style
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaWindow/Interface/IWindow.h
    - Dia/DiaWindow/Interface/IWindowFactory.h
  namespaces: []
  entry_points:
    - Dimensions
    - IWindow
    - IWindowFactory
    - Settings
    - Style

dependencies:
  required:
    - dia.core.containers.bitflag
    - dia.core.core
    - dia.core.strings
    - dia.graphics.interface
    - dia.maths.vector
    - dia.window
  forbidden: []
---
