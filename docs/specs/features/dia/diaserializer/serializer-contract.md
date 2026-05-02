# Feature Spec: serializer-contract

## Parent System
@docs/specs/systems/dia/diaserializer.md

## Summary

Provide `SerializeResult` (structured error type replacing bare `bool`) and `ISerializer` abstract base (version string, migration capability query, file I/O helpers). These become the shared contract that all domain serializer interfaces (`IStateMachineSerializer`, `ISkeletonSerializer`, etc.) inherit from.

## Goals

- `SerializeResult { bool ok; const char* error; }` with `operator bool()` for drop-in compatibility with existing `if (!serializer.Load(...))` call sites
- `ISerializer` base with `GetVersion()` pure virtual, `CanMigrate(fromVersion)` virtual (default returns false), and protected `ReadFileToBuffer`/`WriteBufferToFile` utilities
- No allocation on any code path — errors are static string literals

## Tasks

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1 | Write `Dia/DiaSerializer/SerializeResult.h` | Not Started | `operator bool()`, `Success()`, `Failure(const char*)` static factories |
| 2 | Write `Dia/DiaSerializer/ISerializer.h` + `ISerializer.cpp` | Not Started | Pure abstract `GetVersion()`; default `CanMigrate()` returns false; protected `ReadFileToBuffer`/`WriteBufferToFile` implementations |
| 3 | Add to `DiaSerializer.vcxproj` and `.vcxproj.filters` | Not Started | |
| 4 | Write tests in `TestSerializerContract.cpp` | Not Started | `SerializeResult` bool operator; `Success`/`Failure` factories; `ReadFileToBuffer` with nonexistent path returns false + logs warning |

## Traceability

| Level | Spec | Decision |
|-------|------|----------|
| Platform | Cluiche.md | PD-004 no STL; PD-007 C++20 |
| Application | dia.md | AD-003 namespace `Dia::Serializer::` |
| System | diaserializer.md | SD-003 static error strings; SD-004 version + migration on base; SD-006 migration hook |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-004 | No STL in public APIs | `SerializeResult` has no STL members; `ISerializer` takes/returns `const char*` and plain integers |
| AD-003 | `Dia::<Module>::` namespace | `SerializeResult` and `ISerializer` in `Dia::Serializer::` |
| SD-003 | Static error string literals | `SerializeResult::error` is `const char*`; no dynamic allocation; callers must pass static literals |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | operator bool | Does `explicit operator bool()` break existing `if (result)` patterns? | No — `explicit` allows `if (result)` and `bool b = result;` but prevents implicit conversion in arithmetic contexts. This is the correct choice for a result type. |
| 2 | ReadFileToBuffer size | What happens if the file is larger than the caller's buffer? | Returns false and emits `DIA_LOG_WARNING` — same pattern as `DIA_ASSERT` for programmer errors. Callers must size their buffers appropriately (the file format + known schema version bounds the maximum). |
| 3 | Migration in v1 | No actual migrations are needed yet. Is there a risk of over-engineering the hook? | Low — `CanMigrate` defaults false and `ISerializer` owns no migration logic. The hook costs one virtual call and one `const char*` compare at load time. Concrete serializers activate it when needed. |

## Status

`Approved` — 2026-05-02
