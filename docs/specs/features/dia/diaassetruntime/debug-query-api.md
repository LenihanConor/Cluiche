# Feature Spec: Debug Query API

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetRuntime | @docs/specs/systems/dia/diaassetruntime.md |
| Feature | Debug Query API | (this document) |

## Summary

Read-only query methods on `AssetRuntime` that return snapshots of runtime state for debugging, profiling, and editor integration. `GetLoadedAssets` returns all assets currently in the `Loaded` state. `GetStagedAssets` returns all assets in the `Staged` state. `GetStageDependencies` returns all asset IDs belonging to a named Stage. Results are written into caller-provided `DynamicArrayC` containers, truncated if the array fills, with total count returned for overflow detection.

These queries are the foundation that Feature 6 (DiaAPI Debug Commands) exposes over WebSocket.

## Problem

During development, there is no way to inspect what DiaAssetRuntime currently considers loaded, staged, or associated with a Stage -- short of stepping through a debugger. A debug query API gives tooling (editors, debug overlays, automated tests) read-only access to runtime state without modifying it.

## Acceptance Criteria

1. `GetLoadedAssets(results)` populates the provided `DynamicArrayC<StringCRC, 128>` with asset IDs of all assets currently in `Loaded` state
2. `GetStagedAssets(results)` populates the provided `DynamicArrayC<StringCRC, 128>` with asset IDs of all assets currently in `Staged` state
3. `GetStageDependencies(stageId, results)` populates the provided `DynamicArrayC<StringCRC, 128>` with all asset IDs listed in the named Stage's `RuntimeStageEntry`
4. All three methods return `unsigned int` -- the total count of matching assets (which may exceed the array capacity)
5. If the results array fills before all matches are written, remaining results are silently dropped but the returned count reflects the true total
6. `GetStageDependencies` with an unknown stage ID returns 0 and logs a DiaLogger warning
7. All queries are const -- they do not modify state, ref counts, or transitions
8. Results are unordered (iteration order of the backing HashTableC)
9. Queries are safe to call at any time after `LoadManifest` -- including during stage transitions

## API Design

### API on AssetRuntime

```cpp
namespace Dia::AssetRuntime
{
    class AssetRuntime
    {
    public:
        // Debug queries -- all const, read-only
        unsigned int GetLoadedAssets(
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128>& results) const;

        unsigned int GetStagedAssets(
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128>& results) const;

        unsigned int GetStageDependencies(
            const Dia::Core::StringCRC& stageId,
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128>& results) const;
    };
}
```

### Usage Pattern

```cpp
// In a debug overlay or editor panel
Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128> loaded;
unsigned int totalLoaded = runtime.GetLoadedAssets(loaded);
// totalLoaded might be > loaded.Size() if there are more than 128 loaded assets

Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128> stageDeps;
unsigned int totalDeps = runtime.GetStageDependencies(StringCRC("stage.gameplay"), stageDeps);

for (unsigned int i = 0; i < loaded.Size(); ++i)
{
    const FilePath* path = runtime.ResolveAssetPath(loaded[i]);
    DebugDrawText(loaded[i], path);
}
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Implement GetLoadedAssets | Iterate the asset state table, collect all assets in Loaded state into the results array. Return total count. |
| 2 | Implement GetStagedAssets | Iterate the asset state table, collect all assets in Staged state into the results array. Return total count. |
| 3 | Implement GetStageDependencies | Look up RuntimeStageEntry by stage ID, copy its mAssetIds into the results array. Return total count. Log warning for unknown stage ID. |
| 4 | Add GoogleTest coverage | Tests for: GetLoadedAssets returns correct assets after stage load + acknowledge, GetStagedAssets returns correct assets after stage load (before acknowledge), GetStageDependencies returns stage members, unknown stage returns 0, overflow truncation with correct total count, empty results when nothing loaded |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaAssetRuntime -- Manifest Load & Path Resolution (Feature 1) | RuntimeStageEntry table for GetStageDependencies |
| DiaAssetRuntime -- Asset State Machine (Feature 2) | Per-asset state for filtering by Loaded/Staged |
| DiaAssetRuntime -- Stage Lifecycle & Reference Counting (Feature 3) | State transitions that establish Loaded/Staged states |
| DiaCore CRC/StringCRC | Asset IDs and Stage IDs in query parameters and results |
| DiaCore Containers | DynamicArrayC for result output |
| DiaLogger | Warning for unknown stage ID |

## Files

| File | Action |
|------|--------|
| `Dia/DiaAssetRuntime/AssetRuntime.h` | Modify -- add GetLoadedAssets, GetStagedAssets, GetStageDependencies |
| `Dia/DiaAssetRuntime/AssetRuntime.cpp` | Modify -- implement debug query methods |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Compliant.** All query parameters and results are StringCRC. |
| PD-004 | No STL in public APIs | **Compliant.** Results written to DynamicArrayC. No std::vector or std::string in the interface. |
| PD-005 | x64 Windows only | **Compliant.** No platform-specific code. |
| PD-007 | C++20 required | **Compliant.** Available but no specific C++20 features required. |
| PD-008 | Directory.Build.props owns build settings | **Compliant.** No vcxproj build setting overrides. |
| PD-009 | Generated output under Cluiche/out/ | **Not applicable.** No generated output. |
| AD-001 | YAML frontmatter module docs | **Compliant.** Module doc created in Feature 1. |
| AD-002 | No STL in public APIs | **Compliant.** Same as PD-004. |
| AD-003 | Namespace Dia::AssetRuntime:: | **Compliant.** All code under `Dia::AssetRuntime::`. |
| SD-CAT-001 | Asset IDs are type.name composites | **Compliant.** Queries return type.name StringCRC IDs. |
| SD-ARUN-001 | No DiaApplicationFlow dependency | **Compliant.** Debug queries are pure library methods with no lifecycle coupling. |
| SD-ARUN-008 | No DiaAssetCatalogue dependency | **Compliant.** Queries operate on RuntimeStageEntry and AssetState data, not catalogue records. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Capacity | Why 128 as the DynamicArrayC capacity for results? | 128 is generous for a debug query. Most games have fewer than 128 assets loaded simultaneously. The total count return value lets callers detect truncation. Callers with larger needs can use a bigger array at their call site. |
| 2 | Consistency | Are the results a consistent snapshot, or can they change mid-iteration? | They are a copy into the caller's array, so the results are consistent at the time of the query. If a stage transition happens on another thread after the query returns, the results are stale -- but DiaAssetRuntime is single-threaded (SD-ARUN-001), so this is not a concern. |
| 3 | Performance | GetLoadedAssets and GetStagedAssets iterate the entire state table. Is that acceptable? | Yes. The state table has up to 512 entries (HashTableC capacity). Linear scan of 512 entries is negligible for a debug query that runs at most once per frame. No optimization needed. |
| 4 | API | Should there be a GetUnloadingAssets query as well? | Not in this feature. Unloading is a transient state -- assets pass through it briefly during stage unloads. If profiling or debugging needs it later, it can be added with the same pattern. |
| 5 | Ordering | Should results be sorted (e.g., alphabetically by ID)? | No. Results are in iteration order of the backing HashTableC, which is effectively unordered. Sorting is the caller's responsibility if needed. Keeping the query simple and fast is the priority. |
| 6 | GetStageDependencies | Does GetStageDependencies return all assets in the stage, regardless of their current state? | Yes. It returns the asset IDs from RuntimeStageEntry.mAssetIds, which is the static stage membership from the manifest. It does not filter by state. This is useful for "what does this stage need?" queries before loading. |

## Status

`Approved`
