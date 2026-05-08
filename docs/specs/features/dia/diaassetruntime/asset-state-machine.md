# Feature Spec: Asset State Machine

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetRuntime | @docs/specs/systems/dia/diaassetruntime.md |
| Feature | Asset State Machine | (this document) |

## Summary

Per-asset state tracking through a four-state lifecycle: `Registered -> Staged -> Loaded -> Unloading -> Registered`. The state machine enforces valid transitions, rejects invalid ones with DiaLogger warnings, and provides query APIs (`GetAssetState`, `IsAssetReady`). Consumer systems advance the state by calling `AcknowledgeAssetLoaded` and `AcknowledgeAssetUnloaded` after they finish loading or releasing content.

This feature builds on Feature 1's asset table and adds the lifecycle dimension. It does not trigger state transitions itself -- that is Feature 3's (Stage Lifecycle) responsibility. This feature defines the state storage, transition logic, and validation.

## Problem

Once assets are registered from the manifest, there is no way to track whether content has been requested, is actively loaded, or is being released. Without per-asset state, consuming systems (DiaGraphics, DiaAudio) cannot coordinate load/unload sequences, and the runtime cannot prevent double-loads or detect stale references. A state machine with acknowledgement-based transitions provides a clear contract between DiaAssetRuntime and its consumers.

## Acceptance Criteria

1. `AssetState` enum with values: `Registered`, `Staged`, `Loaded`, `Unloading`
2. Every asset in the runtime asset table has an associated `AssetState`, initialized to `Registered` after manifest load
3. Valid transitions: `Registered->Staged`, `Staged->Loaded`, `Loaded->Unloading`, `Unloading->Registered`
4. Invalid transitions are rejected -- the state does not change and DiaLogger emits a warning with the asset ID, current state, and attempted target state
5. `AcknowledgeAssetLoaded(assetId)` transitions an asset from `Staged` to `Loaded`
6. `AcknowledgeAssetUnloaded(assetId)` transitions an asset from `Unloading` to `Registered`
7. `GetAssetState(assetId)` returns the current state for a registered asset
8. `IsAssetReady(assetId)` returns true if and only if the asset is in the `Loaded` state
9. Calling `GetAssetState` or `IsAssetReady` with an unknown asset ID returns a sentinel/false and logs a DiaLogger warning
10. State storage is per-asset, stored alongside or parallel to the `RuntimeAssetEntry` table from Feature 1

## API Design

### Core Types

```cpp
namespace Dia::AssetRuntime
{
    enum class AssetState
    {
        Registered,
        Staged,
        Loaded,
        Unloading
    };
}
```

### API on AssetRuntime

```cpp
namespace Dia::AssetRuntime
{
    class AssetRuntime
    {
    public:
        // State queries
        AssetState GetAssetState(const Dia::Core::StringCRC& assetId) const;
        bool IsAssetReady(const Dia::Core::StringCRC& assetId) const;

        // Consumer acknowledgements
        void AcknowledgeAssetLoaded(const Dia::Core::StringCRC& assetId);
        void AcknowledgeAssetUnloaded(const Dia::Core::StringCRC& assetId);
    };
}
```

### State Transition Diagram

```
Registered ──[RequestStageLoad]──> Staged
Staged     ──[AcknowledgeAssetLoaded]──> Loaded
Loaded     ──[RequestStageUnload (ref=0)]──> Unloading
Unloading  ──[AcknowledgeAssetUnloaded]──> Registered
```

### Usage Pattern

