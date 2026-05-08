---
schema: dia.module.v1
module_id: dia.assetcatalogue
name: DiaAssetCatalogue
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaAssetCatalogue
language: cpp
parent_module_id: dia

summary: >
  Asset catalogue module providing typed JSON definition loading with validation.

intent: >
  Provides a single-call API to load JSON files from disk or buffers, deserialize them
  into typed C++ objects via DiaCore's TypeJsonSerializer, and validate required fields.

responsibilities:
  - Loading JSON asset definitions from disk or string buffers
  - Deserializing JSON into registered C++ types
  - Validating required fields via TypeVariableAttributeRequired
  - Returning structured LoadResult<T> with errors or populated values

non_responsibilities:
  - Asset streaming or async loading
  - Asset hot-reload
  - Asset dependency resolution

dependent_modules: []

public_api:
  headers:
    - DiaAssetCatalogue/LoadResult.h
    - DiaAssetCatalogue/JsonDefinitionLoader.h
  namespaces:
    - Dia::AssetCatalogue
  entry_points:
    - JsonDefinitionLoader
    - LoadResult
    - LoadError
    - LoadErrorKind

dependencies:
  required:
    - dia.core
  forbidden: []
---
