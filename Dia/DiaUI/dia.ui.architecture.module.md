---
schema: dia.module.v1
module_id: dia.ui
name: UI
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaUI
language: cpp
parent_module_id: dia.root

summary: >
  Defines UI APIs (BoundMethod, IUISystem, Page, UIDataBuffer) including: BoundMethod, BoundMethodArgs, BoundMethodValue, IUISystem, Page, UIDataBuffer.

intent: >
  Provide reusable UI building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: BoundMethod, BoundMethodArgs, BoundMethodValue, IUISystem, Page, UIDataBuffer
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaUI/BoundMethod.h
    - Dia/DiaUI/IUISystem.h
    - Dia/DiaUI/Page.h
    - Dia/DiaUI/UIDataBuffer.h
  namespaces: []
  entry_points:
    - BoundMethod
    - BoundMethodArgs
    - BoundMethodValue
    - IUISystem
    - Page
    - UIDataBuffer

dependencies:
  required:
    - dia.core.containers.arrays
    - dia.core.containers.misc
    - dia.core.core
    - dia.core.filepath
    - dia.core.memory
    - dia.core.strings
    - dia.input
  forbidden: []
---
