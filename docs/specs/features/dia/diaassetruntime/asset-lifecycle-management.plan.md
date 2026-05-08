# Plan: Asset Lifecycle Management

**Spec:** @docs/specs/features/dia/diaassetruntime/asset-lifecycle-management.md  
**Status:** Done  
**Started:** 2026-05-08  
**Last Updated:** 2026-05-08

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `IAssetTypeHandler.h` with `IAssetTypeHandler` and `IAssetLoadCallback` interfaces | TestHandlerDispatch_HandlerCalledOnStageLoad | Done | sonnet | New file, additive — no existing code breaks |
| 2 | Rewrite `AssetState.h`: replace `Registered`/`Unloading` with `Null`/`Loading`/`Failed`/`Unloaded` | TestStateInit_AllAssetsStagedAfterLoad | Done | sonnet | Breaking change — landed atomically with tasks 3-4 |
| 3 | Rewrite `AssetRuntime.h/.cpp`: handler registry, dispatch in `RequestStageLoad`, `IAssetLoadCallback` impl, `RetryAssetLoad`, `GetLoadProgress`, assert on unload-while-loading, remove listener pattern | TestHandlerDispatch, TestRetryAssetLoad, TestGetLoadProgress, TestUnloadWhileLoadingAsserts | Done | opus | Core rewrite — biggest task |
| 4 | Remove `IAssetStateListener.h` | — | Done | haiku | Mechanical deletion after task 3 |
| 5 | Rewrite `AssetServiceModule` to use handler registration + deferred start (remove `IAssetStateListener` impl) | — | Done | sonnet | DefaultAssetHandler immediately calls OnLoadComplete |
| 6 | Update `DiaAssetRuntime.vcxproj`: add `IAssetTypeHandler.h`, remove `IAssetStateListener.h` | — | Done | haiku | Mechanical |
| 7 | Rewrite `TestAssetStateMachine.cpp` for new states and handler dispatch; add handler dispatch tests | All DiaAssetRuntime tests pass | Done | sonnet | All 6 test files rewritten; 4348/4349 pass (1 pre-existing unrelated failure) |

## Session Notes

### 2026-05-08
- Spec approved (Steps 3+4 already complete)
- Plan created from code audit of AssetRuntime.h/.cpp, AssetServiceModule, existing tests
- Key insight: `Registered` maps to `Staged` (post-manifest), `Unloading` becomes `Unloaded`
- Listener dispatch pattern fully replaced by handler dispatch — no co-existence needed
- Tasks 2-4 must land together (enum change breaks all call sites)
- All 7 tasks implemented and verified
- Fixed state transition ordering bug in DispatchLoad: must transition to Loading before handler lookup so no-handler path can reach Failed
- Full test suite: 4348 passed, 1 failed (pre-existing `DiaGameFormat.CluicheTestDataFiles_ComposeFromGameFile_Succeeds` — unrelated)
- CluicheTest builds and launches clean
- Not committed per user instruction
