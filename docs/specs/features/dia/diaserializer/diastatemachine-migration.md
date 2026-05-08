# Feature Spec: diastatemachine-migration

## Parent System
@docs/specs/systems/dia/diaserializer.md

## Summary

Migrate DiaStateMachine's serializer to use DiaSerializer types. Remove `StateMachineMetadata.h`; adopt `Dia::Serializer::MetadataValue`/`MetadataArray`; make `MarkValid()` private with `friend class JsonStateMachineSerializer` on all three definition classes; add `CallbackRegistry::Finalize()`/`IsFinalized()` with a `DIA_ASSERT(registry.IsFinalized())` guard in all `Load()` paths; adopt `SerializeResult`; add `LoadFromFile`/`SaveToFile` to `IStateMachineSerializer`.

## Goals

- `DiaStateMachine/StateMachineMetadata.h` deleted; all three definition headers include `<DiaSerializer/MetadataValue.h>` instead
- `MarkValid()` is private on `StateMachineDefinition`, `HierarchicalStateMachineDefinition`, and `PushdownAutomatonDefinition`; `friend class JsonStateMachineSerializer` declared on each
- `CallbackRegistry` gains `Finalize()` (one-way gate; second call is `DIA_ASSERT`) and `IsFinalized()` const query
- `DIA_ASSERT(registry.IsFinalized())` appears at the top of all three `Load()` paths in `JsonStateMachineSerializer`
- `IStateMachineSerializer::Load`/`Save` return `SerializeResult`; interface extends `ISerializer`
- `IStateMachineSerializer` adds `LoadFromFile`/`SaveToFile` non-virtual convenience overloads
- All 22 existing serialization GoogleTests still pass; tests updated for `SerializeResult` return type

## Tasks

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1 | Add `DiaSerializer` as a dependency to `DiaStateMachine.vcxproj` | Not Started | Add reference + AdditionalIncludeDirectories |
| 2 | Delete `Dia/DiaStateMachine/StateMachineMetadata.h`; update all three definition headers to include `<DiaSerializer/MetadataValue.h>` | Not Started | Namespace changes: `Dia::StateMachine::MetadataValue` → `Dia::Serializer::MetadataValue` |
| 3 | Make `MarkValid()` private on all three definition classes; add `friend class JsonStateMachineSerializer` | Not Started | One declaration per header |
| 4 | Add `Finalize()`/`IsFinalized()` to `CallbackRegistry`; update `CallbackRegistry.cpp` | Not Started | `DIA_ASSERT` on double-finalize |
| 5 | Add `DIA_ASSERT(registry.IsFinalized())` at top of all three `Load()` methods in `JsonStateMachineSerializer.cpp` | Not Started | |
| 6 | Update `IStateMachineSerializer.h` — extend `ISerializer`; change return types to `SerializeResult`; add `LoadFromFile`/`SaveToFile` non-virtual overloads | Not Started | |
| 7 | Update `JsonStateMachineSerializer.h/.cpp` — fix return types; replace `DIA_ASSERT(false, ...)` data-error cases with `return SerializeResult::Failure(...)` + `DIA_LOG_WARNING` | Not Started | Buffer-too-small in Save still uses `DIA_ASSERT` (programmer error) |
| 8 | Remove `StateMachineMetadata.h` from `DiaStateMachine.vcxproj` and `.vcxproj.filters` | Not Started | |
| 9 | Update all 22 serialization GoogleTests — fix includes, `SerializeResult` usage, `registry.Finalize()` in test setup | Not Started | |
| 10 | Verify full GoogleTests build + run passes | Not Started | |

## Traceability

| Level | Spec | Decision |
|-------|------|----------|
| Platform | Cluiche.md | PD-004 no STL; PD-006 VS project files |
| Application | dia.md | AD-003 namespace |
| System | diaserializer.md | SD-007 MarkValid private + friend; SD-008 CallbackRegistry::Finalize; SD-004 domain interfaces extend ISerializer |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-004 | No STL in public APIs | `IStateMachineSerializer` uses `const char*` + DiaCore containers; `SerializeResult` has no STL members |
| PD-006 | VS project files are source of truth | `DiaStateMachine.vcxproj` updated to remove old header, add no new files (StateMachineMetadata.h was already header-only) |
| SD-007 | MarkValid() private + friend | Three `friend class JsonStateMachineSerializer` declarations; `MarkValid()` moved to private section |
| SD-008 | CallbackRegistry::Finalize boot-order guard | `DIA_ASSERT(registry.IsFinalized())` in all three `Load()` paths |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Namespace change impact | Code that uses `Dia::StateMachine::MetadataValue` directly (not through a definition API) will break. How widespread is this? | Only `JsonStateMachineSerializer.cpp` and the 22 serialization tests directly touch `MetadataValue`. Both are updated in this feature. |
| 2 | Friend pattern scalability | If a second JSON backend (e.g. XmlStateMachineSerializer) is added later, it also needs the `friend` declaration. Is this manageable? | Yes — one `friend` line per definition header per serializer class. Three definitions × N serializers. Acceptable for a system that will have at most 2–3 concrete serializers. |
| 3 | registry.Finalize() in tests | All 22 serialization tests construct a `CallbackRegistry` and call `Load()`. Will the assert fire if tests don't call `Finalize()` before `Load()`? | Yes — by design. Tests must call `registry.Finalize()` before `Load()` as part of the updated test setup. This mirrors real usage: register callbacks, finalize, then load. |
| 4 | Double-finalize | Is double-`Finalize()` a realistic mistake? | Unlikely but possible if setup code is copy-pasted across phases. `DIA_ASSERT` makes it an immediate failure in debug rather than a silent no-op. |

## Status

`Approved` — 2026-05-02
