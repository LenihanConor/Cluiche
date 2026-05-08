# Plan: DiaAssetRuntime

**Spec:** @docs/specs/systems/dia/diaassetruntime.md  
**Status:** Done  
**Started:** 2026-05-05  
**Last Updated:** 2026-05-06

## Overview

Pure C++ library under `Dia/DiaAssetRuntime/`. No DiaApplication dependency. Namespace `Dia::AssetRuntime::`. Uses DiaCore containers (HashTableC, DynamicArrayC), StringCRC, FilePath, JSON. Feature 6 adds an optional DiaAPI dependency for debug commands. All tests live in GoogleTests project.

Build order is strictly sequential: F1 → F2 → F3 → F4 → F5 → F6. Each feature builds on the prior one's types and APIs.

## Tasks

| # | Task | Feature | Status | Model | Notes |
|---|------|---------|--------|-------|-------|
| 1 | Create DiaAssetRuntime.vcxproj + filters + module doc | F1 | Done | haiku | New VS project referencing DiaCore + DiaLogger. `dia.assetruntime.architecture.module.md` with YAML frontmatter. Add project to Cluiche.sln. |
| 2 | Implement AssetScope enum, RuntimeAssetEntry, RuntimeStageEntry | F1 | Done | haiku | `AssetScope.h`, `RuntimeAssetEntry.h`, `RuntimeStageEntry.h` — plain data types. mDeployPath uses String512 (FilePath is alias-based; can't hold arbitrary absolute paths). |
| 3 | Implement RuntimeManifestLoader | F1 | Done | sonnet | `RuntimeManifestLoader.h/.cpp` — parse JSON via DiaCore JSON, resolve relative→absolute paths, populate HashTableC tables, reject duplicates |
| 4 | Implement AssetRuntime::LoadManifest + ResolveAssetPath | F1 | Done | sonnet | `AssetRuntime.h/.cpp` — calls RuntimeManifestLoader, stores tables, resolves asset ID → String512*. Returns String512* not FilePath* (see Task 2 note). |
| 5 | Implement path alias registration in DiaCore PathStore | F1 | Done | sonnet | After manifest load, iterate asset table and register each ID as a path alias. Overwrites existing with warning. |
| 6 | GoogleTest: Feature 1 | F1 | Done | sonnet | 12 tests, all passing: successful load, path resolution, duplicate ID rejection, missing file, malformed JSON, folder trailing slash, scope parsing, stage table, alias registration. |
| 7 | Implement AssetState enum + per-asset state storage | F2 | Done | haiku | `AssetState.h` — enum. Parallel `HashTableC<StringCRC, AssetState, 512>` in AssetRuntime, initialized to Registered after LoadManifest. |
| 8 | Implement state transition validation logic | F2 | Done | sonnet | Internal `TryTransition(assetId, targetState)` — validates allowed transitions, DiaLogger warns on invalid |
| 9 | Implement AcknowledgeAssetLoaded + AcknowledgeAssetUnloaded | F2 | Done | haiku | Public API methods that call TryTransition(Staged→Loaded) and (Unloading→Registered) |
| 10 | Implement GetAssetState + IsAssetReady | F2 | Done | haiku | Lookup by StringCRC. Return sentinel/false for unknown IDs + DiaLogger warning. |
| 11 | GoogleTest: Feature 2 | F2 | Done | sonnet | 9 tests all passing. Fixed: PathStore::RegisterToStore asserts on duplicate — RegisterPathAliases now skips (not overwrites) already-registered aliases. |
| 12 | Implement per-asset ref count storage + GetAssetRefCount | F3 | Done | haiku | `HashTableC<StringCRC, unsigned int, 512>`, initialized to 0. Public query method. |
| 13 | Implement RequestStageLoad | F3 | Done | sonnet | Lookup RuntimeStageEntry, iterate member assets, increment ref count, transition Registered→Staged for 0→1 assets |
| 14 | Implement RequestStageUnload + edge cases | F3 | Done | sonnet | Decrement ref counts (clamp at 0), transition to Unloading when reaching 0. Handle unknown stage, double unload (warning). |
| 15 | GoogleTest: Feature 3 | F3 | Done | sonnet | 13 tests, all passing: single load/unload, shared global asset, double load/unload, unknown stage, ref count init. |
| 16 | Implement IAssetStateListener interface | F4 | Done | haiku | `IAssetStateListener.h` — abstract class with virtual dtor, OnAssetReady(StringCRC, String512), OnAssetUnloading(StringCRC). Uses String512 not FilePath (consistent with F1 design). |
| 17 | Implement listener registration + deferred removal | F4 | Done | sonnet | `DynamicArrayC<IAssetStateListener*, 16>`, RegisterListener (reject dup), UnregisterListener (defer if dispatching), mIsDispatching flag |
| 18 | Wire event dispatch into state transitions | F4 | Done | sonnet | Dispatch OnAssetReady on Registered→Staged, OnAssetUnloading on →Unloading. Pass mDeployPath String512 to OnAssetReady. |
| 19 | GoogleTest: Feature 4 | F4 | Done | sonnet | 9 tests, all passing: single/multi listener, unregister during dispatch, duplicate registration, resolved path correctness, no events for re-stage. |
| 20 | Implement GetLoadedAssets + GetStagedAssets | F5 | Done | haiku | Iterate mAssetTable, cross-ref mStateTable. HashTableC has no GetKeyByIndexConst — iterate asset table for IDs instead. |
| 21 | Implement GetStageDependencies | F5 | Done | haiku | Lookup RuntimeStageEntry, copy mAssetIds into results. Return total count. Warn for unknown stage. |
| 22 | GoogleTest: Feature 5 | F5 | Done | sonnet | 9 tests passing. Stage capacity = 64 (DynamicArrayC<StringCRC,64>), so overflow test uses 64-asset stage not 130. |
| 23 | Register get_loaded, get_staged, get_state commands | F6 | Done | sonnet | `AssetRuntimeDebugCommands.h/.cpp` — DiaAPI handlers calling Feature 5 queries, JSON via jsoncpp FastWriter |
| 24 | Register get_stage_deps + get_all_states commands | F6 | Done | sonnet | Full snapshot (staged+loaded) + stage deps → JSON. Error responses for unknowns. |
| 25 | Register subscribe_transitions (push stream) | F6 | Done | sonnet | Internal IAssetStateListener (TransitionLogger) logs events via DiaLogger. Push-stream to WebSocket requires DiaDebugServer layer above this. |
| 26 | GoogleTest: Feature 6 | F6 | Done | sonnet | 10 tests passing: command registration, all 6 commands callable, valid/invalid params, subscribe idempotency. |
| 27 | Add DiaAssetRuntime project reference to GoogleTests.vcxproj | — | Done | haiku | Done in F1. |

## File Map

```
Dia/DiaAssetRuntime/
    DiaAssetRuntime.vcxproj          # Task 1
    DiaAssetRuntime.vcxproj.filters  # Task 1
    dia.assetruntime.architecture.module.md  # Task 1
    AssetScope.h                     # Task 2
    RuntimeAssetEntry.h              # Task 2
    RuntimeStageEntry.h              # Task 2
    RuntimeManifestLoader.h          # Task 3
    RuntimeManifestLoader.cpp        # Task 3
    AssetRuntime.h                   # Tasks 4, 9, 10, 12, 13, 14, 17, 18, 20, 21
    AssetRuntime.cpp                 # Tasks 4, 8, 9, 10, 12, 13, 14, 17, 18, 20, 21
    AssetState.h                     # Task 7
    IAssetStateListener.h            # Task 16
    AssetRuntimeDebugCommands.h      # Task 23
    AssetRuntimeDebugCommands.cpp    # Tasks 23, 24, 25

GoogleTests/Tests/
    AssetRuntime/
        RuntimeManifestLoaderTest.cpp    # Task 6
        AssetStateMachineTest.cpp        # Task 11
        StageLifecycleTest.cpp           # Task 15
        EventNotificationTest.cpp        # Task 19
        DebugQueryTest.cpp               # Task 22
        DebugCommandsTest.cpp            # Task 26
```

## Key Design Notes

- **No DiaAssetCatalogue dependency** (SD-ARUN-008): RuntimeManifestLoader owns its own types and parsing. The pipeline transforms catalogue manifest → runtime manifest.
- **Single-threaded assumption** (SD-ARUN-001): No mutexes. Thread safety is the Module wrapper's responsibility.
- **Acknowledgement model** (SD-ARUN-009): 1:1 asset-to-consumer. One AcknowledgeAssetLoaded call per asset advances state.
- **Late-join pattern**: Late-registering listeners self-serve via GetStagedAssets() — no event replay.
- **Staged→Unloading shortcut**: If RequestStageUnload fires before consumer acknowledges, asset goes directly Staged→Unloading. Consumer must abort and ack unloaded.
- **PathStore integration**: Asset IDs become first-class path aliases. Skips (does not overwrite) already-registered aliases with a warning — `PathStore::RegisterToStore` asserts on duplicate keys.

## Session Notes

### 2026-05-05
- Plan created. 27 tasks across 6 features + 1 infrastructure task. All C++ (pure library).
- Build order: F1 (tasks 1–6) → F2 (tasks 7–11) → F3 (tasks 12–15) → F4 (tasks 16–19) → F5 (tasks 20–22) → F6 (tasks 23–26). Task 27 (vcxproj ref) done alongside task 6.
- Feature 6 introduces optional DiaAPI dependency. Requires understanding DiaAPI command registration pattern.
- Feature 1 complete (tasks 1–6): 12 tests passing. DiaAssetRuntime.lib builds clean.
  - `RuntimeAssetEntry.mDeployPath` uses `String512` not `FilePath` — FilePath is alias-based with String32 component limits and cannot hold arbitrary absolute paths. `ResolveAssetPath` returns `const String512*`.
  - PathStore alias registration uses the asset ID StringCRC as the alias and the full absolute deploy path as the path value.
  - `Json::Reader::parse()` is the correct JSON parse pattern (not CharReaderBuilder — that's the v2 API not used in this codebase).
  - `GetTempPathA` requires `<windows.h>` on MSVC — added to test file.
- Feature 2 complete (tasks 7–11): 9 tests passing. DiaAssetRuntime.lib builds clean.
  - `PathStore::RegisterToStore` has a `DIA_ASSERT` on duplicate keys — `RegisterPathAliases` must skip, not overwrite. Fixed during F2 test run.
  - TryTransition allows Staged→Unloading (cancellation path, documented in Key Design Notes).
  - F2 tests use unique PathStore alias (`test_arun_f2`) to avoid collision with F1 alias.
- Feature 3 complete (tasks 12–15): 13 tests passing. Clean first run.
  - RefCountTable reuses StateHashFunctor (same key type). InitRefCountTable called from LoadManifest alongside InitStateTable.
  - Double-unload clamping: skip (warn) if ref count already 0, rather than decrement below 0.
  - F3 tests use unique PathStore alias (`test_arun_f3`).
- Feature 4 complete (tasks 16–19): 9 tests passing. Clean first run.
  - IAssetStateListener::OnAssetReady takes String512 (not FilePath) consistent with F1 mDeployPath design.
  - Deferred removal: mIsDispatching flag guards UnregisterListener; FlushDeferredRemovals() applies after dispatch loop completes.
  - DynamicArrayC uses RemoveAt(index) / RemoveAll() (not Remove(index) / Reset()).
  - F4 tests use unique PathStore alias (`test_arun_f4`).
- Feature 5 complete (tasks 20–22): 9 tests passing.
  - HashTableC has no GetKeyByIndexConst — iterate mAssetTable for IDs then cross-ref mStateTable.
  - DynamicArrayC::IsFull() used for bounds guard instead of hardcoding 128.
  - RuntimeStageEntry::mAssetIds capacity is 64 (not 128), so stage overflow test capped at 64.
  - F5 tests use unique PathStore alias (`test_arun_f5`).
- Feature 6 complete (tasks 23–26): 10 tests passing. All 27 tasks Done.
  - DiaAPI has no `DiaAPI` class — uses free functions `Dia::API::RegisterCommand`, `GetCommand`, `Initialize`, `Shutdown`.
  - Command names are lowercase-hyphenated (e.g., `asset-runtime-get-loaded`).
  - subscribe_transitions: registers TransitionLogger (IAssetStateListener) via DiaLogger; push-stream delivery requires DiaDebugServer layer.
  - DiaAPI project reference added to DiaAssetRuntime.vcxproj ({1E7F6E13}).
  - F6 tests use unique PathStore alias (`test_arun_f6`).
  - Total: 62 DiaAssetRuntime tests across 6 features, all passing.

### 2026-05-06 — Post-review fixes
- Replaced `std::function` + `Json::Value` in `AssetRuntimeDebugCommands.h` with `ITransitionNotifier` abstract interface (PD-004 compliance).
- Removed `<thread>` from `AssetRuntime.h`; moved thread-owner check to `.cpp` using platform-native thread IDs (PD-004 compliance).
- Replaced static 256KB file buffer in `RuntimeManifestLoader.cpp` with heap allocation (re-entrancy fix).
- Added 2 capacity boundary tests (512 assets = success, 513 = failure) — total now 69 DiaAssetRuntime tests.
- Updated specs to match implementation: String512 return type for ResolveAssetPath, 128-capacity mAssetIds, OnAssetLoadFailed listener method, extra utility APIs documented.
- Full test suite: 4251 tests passing, zero regressions.
