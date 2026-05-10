# System Spec: DiaData

## Parent Application
@docs/specs/applications/dia.md

## Research Context
@docs/research/engine_data_arch/summary.md

## Summary

DiaData is the foundational data model layer for the Dia engine's asset architecture. It provides stable identity (StringCRC-based) for every meaningful game object, a type framework for defining and registering asset types, a central registry for cataloging and querying all project data with bidirectional relationship tracking, and a JSON definition loader for deserializing authored content into validated C++ objects.

DiaData is pure infrastructure with no side effects — it does not load files at runtime, run build pipelines, or render UI. It answers: "what data exists, what type is it, how is it connected, and how do I read it into C++?"

Other systems build on top: DiaAssetPipeline (build-time), DiaStageLoader (runtime), and DiaDataManagementEditor (tooling).

## Responsibilities

**Owns:**
- Asset identity model — every asset gets a StringCRC ID (composite `type.name`, e.g. `"texture.player_ship"`)
- Asset type framework — registered descriptors define schema, file patterns, validation rules per type
- Asset taxonomy — the canonical set of asset types: Texture, Mesh/Sprite, Audio, Config, Entity Definition, Stage, UI Definition, Bundle, UI Bundle
- Central asset registry — catalog of all known assets with metadata, tags, forward references, and computed reverse-reference index
- JSON definition loading — single-call API for JSON file → validated typed C++ object via TypeSystem
- Bidirectional relationship graph — forward deps stored per-record, reverse deps computed and indexed

**Does NOT own:**
- Build/cook pipeline (DiaAssetPipeline)
- Runtime asset loading and Stage/Bundle resolution (DiaStageLoader)
- Editor browsing and inspection UI (DiaDataManagementEditor)
- Validation execution as a pipeline phase (DiaAssetPipeline — but DiaData provides the validation primitives)
- File watching or hot reload
- Binary serialization of built assets

## Asset Taxonomy

An **asset** is authored content with a stable identity that goes through the pipeline and gets loaded at runtime.

| Asset Type | File Pattern | Description |
|------------|-------------|-------------|
| Texture | `*.texture.png` | Image assets — sprites, backgrounds, UI elements |
| Mesh/Sprite | `*.sprite.json` | Sprite definitions referencing textures |
| Audio | `*.audio.wav` | Sound effects and music |
| Config | `*.config.json` | Game configuration data (weapon stats, tuning values) |
| Entity Definition | `*.entity.json` | Component-based entity templates (which components, what params) |
| Stage | `*.stage.json` | Hard game transition boundary — owns systems, references Bundles |
| UI Definition | `*.ui.json` | UI layout and behavior definitions |
| Bundle | `*.bundle.json` | Named grouping of assets, can contain other Bundles |
| UI Bundle | `*.uibundle.json` | UI-specific asset set (layouts + styles + icons + fonts as a unit) |

New asset types are added by registering a descriptor. The taxonomy is extensible — game-specific types can be added without modifying DiaData.

## Public API

### Core Classes

| Class | Responsibility |
|-------|---------------|
| `JsonDefinitionLoader` | Loads a JSON file, deserializes via TypeJsonSerializer into a typed C++ object, validates required fields. Single-call API: give it a path and a type, get back a validated object. |
| `AssetTypeDescriptor` | Defines an asset type: file extension pattern, schema constraints (required fields, value ranges, reference types), validation function. One descriptor per asset type. |
| `AssetTypeRegistry` | Central registry of AssetTypeDescriptors. Register a type, query by ID or file pattern. |
| `AssetRecord` | Canonical record for one asset: StringCRC ID, type, source path, metadata (tags, status), forward references, content hash. |
| `AssetRegistry` | Central catalog of all AssetRecords. Populated by importers or manifest scan. Queries: by ID, by type, by tag. Maintains computed reverse-reference index. |
| `RelationshipIndex` | Bidirectional relationship graph. Forward refs stored per-record; reverse refs ("what uses this?") computed and queryable. Supports relationship types: `uses`, `contains`, `configured_by`, `depends_on`. |

### Namespace

`Dia::Data::`

All public headers under `Dia/DiaData/`.

### Include Pattern

```cpp
#include <DiaData/AssetRegistry.h>
#include <DiaData/AssetTypeDescriptor.h>
#include <DiaData/JsonDefinitionLoader.h>
#include <DiaData/RelationshipIndex.h>
```

## Dependencies

| Dependency | What DiaData uses from it |
|------------|--------------------------|
| DiaCore — TypeSystem | Runtime RTTI, field reflection, TypeJsonSerializer, TypeVariableAttributes for schema constraints |
| DiaCore — CRC/StringCRC | Asset identity (composite `type.name` IDs) |
| DiaCore — Containers | HashTableC (registry backing store), DynamicArrayC (reference lists), Handle<T> (safe asset references) |
| DiaCore — FilePath/PathStore | Path aliasing for source locations, deferred resolution |
| DiaCore — JSON | jsoncpp wrapper for reading/writing authored JSON |

No dependencies on DiaAPI, DiaApplicationFlow, DiaEditor, or any rendering/windowing module.

## Features

| # | Feature | Size | Description | Spec |
|---|---------|------|-------------|------|
| 1 | JSON Definition Loader | S | Single-call API: JSON file → validated typed C++ object via TypeSystem | [json-definition-loader.md](../../features/dia/diadata/json-definition-loader.md) |
| 2 | Asset Type Framework | S-M | Type descriptor registration, file pattern matching, schema definition via TypeSystem reflection | [asset-type-framework.md](../../features/dia/diadata/asset-type-framework.md) |
| 3 | Identity & Relationship Backbone | M | Asset registry with StringCRC identity, metadata, bidirectional relationship tracking (forward + reverse deps) | [identity-relationship-backbone.md](../../features/dia/diadata/identity-relationship-backbone.md) |

