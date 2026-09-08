# Feature Spec: Manifest Heap Modules

**Parent:** @docs/specs/applications/dia/systems/diaapplicationflow/diaapplicationflow.md  
**Status:** Deferred  
**Plan:** @docs/specs/applications/dia/systems/diaapplicationflow/manifest-heap-modules.plan.md

## Summary

Migrate `ProcessingUnitDeclaration.modules` from `DynamicArrayC<ModuleDeclaration, 32>` (fixed-capacity, stack-inline) to `DynamicArray<ModuleDeclaration>` (heap-allocated, sized at load time). This is the dominant source of stack bloat in `ApplicationManifestV3`: 32 × ~424-byte `ModuleDeclaration` structs embedded inline per PU yields ~13.6 KB per `ProcessingUnitDeclaration` and ~60 KB total for the manifest. After this change, stack footprint for the manifest drops to a few hundred bytes; heap is allocated once during deserialization, sized exactly to what the JSON declares.

## Goals

- Reduce `ApplicationManifestV3` stack footprint from ~60 KB to negligible
- Size `modules` array at load time from manifest data — no wasted reserved slots
- Preserve all existing deserialization, validation, and runtime behaviour

## Out of Scope

- Migrating nested arrays inside `ModuleDeclaration` (`stages`, `dependencies`, `channels`) — these are small (≤16 CRCs each) and not worth the churn here
- Migrating other `DynamicArrayC` fields in `ApplicationManifestV3` (`stages`, `streams`, `processingUnits`) — secondary contributors; can be a follow-on
- Changing `DynamicArray` ownership model or container internals

## Acceptance Criteria

1. `ProcessingUnitDeclaration.modules` declared as `Dia::Core::Containers::DynamicArray<ModuleDeclaration>` (no capacity template arg)
2. Manifest deserializer reserves/adds exactly `N` entries where `N` = count of modules in JSON for that PU — no over-allocation
3. `ApplicationManifestV3` on-stack size is ≤ 1 KB (verified by `sizeof` assertion or comment)
4. All existing manifest unit tests (serialization round-trip, validation) pass unchanged
5. No memory leaks: `DynamicArray` correctly destroyed when manifest goes out of scope
6. Code compiles cleanly with no new warnings

## Tasks

| # | Task | Status |
|---|------|--------|
| 1 | Change `ProcessingUnitDeclaration.modules` field type in `ApplicationManifestV3.h` | - |
| 2 | Update deserializer to `Reserve(N)` then `Add()` per module entry | - |
| 3 | Audit all call sites that copy or move `ApplicationManifestV3` / `ProcessingUnitDeclaration` — fix if needed | - |
| 4 | Add `static_assert` or comment documenting reduced sizeof | - |
| 5 | Run and confirm manifest tests pass | - |

## Files Expected to Change

| File | Change |
|------|--------|
| `Dia/DiaApplicationFlow/Manifest/ApplicationManifestV3.h` | Field type change |
| `Dia/DiaApplicationFlow/Manifest/ApplicationManifestV3Serializer.cpp` (or equivalent) | Reserve + Add pattern |
| Any test or call site that value-copies `ProcessingUnitDeclaration` | Update copy semantics if needed |

## Binding Decisions

- **PD-004 / AD-002 — No STL containers in public APIs:** `DynamicArray<T>` is a DiaCore container; compliant.
- **SD-001 — Config is sole source of truth:** Sizing the heap array from parsed JSON data rather than a compile-time constant reinforces this decision — the manifest's shape is determined entirely by the config file.

## Open Design Questions

1. **Copy semantics at call sites:** `DynamicArray<T>` is heap-owning. Are there places that value-copy `ApplicationManifestV3` or `ProcessingUnitDeclaration` (e.g. test helpers, editor state)? If so, do they need to switch to move or pointer/reference passing? Worth auditing before Task 3.

2. **Phased migration vs all-at-once:** The nested arrays inside `ModuleDeclaration` (`stages` cap 16, `dependencies` cap 8, `channels` cap 4) are small individually but add up when multiplied by 32 modules. Is a follow-on spec for those worthwhile, or is the `modules` field migration sufficient for the foreseeable future?
