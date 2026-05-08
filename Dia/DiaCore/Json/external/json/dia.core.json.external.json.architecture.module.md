---
schema: dia.module.v1
module_id: dia.core.json.external.json
name: json
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Json/external/json
language: cpp
parent_module_id: dia.core.json.external

summary: >
  Defines json APIs (assertions, autolink, config, features, forwards, json, reader, value, version, writer) including: CZString, ErrorInfo, FastWriter, Features, JSON_API, Path, PathArgument, Reader, StaticString, StringStorage, StructuredError, StyledWriter.

intent: >
  Provide reusable json building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: CZString, ErrorInfo, FastWriter, Features, JSON_API, Path, PathArgument, Reader, StaticString, StringStorage, StructuredError, StyledWriter
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/Json/external/json/assertions.h
    - Dia/DiaCore/Json/external/json/autolink.h
    - Dia/DiaCore/Json/external/json/config.h
    - Dia/DiaCore/Json/external/json/features.h
    - Dia/DiaCore/Json/external/json/forwards.h
    - Dia/DiaCore/Json/external/json/json.h
    - Dia/DiaCore/Json/external/json/reader.h
    - Dia/DiaCore/Json/external/json/value.h
    - Dia/DiaCore/Json/external/json/version.h
    - Dia/DiaCore/Json/external/json/writer.h
  namespaces: []
  entry_points:
    - CZString
    - ErrorInfo
    - FastWriter
    - Features
    - JSON_API
    - Path
    - PathArgument
    - Reader
    - StaticString
    - StringStorage
    - StructuredError
    - StyledWriter

dependencies:
  required: []
  forbidden: []
---
