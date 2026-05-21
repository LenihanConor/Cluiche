---
schema: dia.module.v1
module_id: dia.core.reflect
name: Reflect
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Reflect
language: cpp
parent_module_id: dia.core

summary: >
  Archive-based reflection and serialization system. Eventual replacement for DiaCore/Type.
  Types register via DIA_SERIALIZE macro DSL; archives (JSON/binary) are swapped at the call site.

intent: >
  Provide a single serialize(Archive&, T&, version) free function pattern per type,
  with swappable read/write archive implementations. Versioned migration, pointer ownership,
  container support, and field attributes built in.

responsibilities:
  - Archive concept (Dia::Reflect::Archive) and archive implementations
  - Macro DSL (DIA_SERIALIZE, DIA_FIELD, DIA_BASE, DIA_SERIALIZE_POLYMORPHIC, etc.)
  - Container archive specializations (T[N], DynamicArrayC, HashTableC)
  - Polymorphic type registry (type CRC → factory, opt-in only)
  - Field attributes (Required, Range, AssetRef)
  - Version migration (per-type version integer, in-function guards)

non_responsibilities:
  - JSON parsing (DiaCore/Json/ owns jsoncpp wrapper)
  - Asset pipeline loading (DiaAssetCatalogue)
  - Domain-specific serialization formats (.diagame, .diastage)

dependent_modules: []

public_api:
  headers:
    - DiaCore/Reflect/Reflect.h
  namespaces:
    - Dia::Reflect::
  entry_points: []

dependencies:
  required:
    - dia.core.containers
    - dia.core.crc
    - dia.core.json
    - dia.core.core
  forbidden:
    - dia.maths
    - dia.graphics
