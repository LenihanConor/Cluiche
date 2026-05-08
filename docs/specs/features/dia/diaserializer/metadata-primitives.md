# Feature Spec: metadata-primitives

## Parent System
@docs/specs/systems/dia/diaserializer.md

## Summary

Provide the canonical `MetadataValue`, `MetadataEntry`, `MetadataArray`, `SetMetadata`/`FindMetadata` helpers, and `JsonMetadataHelpers` in `Dia::Serializer::`. Remove the duplicated copies currently in `DiaRig2D/Bone.h` and `DiaStateMachine/StateMachineMetadata.h`.

## Goals

- Single authoritative definition of `MetadataValue` (tagged union: bool/int/float/string) in `Dia/DiaSerializer/`
- `MetadataArray` = `DynamicArrayC<MetadataEntry, 16>` under `Dia::Serializer::`
- `SetMetadata` (insert/overwrite) and `FindMetadata` (nullable-pointer lookup) inline helpers
- `JsonMetadataHelpers.h` — header-only utilities for reading/writing `MetadataArray` from/to jsoncpp `Json::Value`
- All existing DiaRig2D and DiaStateMachine metadata behaviour preserved; only namespace + include path changes

## Tasks

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1 | Create `Dia/DiaSerializer/` directory and `DiaSerializer.vcxproj` static library | Not Started | x64 only, C++20, no OutDir override |
| 2 | Create `DiaSerializer.vcxproj.filters` and add to `Cluiche.sln` | Not Started | Under Dia solution folder |
| 3 | Create `dia.serializer.architecture.module.md` YAML module doc | Not Started | parent: dia.core; no game-facing dependencies |
| 4 | Write `Dia/DiaSerializer/MetadataValue.h` — tagged union + `MetadataEntry` + `MetadataArray` + `SetMetadata`/`FindMetadata` | Not Started | Namespace `Dia::Serializer::` |
| 5 | Write `Dia/DiaSerializer/JsonMetadataHelpers.h` — header-only `WriteMetadataToJson`/`ReadMetadataFromJson` | Not Started | Includes `<DiaCore/Json/json.h>`; not included by `MetadataValue.h` |
| 6 | Write unit tests in `Cluiche/Tests/GoogleTests/Serializer/TestMetadataPrimitives.cpp` | Not Started | SetMetadata round-trip all types, overwrite, full-cap assert, JsonMetadataHelpers all types + malformed |
| 7 | Update `GoogleTests.vcxproj` + `.vcxproj.filters` to include new test file | Not Started | |

## Traceability

| Level | Spec | Decision |
|-------|------|----------|
| Platform | Cluiche.md | PD-001 StringCRC keys; PD-004 no STL in public APIs; PD-007 C++20; PD-008 Directory.Build.props |
| Application | dia.md | AD-001 YAML module doc; AD-002 no STL; AD-003 namespace `Dia::Serializer::` |
| System | diaserializer.md | SD-001 tagged union not std::variant; SD-002 cap 16; SD-009 `Dia::Serializer::` namespace |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | `MetadataEntry::key` is `StringCRC`; no raw `const char*` keys |
| PD-004 | No STL in public APIs | `MetadataArray` is `DynamicArrayC`; `MetadataValue` is tagged union |
| PD-007 | C++20 | `DiaSerializer.vcxproj` compiled under `/std:c++20` |
| PD-008 | Directory.Build.props owns OutDir/IntDir | vcxproj has no OutDir/IntDir/PlatformToolset overrides |
| AD-003 | `Dia::<Module>::` namespace | All types in `Dia::Serializer::` |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | String storage | StringCRC loses the original string text. Does anything in the codebase need to recover the original string value from a MetadataValue of type kString? | No — metadata values of type kString are matched by CRC at runtime. Debug tools that want the text must maintain their own string table (same pattern as StringCRC throughout the engine). |
| 2 | SetMetadata full-cap | Should the full-cap case be DIA_ASSERT or silently drop? | DIA_ASSERT — running out of metadata slots is a programmer error (per SD-002, 16 is the agreed cap for v1). Silent drop would hide misconfigured definitions. |
| 3 | JsonMetadataHelpers include isolation | Can `MetadataValue.h` be included without pulling in jsoncpp? | Yes — `JsonMetadataHelpers.h` is a separate header that includes `<DiaCore/Json/json.h>`. `MetadataValue.h` has no jsoncpp dependency. |

## Status

`Approved` — 2026-05-02
