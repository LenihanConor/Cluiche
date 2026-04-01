---
schema: dia.module.v1
module_id: dia.core.containers.bitflag
name: BitFlag
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Containers/BitFlag
language: cpp
parent_module_id: dia.core.containers

summary: >
  Defines BitFlag APIs (BitArray16, BitArray32, BitArray64, BitArray8) including: BitArray16, BitArray32, BitArray64, BitArray8.

intent: >
  Provide reusable BitFlag building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: BitArray16, BitArray32, BitArray64, BitArray8
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/Containers/BitFlag/BitArray16.h
    - Dia/DiaCore/Containers/BitFlag/BitArray32.h
    - Dia/DiaCore/Containers/BitFlag/BitArray64.h
    - Dia/DiaCore/Containers/BitFlag/BitArray8.h
  namespaces: []
  entry_points:
    - BitArray16
    - BitArray32
    - BitArray64
    - BitArray8

dependencies:
  required:
    - dia.core.core
  forbidden: []
---