```cpp
// After RequestStageLoad (Feature 3) moves assets to Staged
// Consumer receives OnAssetReady (Feature 4) and loads content
void MyGraphicsSystem::OnAssetReady(const StringCRC& assetId, const FilePath& path)
{
    mTextures.Load(assetId, path);
    runtime.AcknowledgeAssetLoaded(assetId); // Staged -> Loaded
}

// Query state
if (runtime.IsAssetReady(StringCRC("texture.player_ship")))
{
    // Safe to use the asset
}

// After RequestStageUnload (Feature 3) moves assets to Unloading
// Consumer receives OnAssetUnloading (Feature 4) and releases content
void MyGraphicsSystem::OnAssetUnloading(const StringCRC& assetId)
{
    mTextures.Unload(assetId);
    runtime.AcknowledgeAssetUnloaded(assetId); // Unloading -> Registered
}
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Implement AssetState enum and per-asset state storage | Add AssetState enum. Add a parallel HashTableC mapping asset IDs to their current state, initialized to Registered after manifest load. |
| 2 | Implement state transition logic with validation | Internal method that validates the transition (current state -> target state) against the allowed transition table. Rejects invalid transitions with DiaLogger warning. |
| 3 | Implement AcknowledgeAssetLoaded | Validates Staged->Loaded transition. Rejects if asset is not in Staged state. |
| 4 | Implement AcknowledgeAssetUnloaded | Validates Unloading->Registered transition. Rejects if asset is not in Unloading state. |
| 5 | Implement GetAssetState and IsAssetReady | Lookup by StringCRC ID. Return sentinel/false for unknown IDs with DiaLogger warning. |
| 6 | Add DiaLogger integration for state transitions | Log every successful transition (asset ID, old state, new state) at debug level. Log invalid transitions and unknown IDs at warning level. |
| 7 | Add GoogleTest coverage | Tests for: all valid transitions, all invalid transitions, acknowledge from wrong state, unknown asset ID queries, IsAssetReady correctness, state initialization after manifest load |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaAssetRuntime -- Manifest Load & Path Resolution (Feature 1) | RuntimeAssetEntry table populated by manifest loader; asset IDs to track state for |
| DiaCore CRC/StringCRC | Asset IDs for state lookups |
| DiaCore Containers | HashTableC for per-asset state storage |
| DiaLogger | Transition logging and invalid-transition warnings |

## Files

| File | Action |
|------|--------|
| `Dia/DiaAssetRuntime/AssetState.h` | Create -- AssetState enum |
| `Dia/DiaAssetRuntime/AssetRuntime.h` | Modify -- add state query and acknowledgement methods |
| `Dia/DiaAssetRuntime/AssetRuntime.cpp` | Modify -- implement state machine logic, transition validation, DiaLogger integration |
| `Dia/DiaAssetRuntime/DiaAssetRuntime.vcxproj` | Modify -- add new files |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Compliant.** All state lookups and transitions keyed by StringCRC asset ID. |
| PD-004 | No STL in public APIs | **Compliant.** AssetState is a plain enum. Query methods use StringCRC parameters and return plain types. No STL in public interface. |
| PD-005 | x64 Windows only | **Compliant.** No platform-specific code. |
| PD-007 | C++20 required | **Compliant.** Available but no specific C++20 features required. |
| PD-008 | Directory.Build.props owns build settings | **Compliant.** No vcxproj build setting overrides. |
| PD-009 | Generated output under Cluiche/out/ | **Not applicable.** No generated output. |
| AD-001 | YAML frontmatter module docs | **Compliant.** Module doc created in Feature 1. |
| AD-002 | No STL in public APIs | **Compliant.** Same as PD-004. |
| AD-003 | Namespace Dia::AssetRuntime:: | **Compliant.** All code under `Dia::AssetRuntime::`. |
| SD-CAT-001 | Asset IDs are type.name composites | **Compliant.** State machine operates on StringCRC IDs that follow the type.name convention. |
| SD-ARUN-001 | No DiaApplication dependency | **Compliant.** State machine is a pure library mechanism with no ProcessingUnit or Phase awareness. |
| SD-ARUN-008 | No DiaAssetCatalogue dependency | **Compliant.** No imports from DiaAssetCatalogue. State is tracked on RuntimeAssetEntry IDs, not catalogue records. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Transitions | What happens if `AcknowledgeAssetLoaded` is called for an asset already in `Loaded` state? | Rejected. DiaLogger warning with the asset ID and "already Loaded" message. State does not change. This catches double-acknowledgement bugs in consuming systems. |
| 2 | Transitions | Can an asset go directly from `Staged` to `Unloading` (skipping `Loaded`)? | Yes. If `RequestStageUnload` (Feature 3) is called before the consumer acknowledges loading, the asset transitions `Staged->Unloading`. The consumer must handle receiving `OnAssetUnloading` before finishing their load -- they should abort and call `AcknowledgeAssetUnloaded`. This is documented in the system spec AI Review Q2. |
| 3 | Storage | Should per-asset state be stored in the same HashTableC as RuntimeAssetEntry, or in a parallel table? | Parallel HashTableC<StringCRC, AssetState, 512>. Keeps RuntimeAssetEntry immutable after manifest load (it is pure lookup data). The state table is mutable and changes throughout the application lifetime. |
| 4 | Initialization | When is state initialized to Registered -- during RuntimeManifestLoader::Load, or when AssetRuntime::LoadManifest completes? | When AssetRuntime::LoadManifest completes. The AssetRuntime class calls the loader, then iterates the asset table and initializes every entry to Registered in the state table. The loader itself does not know about states. |
| 5 | Thread safety | Is the state machine thread-safe? | No. DiaAssetRuntime is a pure library (SD-ARUN-001). Thread safety is the responsibility of the Module wrapper that wires it into a ProcessingUnit. The state machine assumes single-threaded access. |
| 6 | Logging | Should state transition logs be at debug or info level? | Debug level for successful transitions (high frequency during stage loads). Warning level for invalid transitions and unknown asset IDs (these indicate bugs). |

## Status

`Approved`
