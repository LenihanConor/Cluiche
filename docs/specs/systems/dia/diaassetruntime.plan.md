# Plan: DiaAssetRuntime

**Spec:** @docs/specs/systems/dia/diaassetruntime.md  
**Status:** In Progress  
**Started:** 2026-05-05  
**Last Updated:** 2026-05-05

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
| 7 | Implement AssetState enum + per-asset state storage | F2 | Not Started | haiku | `AssetState.h` — enum. Parallel `HashTableC<StringCRC, AssetState, 512>` in AssetRuntime, initialized to Registered after LoadManifest. |
| 8 | Implement state transition validation logic | F2 | Not Started | sonnet | Internal `TryTransition(assetId, targetState)` — validates allowed transitions, DiaLogger warns on invalid |
| 9 | Implement AcknowledgeAssetLoaded + AcknowledgeAssetUnloaded | F2 | Not Started | haiku | Public API methods that call TryTransition(Staged→Loaded) and (Unloading→Registered) |
| 10 | Implement GetAssetState + IsAssetReady | F2 | Not Started | haiku | Lookup by StringCRC. Return sentinel/false for unknown IDs + DiaLogger warning. |
| 11 | GoogleTest: Feature 2 | F2 | Not Started | sonnet | All valid transitions, all invalid transitions, acknowledge from wrong state, unknown ID queries, IsAssetReady, state init after manifest load |
| 12 | Implement per-asset ref count storage + GetAssetRefCount | F3 | Not Started | haiku | `HashTableC<StringCRC, unsigned int, 512>`, initialized to 0. Public query method. |
| 13 | Implement RequestStageLoad | F3 | Not Started | sonnet | Lookup RuntimeStageEntry, iterate member assets, increment ref count, transition Registered→Staged for 0→1 assets |
| 14 | Implement RequestStageUnload + edge cases | F3 | Not Started | sonnet | Decrement ref counts (clamp at 0), transition to Unloading when reaching 0. Handle unknown stage, double unload (warning). |
| 15 | GoogleTest: Feature 3 | F3 | Not Started | sonnet | Single stage load/unload, shared global asset across two stages, stage-scoped lifecycle, ref count correctness, double load idempotency, double unload clamping, unknown stage |
| 16 | Implement IAssetStateListener interface | F4 | Not Started | haiku | `IAssetStateListener.h` — abstract class with virtual dtor, OnAssetReady, OnAssetUnloading |
| 17 | Implement listener registration + deferred removal | F4 | Not Started | sonnet | `DynamicArrayC<IAssetStateListener*, 16>`, RegisterListener (reject dup), UnregisterListener (defer if dispatching), mIsDispatching flag |
| 18 | Wire event dispatch into state transitions | F4 | Not Started | sonnet | Dispatch OnAssetReady on Registered→Staged, OnAssetUnloading on →Unloading. Pass resolved path to OnAssetReady. |
| 19 | GoogleTest: Feature 4 | F4 | Not Started | sonnet | Single/multi listener, unregister during dispatch, duplicate registration, resolved path correctness, no events for already-loaded re-stage |
| 20 | Implement GetLoadedAssets + GetStagedAssets | F5 | Not Started | haiku | Iterate state table, collect matching IDs into caller-provided DynamicArrayC. Return total count. |
| 21 | Implement GetStageDependencies | F5 | Not Started | haiku | Lookup RuntimeStageEntry, copy mAssetIds into results. Return total count. Warn for unknown stage. |
| 22 | GoogleTest: Feature 5 | F5 | Not Started | sonnet | Correct results after stage load + acknowledge, overflow truncation, empty results, unknown stage |
| 23 | Register get_loaded, get_staged, get_state commands | F6 | Not Started | sonnet | `AssetRuntimeDebugCommands.h/.cpp` — DiaAPI handlers calling Feature 5 queries, JSON serialization |
| 24 | Register get_stage_deps + get_all_states commands | F6 | Not Started | sonnet | Full snapshot query + stage deps query → JSON. Error responses for unknowns. |
| 25 | Register subscribe_transitions (push stream) | F6 | Not Started | sonnet | Internal IAssetStateListener that forwards events to subscribed DiaAPI connections. Per-connection subscription. |
| 26 | GoogleTest: Feature 6 | F6 | Not Started | sonnet | Command registration, JSON response formats, valid/invalid asset/stage IDs, get_all_states completeness, subscribe event format |
| 27 | Add DiaAssetRuntime project reference to GoogleTests.vcxproj | — | Not Started | haiku | So GoogleTests can link DiaAssetRuntime and compile the test files |

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
- **PathStore integration**: Asset IDs become first-class path aliases. Overwrites existing aliases with a warning.

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
