---
schema: dia.module.v1
module_id: dia.core.filepath
name: FilePath
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/FilePath
language: cpp
parent_module_id: dia.core

summary: >
  Defines FilePath APIs (FileLoad, FilePath, IFileLoad, Path, PathStore, PathStoreConfig, SerializedFileLoad) including: AliasAppendPathConfig, AliasPathConfigTuple, FileLoad, FilePath, IFileLoad, Path, PathStore, PathStoreConfig, PathStoreConfigFragment, SerializedFileLoad.

intent: >
  Provide reusable FilePath building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: AliasAppendPathConfig, AliasPathConfigTuple, FileLoad, FilePath, IFileLoad, Path, PathStore, PathStoreConfig, PathStoreConfigFragment, SerializedFileLoad
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/FilePath/FileLoad.h
    - Dia/DiaCore/FilePath/FilePath.h
    - Dia/DiaCore/FilePath/IFileLoad.h
    - Dia/DiaCore/FilePath/Path.h
    - Dia/DiaCore/FilePath/PathStore.h
    - Dia/DiaCore/FilePath/PathStoreConfig.h
    - Dia/DiaCore/FilePath/SerializedFileLoad.h
  namespaces: []
  entry_points:
    - AliasAppendPathConfig
    - AliasPathConfigTuple
    - FileLoad
    - FilePath
    - IFileLoad
    - Path
    - PathStore
    - PathStoreConfig
    - PathStoreConfigFragment
    - SerializedFileLoad

dependencies:
  required:
    - dia.core.containers.arrays
    - dia.core.containers.strings
    - dia.core.core
    - dia.core.crc
    - dia.core.json.external.json
    - dia.core.strings
    - dia.core.type
  forbidden: []
---
