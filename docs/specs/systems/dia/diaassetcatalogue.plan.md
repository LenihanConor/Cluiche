# Plan: DiaAssetCatalogue

**Spec:** @docs/specs/systems/dia/diaassetcatalogue.md  
**Status:** Not Started  
**Started:** —  
**Last Updated:** 2026-05-04

## Tasks

| # | Task | Status | Model | Notes |
|---|------|--------|-------|-------|
| 1 | Feature 1 — JSON Definition Loader | Not Started | sonnet | Create `DiaAssetCatalogue` vcxproj + solution entry. Implement `LoadError`/`LoadResult<T>`, `JsonDefinitionLoader` (Load from FilePath + LoadFromBuffer). Add `TypeVariableAttributeRequired` to DiaCore. Required-field + type-mismatch validation (recursive). 6 GoogleTest scenarios. |
| 2 | Feature 2 — Asset Type Framework | Not Started | sonnet | `AssetTypeDescriptor`, `AssetTypeRegistry` (HashTableC, 64 cap). Add `TypeVariableAttributeAssetReference` to DiaCore. Minimal stub TypeDefinitions for all 8 built-in types. `RegisterBuiltInAssetTypes()`. 7 GoogleTest scenarios. |
| 3 | Feature 3 — Identity & Relationship Backbone | Not Started | sonnet | `AssetRecord`, `AssetStatus`, `AssetScope`, `RelationshipEdge`, built-in relationship type constants. `AssetRegistry` (HashTableC, 1024 cap) with Register/Remove/FindById/QueryByType/QueryByTag. `RelationshipIndex` with forward refs, lazy reverse-ref build, cache invalidation. `CatalogueManifestSerializer` (LoadManifest with single-level include support, SaveManifest flattened). 6 GoogleTest scenarios. |
| 4 | Feature 4 — Catalogue Automation | Not Started | opus | `ContentHasher` (single file CRC32; directory hash combining sorted-per-file CRC32s). `ScopeComputer` (Stage contains-graph traversal; `RecomputeAllScopes`). `RelationshipInferrer` (walk TypeDefinition for `TypeVariableAttributeAssetReference` fields). `CatalogueRulesEngine` — LoadRules with full schema + referential validation; snapshot-based evaluation; dry-run; Apply; conflict detection (mIsConflict). 10 GoogleTest scenarios. |
| 5 | Exhaustive Testing — all 4 features | Not Started | opus | Exhaustive GoogleTest coverage across all 4 features. Covers: golden values, invariants, stress, boundary, determinism, integration across feature boundaries. Target: meaningful coverage of every public API surface. Add all tests to GoogleTests.vcxproj. Build and run to confirm all pass. |

## Session Notes

### 2026-05-04
- Plan created. All 4 feature specs Approved. Build order: 1 → 2 → 3 → 4 → 5.
- Feature 1 creates the vcxproj and DiaCore additions that all subsequent features depend on.
- Feature 4 is the most complex task — rules engine snapshot semantics, conflict detection, and directory hashing justify opus.
- Task 5 (exhaustive testing) runs after Feature 4 is complete — tests the full integrated system.
- DiaAssetCatalogueEditor is blocked on this system being Done.
