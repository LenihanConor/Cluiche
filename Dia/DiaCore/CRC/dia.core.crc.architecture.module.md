---
schema: dia.module.v1
module_id: dia.core.crc
name: CRC
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/CRC
language: cpp
parent_module_id: dia.core

summary: >
  Defines CRC APIs (CRC, CRCHashFunctor, StringCRC, StripStringCRC) including: CRC, CRCHashFunctor, StringCRC, StringCRCHashFunctor, StripStringCRC, StripStringCRCHashFunctor.

intent: >
  Provide reusable CRC building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: CRC, CRCHashFunctor, StringCRC, StringCRCHashFunctor, StripStringCRC, StripStringCRCHashFunctor
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/CRC/CRC.h
    - Dia/DiaCore/CRC/CRCHashFunctor.h
    - Dia/DiaCore/CRC/StringCRC.h
    - Dia/DiaCore/CRC/StripStringCRC.h
  namespaces: []
  entry_points:
    - CRC
    - CRCHashFunctor
    - StringCRC
    - StringCRCHashFunctor
    - StripStringCRC
    - StripStringCRCHashFunctor

dependencies:
  required:
    - dia.core.containers.hashtables
    - dia.core.memory
    - dia.core.type
  forbidden: []
---
