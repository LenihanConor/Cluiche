# Feature Spec: Identity & Relationship Backbone

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaData | @docs/specs/systems/dia/diadata.md |
| Feature | Identity & Relationship Backbone | (this document) |

## Summary

The central registry that knows what assets exist and how they connect. Every asset gets a composite StringCRC identity (`type.name`), a canonical record with metadata and content hash, and a list of forward references to other assets. A relationship index computes reverse dependencies lazily on first query. The registry can be serialized to/from a JSON manifest, with support for manifest includes (sub-manifests referenced from a master file).

This is the backbone that the pipeline (DiaAssetPipeline), runtime loader (DiaStageLoader), and editor (DiaAssetBrowserEditor) all read from. It is pure data model — it does not load asset content, run validation, or execute build steps.

## Problem

Without a central registry, every system that touches assets must independently discover and track what exists and how it relates. The pipeline can't know what to build, the editor can't show relationships, and the runtime loader can't resolve Stage → Bundle → Asset chains. "What uses this texture?" is unanswerable without scanning the entire project. A single catalog with bidirectional relationships solves all of these.

## Acceptance Criteria

1. `AssetRecord` stores: composite StringCRC ID (`type.name`), asset type ID (StringCRC), source path (FilePath), content hash (uint32), metadata tags (DynamicArrayC of StringCRC), status (enum: active/deprecated/draft), and a list of forward references (DynamicArrayC of typed relationship edges)
2. Asset IDs are strictly `type.name` composites — enforced at registration. The type prefix must match a registered AssetTypeDescriptor (Feature 2).
3. `AssetRegistry` stores records in a HashTableC keyed by StringCRC ID
4. Registry is populated by external callers — the pipeline discover phase or manifest deserialization. DiaData does not scan directories itself.
5. Query by ID returns the record (or null)
6. Query by type returns all records of that asset type
7. Query by tag returns all records with that tag
8. `RelationshipIndex` maintains bidirectional relationships. Forward refs are stored per-record. Reverse refs are computed **lazily** on first reverse query, then cached. Cache is invalidated when records are added, removed, or updated.
9. Relationship types are StringCRC-typed and extensible. Ships with: `uses`, `contains`, `configured_by`, `depends_on`
10. `GetForwardRefs(assetId)` returns all assets this record references, optionally filtered by relationship type
11. `GetReverseRefs(assetId)` returns all assets that reference this record, optionally filtered by relationship type. First call triggers lazy index build.
12. Content hash (CRC32 of source file bytes) is stored per record at registration time — provided by the caller, not computed by the registry
13. Registry serializes to a single JSON manifest file
14. Manifest supports `"includes"` — an array of relative paths to sub-manifests that are loaded and merged into the registry
15. Manifest deserialization uses JsonDefinitionLoader (Feature 1) for reading and validation
16. Include resolution is non-recursive (one level deep) to prevent cycles and keep it simple. Sub-manifests cannot include other sub-manifests.
17. Duplicate IDs across manifests are rejected with a clear error
18. Pure data model — no file system scanning, no validation execution, no build steps, no asset content loading

## API Design

### Core Types

```cpp
namespace Dia::Data
{
    enum class AssetStatus
    {
        Active,
        Draft,
        Deprecated
    };

    struct RelationshipEdge
    {
        Dia::Core::StringCRC mRelationshipType; // e.g. "uses", "contains"
        Dia::Core::StringCRC mTargetAssetId;    // e.g. "texture.player_ship"
    };

    struct AssetRecord
    {
        Dia::Core::StringCRC mId;               // composite "type.name"
        Dia::Core::StringCRC mAssetTypeId;      // e.g. "texture", "entity"
        Dia::Core::FilePath mSourcePath;
        unsigned int mContentHash;
        AssetStatus mStatus;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> mTags;
        Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16> mReferences;
    };

    class AssetRegistry
    {
    public:
        bool Register(const AssetRecord& record);
        bool Remove(const Dia::Core::StringCRC& id);
        const AssetRecord* FindById(const Dia::Core::StringCRC& id) const;

        void QueryByType(const Dia::Core::StringCRC& typeId,
                         Dia::Core::Containers::DynamicArrayC<const AssetRecord*, 64>& results) const;
        void QueryByTag(const Dia::Core::StringCRC& tag,
                        Dia::Core::Containers::DynamicArrayC<const AssetRecord*, 64>& results) const;

        unsigned int GetCount() const;

        RelationshipIndex& GetRelationshipIndex();
        const RelationshipIndex& GetRelationshipIndex() const;
    };

    class RelationshipIndex
    {
    public:
        void GetForwardRefs(const Dia::Core::StringCRC& assetId,
                           Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16>& results) const;
        void GetForwardRefsByType(const Dia::Core::StringCRC& assetId,
                                  const Dia::Core::StringCRC& relationshipType,
                                  Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16>& results) const;

        void GetReverseRefs(const Dia::Core::StringCRC& assetId,
                           Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16>& results);
        void GetReverseRefsByType(const Dia::Core::StringCRC& assetId,
                                   const Dia::Core::StringCRC& relationshipType,
                                   Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16>& results);

        void InvalidateReverseCache();

    private:
        bool mReverseCacheDirty;
        // Reverse index built lazily on first GetReverseRefs call
    };
}
```

