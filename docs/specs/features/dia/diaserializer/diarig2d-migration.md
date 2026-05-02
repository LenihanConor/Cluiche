# Feature Spec: diarig2d-migration

## Parent System
@docs/specs/systems/dia/diaserializer.md

## Summary

Migrate DiaRig2D's serializer to use DiaSerializer types. Rename `ISkeletonLoader` → `ISkeletonSerializer` and `JsonSkeletonLoader` → `JsonSkeletonSerializer`; remove the local `MetadataValue` copy from `DiaRig2D/Bone.h`; adopt `SerializeResult` in place of bare `bool`; add `LoadFromFile`/`SaveToFile` convenience overloads to `ISkeletonSerializer`.

## Goals

- `DiaRig2D/Bone.h` no longer defines `MetadataValue` — it includes `<DiaSerializer/MetadataValue.h>` and uses `Dia::Serializer::MetadataValue`
- `ISkeletonLoader.h` → `ISkeletonSerializer.h`; class renamed `ISkeletonSerializer`; `Load`/`Save` return `SerializeResult`
- `JsonSkeletonLoader.h/.cpp` → `JsonSkeletonSerializer.h/.cpp`; class renamed `JsonSkeletonSerializer`; inherits `ISerializer`
- `JsonSkeletonSerializer` uses `JsonMetadataHelpers` instead of its own inline getMemberNames loop
- `ISkeletonSerializer` adds `LoadFromFile(const char* path, ...)` and `SaveToFile(const char* path, ...)` convenience overloads using `ISerializer::ReadFileToBuffer`/`WriteBufferToFile`
- All existing GoogleTest tests for DiaRig2D still pass with no change to test logic (only include paths + return type checks)

## Tasks

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1 | Add `DiaSerializer` as a dependency to `DiaRig2D.vcxproj` | Not Started | Add reference + AdditionalIncludeDirectories entry |
| 2 | Update `Dia/DiaRig2D/Bone.h` — remove local `MetadataValue`; use `Dia::Serializer::MetadataValue` | Not Started | Add `#include <DiaSerializer/MetadataValue.h>` |
| 3 | Rename `ISkeletonLoader.h` → `ISkeletonSerializer.h`; rename class; change return types to `SerializeResult` | Not Started | |
| 4 | Rename `JsonSkeletonLoader.h/.cpp` → `JsonSkeletonSerializer.h/.cpp`; rename class; inherit `ISerializer`; replace inline metadata loop with `JsonMetadataHelpers` | Not Started | |
| 5 | Add `LoadFromFile`/`SaveToFile` to `ISkeletonSerializer` (non-virtual convenience overloads) | Not Started | Call `ReadFileToBuffer` then in-memory `Load` |
| 6 | Update `DiaRig2D.vcxproj` and `.vcxproj.filters` — remove old filenames, add new filenames | Not Started | |
| 7 | Fix all include paths and using-declarations in DiaRig2D `.cpp` files | Not Started | |
| 8 | Update DiaRig2D GoogleTests — fix includes + SerializeResult usage; verify all pass | Not Started | |
| 9 | Update any other consumers of `ISkeletonLoader`/`JsonSkeletonLoader` in the codebase | Not Started | Search for all includes and update |

## Traceability

| Level | Spec | Decision |
|-------|------|----------|
| Platform | Cluiche.md | PD-004 no STL; PD-006 VS project files as source of truth |
| Application | dia.md | AD-003 namespace consistency |
| System | diaserializer.md | SD-004 domain interfaces extend ISerializer; SD-005 JsonMetadataHelpers header-only |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-004 | No STL in public APIs | `ISkeletonSerializer` methods use `const char*` + DiaCore containers; `SerializeResult` has no STL members |
| PD-006 | VS project files are source of truth | `DiaRig2D.vcxproj` updated to reflect file renames |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Breaking change | `ISkeletonLoader` is the public API — does the rename break consumers in CluicheTest? | Yes — mechanical rename. All consumers must update their includes. The GoogleTests build will catch any missed references. The rename happens in a single commit. |
| 2 | Return type change | Changing `bool Load(...)` to `SerializeResult Load(...)` at call sites — is `operator bool()` sufficient? | Yes — `if (!serializer.Load(...))` continues to compile. Tests that explicitly check `== true` / `== false` need minor updates. |
| 3 | LoadFromFile/SaveToFile — are these on the interface or only on the concrete class? | On the interface (`ISkeletonSerializer`) as non-virtual overloads that call `ISerializer::ReadFileToBuffer` then the pure-virtual in-memory `Load`. Concrete class does not need to override them. |

## Status

`Approved` — 2026-05-02
