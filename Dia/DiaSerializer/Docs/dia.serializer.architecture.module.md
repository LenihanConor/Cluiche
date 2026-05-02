---
schema: dia.module.v1
module_id: dia.serializer
name: DiaSerializer
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaSerializer
language: cpp
parent_module_id: dia.core

summary: >
  Shared serialization primitives: MetadataValue/MetadataArray, SerializeResult,
  ISerializer interface, and JsonMetadataHelpers.

intent: >
  Provides the canonical types and contracts that all domain serializers (ISkeletonSerializer,
  IStateMachineSerializer, etc.) build on, eliminating duplication and ensuring consistency
  across all format-agnostic serialization in the engine.

responsibilities:
  - MetadataValue (typed discriminated union: bool/int/float/string)
  - MetadataEntry and MetadataArray (fixed-cap 16)
  - SetMetadata/FindMetadata inline helpers
  - JsonMetadataHelpers header-only read/write utilities
  - SerializeResult structured error type (replaces bare bool)
  - ISerializer abstract base (version string, migration query, file I/O helpers)

non_responsibilities:
  - JSON parsing (jsoncpp stays in DiaCore)
  - Domain types (SkeletonDef, StateMachineDefinition, etc.)
  - Asset identity, registries, or build pipeline (DiaData)
  - Binary/XML format implementations

dependent_modules: []

public_api:
  headers:
    - DiaSerializer/MetadataValue.h
    - DiaSerializer/JsonMetadataHelpers.h
    - DiaSerializer/SerializeResult.h
    - DiaSerializer/ISerializer.h
  namespaces:
    - Dia::Serializer
  entry_points:
    - MetadataValue
    - MetadataArray
    - SetMetadata
    - FindMetadata
    - SerializeResult
    - ISerializer

dependencies:
  required:
    - dia.core
    - dia.logger
  forbidden:
    - dia.rig2d
    - dia.statemachine
    - dia.data
---
