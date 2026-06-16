# Plan: Asset Lifecycle Management — Tests

**Spec:** @docs/specs/applications/dia/systems/diaassetruntime/asset-lifecycle-management-tests.md
**Status:** Not Started
**Started:** —
**Last Updated:** 2026-05-17

---

## Implementation Patterns

### GetAssetFailureReason — storage pattern

`AssetRuntime` already uses `HashTableC<StringCRC, T, StateHashFunctor, kMaxAssets, kTableSize>` for state and ref-count tables. Add a parallel table for failure reasons using `String512` as the value type (same size as deploy path strings already stored). Clear the entry in `OnLoadComplete`, `RetryAssetLoad`, and `DispatchUnload`.

```cpp
// AssetRuntime.h — public query
const char* GetAssetFailureReason(const Dia::Core::StringCRC& assetId) const;

// AssetRuntime.h — private member (alongside mStateTable / mRefCountTable)
typedef Dia::Core::Containers::HashTableC<
    Dia::Core::StringCRC,
    Dia::Core::Containers::String512,
    StateHashFunctor,
    kMaxAssets,
    kStateTableSize> FailureReasonTable;

FailureReasonTable mFailureReasonTable;

// AssetRuntime.cpp — OnLoadFailed
void AssetRuntime::OnLoadFailed(const Dia::Core::StringCRC& assetId, const char* reason)
{
    DIA_LOG_ERROR("AssetRuntime", "Asset '%s' load failed: %s", assetId.AsChar(), reason ? reason : "unknown");
    if (reason)
    {
        Dia::Core::Containers::String512* entry = mFailureReasonTable.TryGetItem(assetId);
        if (entry) entry->Set(reason);
        else { Dia::Core::Containers::String512 s; s.Set(reason); mFailureReasonTable.Insert(assetId, s); }
    }
    TryTransition(assetId, AssetState::Failed);
}

// Clear on retry/unload
mFailureReasonTable.Remove(assetId);  // in RetryAssetLoad + DispatchUnload
```

### Death test pattern

Established pattern from `TestDynamicArrayC.cpp`. Use `EXPECT_DEATH` with empty string matcher. Requires `DeferredHandler` (already defined in `TestAssetStateMachine.cpp` anonymous namespace) to hold assets in `Loading` state:

```cpp
TEST_F(AssetStateMachineTest, UnloadWhileLoading_Asserts)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_deathtest.json"));
    DeferredHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    // asset.alpha is now Loading — unload should assert
    EXPECT_DEATH({ runtime.RequestStageUnload(Dia::Core::StringCRC("stage.s1")); }, "");
}
```

### Scope/StageId tests — reuse F5 manifest fixture

`TestDebugQueryAPI.cpp` already has a manifest with both `"global"` and `"stage"` scoped assets (`WriteTempFileF5` helper, `kF5Alias`). Add tests to the existing `DebugQueryAPITest` fixture — no new manifest JSON needed.

```cpp
TEST_F(DebugQueryAPITest, GetAssetScope_GlobalAsset_ReturnsGlobal)
{
    // manifest already loaded in SetUp — asset.beta is scope:"global"
    EXPECT_EQ(runtime.GetAssetScope(Dia::Core::StringCRC("asset.beta")),
              Dia::AssetRuntime::AssetScope::kGlobal);
}
```

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `GetAssetFailureReason` to `AssetRuntime.h` — declare public query + private `FailureReasonTable` member | Build succeeds | Not Started | sonnet | Parallel to `mStateTable`; `String512` value type |
| 2 | Implement `GetAssetFailureReason` in `AssetRuntime.cpp` — store in `OnLoadFailed`, clear in `RetryAssetLoad` + `DispatchUnload` | Build succeeds | Not Started | sonnet | Depends on task 1 |
| 3 | Add death test `UnloadWhileLoading_Asserts` to `TestAssetStateMachine.cpp` (AC1) | `EXPECT_DEATH` fires | Not Started | sonnet | Uses existing `DeferredHandler` mock + `LoadTwoAssetRuntime` |
| 4 | Add test `UnloadAfterLoaded_DoesNotAssert` to `TestAssetStateMachine.cpp` (AC2) | Passes cleanly | Not Started | haiku | Normal path — `ImmediateHandler`, load, then unload |
| 5 | Add test `OnLoadFailed_ReasonStringStoredAndQueryable` to `TestAssetStateMachine.cpp` (AC3–AC4) | `GetAssetFailureReason` returns `"file not found"` | Not Started | sonnet | Depends on tasks 1–2; uses `FailingHandler` mock already in file |
| 6 | Add `GetAssetScope_*` tests to `TestDebugQueryAPI.cpp` (AC5–AC6) — global asset returns `kGlobal`, stage asset returns `kStage`, unknown returns `kGlobal` sentinel | 3 assertions pass | Not Started | haiku | Reuse existing F5 manifest fixture |
| 7 | Add `GetAssetStageId_*` tests to `TestDebugQueryAPI.cpp` (AC7–AC9) — owned asset returns correct stage CRC, global returns empty CRC, unknown returns empty CRC | 3 assertions pass | Not Started | haiku | Reuse existing F5 manifest fixture |
| 8 | Run `dia run googletest --filter="DiaAssetRuntime*"` — all pass | All 96+ tests green | Not Started | haiku | Report pass count + any failures |

---

## Session Notes

### Spec Decisions Summary

**Binding constraints from spec chain (Platform → App → System → Feature):**
- PD-001: All asset/stage IDs use `StringCRC` — `GetAssetStageId` returns `StringCRC`, `GetAssetFailureReason` is keyed by `StringCRC`
- PD-004 / AD-002: No STL in public APIs — `GetAssetFailureReason` returns `const char*`, storage uses `HashTableC<StringCRC, String512>`
- SD-ARUN-001: No DiaApplicationFlow dependency — tests depend only on `DiaAssetRuntime` + `DiaCore`
- SD-ARUN-006: No content loading in DiaAssetRuntime — all tests use mock handlers, no real I/O
- `DIA_ASSERT` fires only in DEBUG builds (`__debugbreak`) — death tests must run under Debug config (default for `dia run googletest`)
- `AssetScope` is `enum class { kGlobal, kStage }` — unknown asset sentinel is `kGlobal` (with warning log)
- `GetAssetStageId` returns empty `StringCRC` (Value() == 0) for global and unknown assets
- Failure reason storage uses `HashTableC` parallel to `mStateTable`; cleared on retry and unload
- `DeferredHandler`, `ImmediateHandler`, `FailingHandler` mocks already exist in `TestAssetStateMachine.cpp` anonymous namespace — reuse them
- `TestDebugQueryAPI.cpp` uses `kF5Alias` + `WriteTempFileF5` + `DebugQueryAPITest` fixture — new scope/stageId tests slot directly into this file
