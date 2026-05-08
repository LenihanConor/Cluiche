---
schema: dia.module.v1
module_id: dia.core.strings
name: Strings
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Strings
language: cpp
parent_module_id: dia.core

summary: >
  Defines Strings APIs (String, String1024, String128, String256, String32, String512, String64, String8, stringutils) including: String, String1024, String128, String256, String32, String512, String64, String8.

intent: >
  Provide reusable Strings building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: String, String1024, String128, String256, String32, String512, String64, String8
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/Strings/String.h
    - Dia/DiaCore/Strings/String1024.h
    - Dia/DiaCore/Strings/String128.h
    - Dia/DiaCore/Strings/String256.h
    - Dia/DiaCore/Strings/String32.h
    - Dia/DiaCore/Strings/String512.h
    - Dia/DiaCore/Strings/String64.h
    - Dia/DiaCore/Strings/String8.h
    - Dia/DiaCore/Strings/stringutils.h
  namespaces:
    - Dia::Dia
  entry_points:
    - String
    - String1024
    - String128
    - String256
    - String32
    - String512
    - String64
    - String8

dependencies:
  required:
    - dia.core.containers.arrays
    - dia.core.core
    - dia.core.json.external.json
    - dia.core.type
  forbidden: []
---
