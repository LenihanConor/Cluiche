# Feature Spec: Stage Lifecycle & Reference Counting

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetRuntime | @docs/specs/systems/dia/diaassetruntime.md |
| Feature | Stage Lifecycle & Reference Counting | (this document) |

## Summary

`RequestStageLoad` and `RequestStageUnload` are the game-code-facing entry points for loading and releasing assets at Stage boundaries. `RequestStageLoad(stageId)` looks up the `RuntimeStageEntry`, iterates its member assets, increments per-asset reference counts, and transitions newly-referenced assets from `Registered` to `Staged`. `RequestStageUnload(stageId)` decrements reference counts and transitions assets to `Unloading` only when their reference count drops to zero.

Global assets (scope = `kGlobal`) can be referenced by multiple Stages simultaneously and are ref-counted accordingly. Stage-scoped assets (scope = `kStage`) have a ref count of 0 or 1 -- they belong to exactly one Stage.

## Problem

Without Stage-level lifecycle management, game code would need to manually track which assets to load and unload during Stage transitions, handle shared assets across Stages, and prevent premature unloading of global assets still in use by another Stage. This is error-prone and duplicates logic that should live in the runtime. Reference counting at the asset level, driven by Stage membership, automates this correctly.

## Acceptance Criteria

1. `RequestStageLoad(stageId)` looks up the `RuntimeStageEntry` for the given stage ID and iterates all member asset IDs
2. For each asset: increment its reference count by 1
3. For each asset whose reference count transitions from 0 to 1: transition state from `Registered` to `Staged` (triggers downstream events via Feature 4)
4. For assets already in `Staged` or `Loaded` state (ref count was already > 0): ref count is incremented, no state change, no events
5. `RequestStageUnload(stageId)` looks up the `RuntimeStageEntry` and iterates all member asset IDs
6. For each asset: decrement its reference count by 1
7. For each asset whose reference count drops to 0: transition state to `Unloading` (triggers downstream events via Feature 4)
8. For assets whose reference count is still > 0 after decrement: no state change, no events
9. Stage-scoped assets always have ref count 0 or 1 -- they are owned by exactly one Stage
10. Global assets can have ref count > 1 when referenced by multiple loaded Stages
11. `RequestStageLoad` on an already-loaded Stage: increments ref counts on its assets, no state changes, no events (idempotent for already-active assets)
12. `RequestStageUnload` on an unknown stage ID: DiaLogger warning, no crash, no state changes
13. `RequestStageUnload` called twice for the same Stage (double unload): second call decrements ref counts that may already be 0 -- ref count is clamped at 0, no negative values, DiaLogger warning for the double-unload
14. Per-asset reference count is queryable for debug purposes

## API Design

### API on AssetRuntime

```cpp
namespace Dia::AssetRuntime
{
    class AssetRuntime
    {
    public:
        // Stage lifecycle
        void RequestStageLoad(const Dia::Core::StringCRC& stageId);
        void RequestStageUnload(const Dia::Core::StringCRC& stageId);

        // Debug: ref count query
        unsigned int GetAssetRefCount(const Dia::Core::StringCRC& assetId) const;
    };
}
```

### Reference Count Rules

| Scope | Ref count range | Behavior |
|-------|----------------|----------|
| `kStage` | 0 or 1 | Owned by one Stage. Load sets to 1, unload sets to 0. |
| `kGlobal` | 0..N | Shared across Stages. Each `RequestStageLoad` that includes this asset increments by 1. Each `RequestStageUnload` decrements by 1. Unloading only when it reaches 0. |

### Usage Pattern