**Build order:** 1 → 2 → 3 (strict dependency chain)

## Design Constraints

- **Content hashing from the start.** Every AssetRecord stores a content hash of its source file at registration time. This enables future incremental builds (DiaAssetPipeline) without rework in DiaData.
- **Reverse dependencies are first-class.** The RelationshipIndex computes and maintains reverse refs — not a query-time scan. This is an architectural commitment in the data model.
- **Validation primitives, not validation execution.** DiaData provides the schema constraints (via AssetTypeDescriptor) and referential integrity checks (via RelationshipIndex). DiaAssetPipeline orchestrates when validation runs.
- **No STL containers in public APIs (PD-004, AD-002).** All public interfaces use DiaCore containers.
- **StringCRC identity (PD-001).** Asset IDs are StringCRC values, not raw strings.

## Downstream Systems

| System | How it uses DiaData | Status |
|--------|-------------------|--------|
| DiaAssetPipeline | Reads AssetTypeRegistry for per-type build steps. Populates AssetRegistry during discover phase. Runs validation against type schemas and relationship integrity. | Planned |
| DiaStageLoader | Reads AssetRegistry to resolve Stage → Bundle → Asset chains at runtime. Uses Handle<T> for safe asset references. | Planned |
| DiaDataManagementEditor | Reads AssetRegistry and RelationshipIndex to present browse, inspect, and relationship views. | Future |

## Inherited Binding Decisions

| ID | Decision | Source | Implication for DiaData |
|----|----------|--------|------------------------|
| PD-001 | StringCRC for all entity/component IDs | Platform | Asset IDs are StringCRC values (composite `type.name`). All registry lookups are CRC-keyed. |
| PD-002 | ProcessingUnit/Phase/Module architecture | Platform | DiaData itself does not use ProcessingUnit (it's a data model, not an app). Downstream systems (DiaStageLoader) integrate with this pattern. |
| PD-003 | Component-based entities (IComponent/IComponentObject) | Platform | Entity Definition assets define component compositions. DiaData stores the definition; DiaStageLoader instantiates via ComponentFactoryRegistry. |
| PD-004 | No STL containers in public APIs | Platform | All DiaData public interfaces use HashTableC, DynamicArrayC, Handle<T>. |
| PD-005 | x64 Windows only | Platform | No cross-platform considerations in DiaData. |
| PD-007 | C++20 required | Platform | Can use concepts for type constraints on AssetTypeDescriptor, std::span for zero-copy views, constexpr for compile-time validation. |
| PD-008 | Directory.Build.props owns build settings | Platform | DiaData.vcxproj follows centralized build configuration. |
| PD-009 | Generated output under Cluiche/out/ | Platform | DiaData does not generate output itself, but defines the record structure that DiaAssetPipeline writes to Cluiche/out/. |
| AD-001 | Module system with YAML frontmatter | Application | DiaData gets a `dia.data.architecture.module.md` with YAML frontmatter. Child modules get their own architecture docs. |
| AD-002 | No STL in public APIs | Application | Same as PD-004, reinforced at application level. |
| AD-003 | Namespace: Dia::\<Module\>:: | Application | All DiaData code under `Dia::Data::` namespace. |
| AD-005 | Component-based entities | Application | Entity Definition assets are component-aware — they reference IComponent types by StringCRC. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Identity | Should asset IDs be strictly `type.name` composites, or should the format be flexible? | Strict `type.name` — the type prefix enables efficient per-type queries on the registry without a secondary index. Enforced at registration time. |
| 2 | Taxonomy | Should new asset types require C++ code (registering a descriptor), or should types be definable in data? | C++ registration for now. TypeSystem reflection requires compiled types. Data-driven type definitions could be a future extension but add significant complexity. |
| 3 | Relationships | Should the relationship vocabulary (uses, contains, etc.) be fixed or extensible? | Extensible — relationships are StringCRC-typed. DiaData ships with a starter set (uses, contains, configured_by, depends_on) but game-specific types can be registered. |
| 4 | Registry | Should the registry persist to disk (JSON manifest) or be rebuilt in-memory each time? | Both. The registry can be populated from a manifest file (for tools/editor) or built in-memory by importers (for the pipeline). The manifest is a serialized snapshot of registry state. |
| 5 | Hashing | CRC32 has non-trivial collision probability at scale. Is this acceptable? | Yes for a small game. StringCRC already stores the original string alongside the hash for debug comparison. If collisions become a problem, the identity model can be upgraded to CRC64 without changing the registry's API — it's keyed by StringCRC, not by raw uint32. |
| 6 | Threading | Does the AssetRegistry need to be thread-safe? | Not in DiaData itself. DiaData is a data model — thread safety is the responsibility of the system that owns the lifecycle (DiaAssetPipeline at build time, DiaStageLoader at runtime). DiaData's API should be reentrant but not internally synchronized. |
| 7 | Scope | Where is the line between DiaData's validation primitives and DiaAssetPipeline's validation execution? | DiaData provides: schema constraints on AssetTypeDescriptor (required fields, value ranges, reference types) and referential integrity queries on RelationshipIndex (does this ID exist? are there dangling refs?). DiaAssetPipeline provides: orchestration (when to validate), reporting (structured error output), and the decision to reject or pass assets. |

## Status

`Approved` — all 3 child feature specs Approved