### Manifest Format

```json
{
    "includes": [
        "characters.manifest.json",
        "levels.manifest.json"
    ],
    "assets": [
        {
            "id": "texture.player_ship",
            "type": "texture",
            "source_path": "Raw/Textures/player_ship.texture.png",
            "content_hash": 2918374651,
            "status": "active",
            "tags": ["player", "ship"],
            "references": [
            ]
        },
        {
            "id": "entity.player_ship",
            "type": "entity",
            "source_path": "Raw/Entities/player_ship.entity.json",
            "content_hash": 1837462901,
            "status": "active",
            "tags": ["player", "ship"],
            "references": [
                { "type": "uses", "target": "texture.player_ship" },
                { "type": "uses", "target": "config.ship_stats" }
            ]
        },
        {
            "id": "bundle.player_assets",
            "type": "bundle",
            "source_path": "Raw/Bundles/player_assets.bundle.json",
            "content_hash": 9182736450,
            "status": "active",
            "tags": ["player"],
            "references": [
                { "type": "contains", "target": "entity.player_ship" },
                { "type": "contains", "target": "texture.player_ship" },
                { "type": "contains", "target": "config.ship_stats" }
            ]
        }
    ]
}
```

### Manifest Serialization API

```cpp
namespace Dia::Data
{
    class ManifestSerializer
    {
    public:
        ManifestSerializer(const JsonDefinitionLoader& loader);

        LoadResult<AssetRegistry> LoadManifest(const Dia::Core::FilePath& path) const;
        bool SaveManifest(const AssetRegistry& registry, const Dia::Core::FilePath& path) const;
    };
}
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Implement AssetRecord and supporting types | AssetRecord struct, AssetStatus enum, RelationshipEdge struct, built-in relationship type constants (kUses, kContains, kConfiguredBy, kDependsOn) |
| 2 | Implement AssetRegistry | HashTableC-backed registry. Register (with ID format validation against AssetTypeRegistry), Remove, FindById, QueryByType, QueryByTag. |
| 3 | Implement RelationshipIndex | Forward ref queries (read from records). Lazy reverse-ref index build on first reverse query. Cache invalidation on registry mutation. Filtered queries by relationship type. |
| 4 | Wire RelationshipIndex into AssetRegistry | Registry owns the index. Register/Remove calls invalidate reverse cache. |
| 5 | Implement ManifestSerializer | LoadManifest (reads JSON via JsonDefinitionLoader, populates registry, resolves single-level includes, rejects duplicates). SaveManifest (serializes registry to JSON). |
| 6 | Add GoogleTest coverage | Tests for: register/remove/find, query by type/tag, forward refs, reverse refs (lazy build + invalidation), manifest round-trip, manifest includes, duplicate ID rejection, ID format validation |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaData — JSON Definition Loader (Feature 1) | JsonDefinitionLoader for manifest deserialization, LoadResult/LoadError for error reporting |
| DiaData — Asset Type Framework (Feature 2) | AssetTypeRegistry for validating that asset type IDs in records match registered types |
| DiaCore TypeSystem | TypeDefinition for manifest record types |
| DiaCore CRC/StringCRC | Asset IDs, relationship type IDs, tag IDs |
| DiaCore Containers | HashTableC (registry), DynamicArrayC (reference lists, query results, tags) |
| DiaCore FilePath | Source paths in records, manifest file paths |
| DiaCore JSON | Manifest serialization/deserialization |

## Files

| File | Action |
|------|--------|
| `Dia/DiaData/AssetRecord.h` | Create — record struct, status enum, relationship edge |
| `Dia/DiaData/RelationshipTypes.h` | Create — built-in relationship type StringCRC constants |
| `Dia/DiaData/AssetRegistry.h` | Create — registry class header |
| `Dia/DiaData/AssetRegistry.cpp` | Create — registry implementation |
| `Dia/DiaData/RelationshipIndex.h` | Create — relationship index header |
| `Dia/DiaData/RelationshipIndex.cpp` | Create — relationship index implementation (lazy reverse build) |
| `Dia/DiaData/ManifestSerializer.h` | Create — manifest read/write header |
| `Dia/DiaData/ManifestSerializer.cpp` | Create — manifest implementation with include support |
| `Dia/DiaData/DiaData.vcxproj` | Modify — add new files |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Compliant.** Asset IDs, type IDs, relationship types, and tags are all StringCRC. Registry is CRC-keyed. |
| PD-002 | ProcessingUnit/Phase/Module | **Not applicable.** Pure data model, no lifecycle. |
| PD-003 | Component-based entities | **Compliant.** Entity Definition is a registered asset type. Its references to component types use StringCRC (PD-001). Component instantiation is DiaStageLoader's job. |
| PD-004 | No STL in public APIs | **Compliant.** HashTableC for registry, DynamicArrayC for reference lists and query results. No STL in public interface. |
| PD-005 | x64 Windows only | **Compliant.** No platform-specific code. |
| PD-007 | C++20 required | **Compliant.** Available but no specific C++20 features required. |
| PD-008 | Directory.Build.props | **Compliant.** DiaData.vcxproj inherits centralized settings. |
| PD-009 | Generated output under Cluiche/out/ | **Not applicable.** Manifest files are authored, not generated. Pipeline-generated manifests would go to Cluiche/out/ but that's DiaAssetPipeline's concern. |
| AD-001 | YAML frontmatter module docs | **Compliant.** DiaData module doc created in Feature 1. |
| AD-002 | No STL in public APIs | **Compliant.** Same as PD-004. |
| AD-003 | Namespace Dia::\<Module\>:: | **Compliant.** All code under `Dia::Data::`. |
| AD-005 | Component-based entities | **Compliant.** Same as PD-003. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Registry | Should AssetRegistry have a fixed capacity? What's a reasonable max? | Fixed capacity, 1024 records. Generous for a small game. Matches the engine's pattern of compile-time-sized containers. |
| 2 | Relationships | The reverse index is lazily built and cached. What invalidates it? | Any call to Register() or Remove() sets a dirty flag. Next GetReverseRefs() call rebuilds the full reverse index. This is simple and correct — optimization (incremental updates) can come later if profiling shows it matters. |
| 3 | Manifest | Includes are non-recursive (one level). Should the loader detect and error on nested includes, or silently ignore the `includes` field in sub-manifests? | Error. If a sub-manifest has an `includes` field, LoadManifest reports a LoadError. Silent ignore masks mistakes. |
| 4 | Identity | Composite IDs use `type.name` — what characters are allowed in the name portion? | Lowercase alphanumeric plus underscores and hyphens. Validated at Register() time. This matches DiaAPI command naming rules and prevents characters that would break file paths or JSON keys. |
| 5 | Manifest | Should SaveManifest output includes, or always flatten to a single file? | Flatten. SaveManifest serializes the current registry state as one file. Includes are an authoring convenience for organizing manifests by hand — they're resolved on load and not preserved. If the user wants to maintain a multi-file manifest structure, they author it manually. |
| 6 | Query | QueryByType and QueryByTag return results into a caller-provided DynamicArrayC. What if there are more results than the array capacity? | The query fills the array up to capacity and returns the total count so the caller knows results were truncated. For most queries on a small game, 64 is plenty. Callers with larger needs can use a bigger array. |
| 7 | Hashing | Content hash is provided by the caller, not computed by the registry. Should the registry enforce that it's non-zero? | Yes — a zero hash is treated as "not computed" and is acceptable (pipeline may register a record before hashing its source). But the registry should expose a `HasContentHash()` helper that checks for non-zero, so downstream systems can distinguish hashed from unhashed records. |

## Status

`Approved`