```cpp
Dia::AssetRuntime::AssetRuntime runtime;
runtime.LoadManifest(manifestPath);
runtime.RegisterListener(&graphicsSystem);

// Load Gameplay stage -- all its assets transition Registered -> Staged
runtime.RequestStageLoad(StringCRC("stage.gameplay"));

// Load MainMenu stage -- shared global assets get ref count 2, no re-stage
runtime.RequestStageLoad(StringCRC("stage.main_menu"));

// Unload Gameplay -- shared assets drop to ref count 1, stay Loaded
// Stage-only assets drop to 0, transition to Unloading
runtime.RequestStageUnload(StringCRC("stage.gameplay"));

// Unload MainMenu -- shared assets drop to ref count 0, transition to Unloading
runtime.RequestStageUnload(StringCRC("stage.main_menu"));
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Implement per-asset reference count storage | Add a parallel HashTableC<StringCRC, unsigned int, 512> for ref counts, initialized to 0 for all assets after manifest load. Add `GetAssetRefCount` query method. |
| 2 | Implement RequestStageLoad | Look up RuntimeStageEntry, iterate member assets, increment ref count, transition Registered->Staged for newly-loaded assets (ref count 0->1). |
| 3 | Implement RequestStageUnload | Look up RuntimeStageEntry, iterate member assets, decrement ref count (clamp at 0), transition to Unloading for assets reaching ref count 0. |
| 4 | Handle edge cases | Unknown stage ID (warning, no crash). Already-loaded stage (increment only, no state changes). Double unload (clamp, warning). |
| 5 | Add GoogleTest coverage | Tests for: single stage load/unload, shared global asset across two stages, stage-scoped asset lifecycle, ref count correctness, double load idempotency, double unload clamping, unknown stage warning |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaAssetRuntime -- Manifest Load & Path Resolution (Feature 1) | RuntimeStageEntry table for Stage->Asset expansion, RuntimeAssetEntry for scope information |
| DiaAssetRuntime -- Asset State Machine (Feature 2) | State transition logic (Registered->Staged, Loaded->Unloading) |
| DiaCore CRC/StringCRC | Stage IDs and asset IDs |
| DiaCore Containers | HashTableC for ref count storage, DynamicArrayC in RuntimeStageEntry |
| DiaLogger | Warnings for unknown stages, double unloads |

## Files

| File | Action |
|------|--------|
| `Dia/DiaAssetRuntime/AssetRuntime.h` | Modify -- add RequestStageLoad, RequestStageUnload, GetAssetRefCount |
| `Dia/DiaAssetRuntime/AssetRuntime.cpp` | Modify -- implement stage lifecycle logic and ref counting |
| `Dia/DiaAssetRuntime/DiaAssetRuntime.vcxproj` | Modify -- no new files, but may update if internal helpers are factored out |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Compliant.** Stage IDs and asset IDs are StringCRC. Ref count table is CRC-keyed. |
| PD-004 | No STL in public APIs | **Compliant.** RequestStageLoad/Unload take StringCRC. GetAssetRefCount returns unsigned int. No STL. |
| PD-005 | x64 Windows only | **Compliant.** No platform-specific code. |
| PD-007 | C++20 required | **Compliant.** Available but no specific C++20 features required. |
| PD-008 | Directory.Build.props owns build settings | **Compliant.** No vcxproj build setting overrides. |
| PD-009 | Generated output under Cluiche/out/ | **Not applicable.** No generated output. |
| AD-001 | YAML frontmatter module docs | **Compliant.** Module doc created in Feature 1. |
| AD-002 | No STL in public APIs | **Compliant.** Same as PD-004. |
| AD-003 | Namespace Dia::AssetRuntime:: | **Compliant.** All code under `Dia::AssetRuntime::`. |
| SD-CAT-001 | Asset IDs are type.name composites | **Compliant.** Stage->Asset expansion uses type.name StringCRC IDs from the runtime manifest. |
| SD-ARUN-001 | No DiaApplication dependency | **Compliant.** Stage lifecycle is driven by explicit game-code calls, not by ProcessingUnit phases. |
| SD-ARUN-008 | No DiaAssetCatalogue dependency | **Compliant.** Stage membership comes from RuntimeStageEntry (loaded from assets.runtime.json), not from DiaAssetCatalogue relationships. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Ref counting | Can a Stage-scoped asset appear in multiple RuntimeStageEntries? | No -- the pipeline should not produce this. But if it does, the ref-counting logic handles it correctly (the asset would behave like a global asset with ref count > 1). DiaAssetRuntime does not enforce scope constraints at load time -- it trusts the manifest. |
| 2 | Ordering | What if RequestStageUnload is called while assets are still in `Staged` state (consumer has not yet acknowledged loading)? | Assets in Staged state with ref count dropping to 0 transition directly to Unloading. The consumer receives OnAssetUnloading and must abort its pending load, then call AcknowledgeAssetUnloaded. This is the cancellation path. |
| 3 | Ref counting | Should ref counts be signed or unsigned? | Unsigned with clamping at 0. A negative ref count would indicate a bug (double unload). Clamping prevents underflow and the DiaLogger warning catches the logic error. |
| 4 | Performance | Is iterating all assets in a RuntimeStageEntry on every RequestStageLoad acceptable? | Yes. RuntimeStageEntry holds up to 64 asset IDs (DynamicArrayC<StringCRC, 64>). Iterating 64 entries and doing 64 HashTableC lookups is negligible. Stage loads are infrequent (once per level transition). |
| 5 | Edge case | What if RequestStageLoad is called with a valid stage ID but the stage has zero assets? | No-op. Ref counts unchanged, no state transitions, no events. This is valid -- an empty stage is a legal configuration (e.g., a cutscene stage with no assets). |
| 6 | Double load | RequestStageLoad called twice for the same stage. Should DiaAssetRuntime track which stages are "active" to detect this? | No explicit stage tracking. Each call increments ref counts. The caller is responsible for not double-loading. If they do, the ref counts will be elevated and will need a matching number of unloads. This is consistent with ref counting semantics -- every load must be balanced by an unload. |

## Status

`Approved`
