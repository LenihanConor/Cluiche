---
schema: dia.module.v1
module_id: dia.core.frame
name: Frame
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Frame
language: cpp
parent_module_id: dia.core

summary: >
  Defines Frame APIs (FrameStream) including: FrameStream, InternalData.

intent: >
  Provide reusable Frame building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: FrameStream, InternalData
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/Frame/FrameStream.h
  namespaces: []
  entry_points:
    - FrameStream
    - InternalData

dependencies:
  required:
    - dia.core.containers.arrays
    - dia.core.time
  forbidden: []
---
