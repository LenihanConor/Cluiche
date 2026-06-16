# Feature Spec: Asset Lifecycle Management — Tests

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia/dia.md |
| System | DiaAssetRuntime | @docs/specs/applications/dia/systems/diaassetruntime/diaassetruntime.md |
| Feature | Asset Lifecycle Management — Tests | (this document) |

## Summary

Fills the four coverage gaps left by the Asset Lifecycle Management implementation (task 12 of the parent feature spec). The existing 89-test suite covers state transitions, handler dispatch, stage lifecycle, auto-validation, reset, and debug queries well. This feature adds the missing coverage: the unload-while-loading assert, the `OnLoadFailed` reason string, and the `GetAssetScope` / `GetAssetStageId` query APIs.

## Problem

Four behaviours implemented in `AssetRuntime` have no test coverage:

1. `RequestStageUnload` while an asset is `Loading` fires a `DIA_ASSERT` — never exercised by a test
2. The `reason` string passed to `OnLoadFailed` is stored but never verified as retrievable
3. `GetAssetScope` has no tests (returns `AssetScope::Global` vs `AssetScope::Stage`)
4. `GetAssetStageId` has no tests (returns the stage ID that owns a given asset, or empty for global assets)

## Acceptance Criteria

### Unload-While-Loading Guard

1. **AC1**: A test calls `RequestStageLoad` with a deferred handler (does not immediately call back), then calls `RequestStageUnload` on the same stage — verifies that `DIA_ASSERT` fires (death test)
2. **AC2**: A test confirms that `RequestStageUnload` on a stage where all assets are in `Loaded` state does NOT assert — normal path is unaffected

### OnLoadFailed Reason String

3. **AC3**: A test registers a handler that calls `OnLoadFailed(assetId, "file not found")` — verifies the asset transitions to `Failed` state AND that a query API (e.g. `GetAssetFailureReason`) returns `"file not found"`
4. **AC4**: If `GetAssetFailureReason` does not exist on `AssetRuntime`, AC3 is satisfied by verifying the asset is in `Failed` state and the reason string was received by a test observer — and an Open Question is raised for whether the reason string needs to be publicly queryable

### GetAssetScope

5. **AC5**: A test loads a manifest with both `"global"` and `"stage"` scoped assets — verifies `GetAssetScope` returns `AssetScope::Global` for global assets and `AssetScope::Stage` for stage-scoped assets
6. **AC6**: `GetAssetScope` for an unknown asset ID returns a defined sentinel (not a crash)

### GetAssetStageId

7. **AC7**: A test loads a manifest where an asset belongs to a specific stage — verifies `GetAssetStageId` returns the correct stage StringCRC for that asset
8. **AC8**: `GetAssetStageId` for a global asset returns an empty `StringCRC` (no owning stage)
9. **AC9**: `GetAssetStageId` for an unknown asset ID returns an empty `StringCRC` (not a crash)

## Files Affected

| File | Change |
|------|--------|
| `Cluiche/Tests/GoogleTests/DiaAssetRuntime/TestAssetStateMachine.cpp` | Add AC1 (death test), AC2 (normal unload does not assert) |
| `Cluiche/Tests/GoogleTests/DiaAssetRuntime/TestAssetStateMachine.cpp` | Add AC3–AC4 (reason string on failed load) |
| `Cluiche/Tests/GoogleTests/DiaAssetRuntime/TestDebugQueryAPI.cpp` | Add AC5–AC9 (GetAssetScope, GetAssetStageId) |
| `Dia/DiaAssetRuntime/AssetRuntime.h` | Add `GetAssetFailureReason(assetId)` returning `const char*`; add `mAssetFailureReasons` storage |
| `Dia/DiaAssetRuntime/AssetRuntime.cpp` | Implement `GetAssetFailureReason`; store reason in `OnLoadFailed`; clear on retry/unload |

## Tasks

| # | Task | Status |
|---|------|--------|
| 1 | Add `GetAssetFailureReason(assetId)` to `AssetRuntime.h/.cpp` — store reason string in `OnLoadFailed`, clear on retry/unload (resolves OQ-1) | Not Started |
| 2 | Add death test: `RequestStageUnload` while asset is `Loading` fires assert (AC1) | Not Started |
| 3 | Add test: `RequestStageUnload` on fully-loaded stage does not assert (AC2) | Not Started |
| 4 | Add test: `OnLoadFailed` reason string stored/observable (AC3–AC4) | Not Started |
| 5 | Add tests: `GetAssetScope` returns correct value for global/stage assets and unknown ID (AC5–AC6) | Not Started |
| 6 | Add tests: `GetAssetStageId` returns correct stage for owned asset, empty for global, empty for unknown (AC7–AC9) | Not Started |
| 7 | Run `dia run googletest --filter="DiaAssetRuntime*"` — all pass | Not Started |

## Open Questions

| # | Question | Resolution |
|---|----------|------------|
| OQ-1 | Does `AssetRuntime` expose `GetAssetFailureReason(assetId)`? If not, should it be added, or is the reason string only observable via `DiaLogger` output? | Resolved — add `GetAssetFailureReason(assetId)` returning `const char*`. Reason string stored per-asset; cleared on retry or unload. |

## Design Decisions

| Decision | Rationale |
|----------|-----------|
| Death test for unload-while-loading | `DIA_ASSERT` is a `GTEST_DEATH_TEST` — this is the standard pattern in the suite |
| Reason string stored per-asset | Even if not currently queryable, the test validates the contract described in AC15 of the parent spec — the reason must reach some observable surface |
| Scope/StageId tests added to `TestDebugQueryAPI.cpp` | These are read-only query APIs, consistent with the existing test file's purpose |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | All test asset/stage IDs use `StringCRC`. `GetAssetStageId` returns `StringCRC`. |
| PD-004 | No STL in public APIs | No new public API introduces STL types. `GetAssetFailureReason` would return `const char*` if added. |
| PD-007 | C++20 required | Tests use standard GTest macros; no C++23 features. |
| AD-002 | No STL in public APIs | Same as PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | All new queries remain in `Dia::AssetRuntime::` namespace. |
| SD-ARUN-001 | No DiaApplicationFlow dependency | Tests depend only on `DiaAssetRuntime` and `DiaCore`. |
| SD-ARUN-006 | No content loading in DiaAssetRuntime | Tests use mock handlers only — no real I/O. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | AC1 — Death test | Does the codebase use `EXPECT_DEATH` or `ASSERT_DEATH` for `DIA_ASSERT` tests? | `DIA_ASSERT` calls `__debugbreak()` which terminates the child process when no debugger is attached. `EXPECT_DEATH({ ... }, "")` with an empty string matcher is the established pattern (used throughout `TestDynamicArrayC.cpp`). Only fires in `DEBUG` builds — tests must run under Debug config (the default). |
| 2 | AC3–AC4 | Is `GetAssetFailureReason` already on `AssetRuntime`, or does the reason string only go to `DiaLogger`? | Not present — `OnLoadFailed` logs to `DIA_LOG_ERROR` and transitions state but does not store the reason. Decision: add `GetAssetFailureReason(assetId)` returning `const char*` (null if not failed or no reason). Reason stored per-asset in `mAssetFailureReasons` table. |
| 3 | AC5–AC6 | What does `AssetScope` look like — enum class with `Global`/`Stage` values? | `enum class AssetScope { kGlobal, kStage }` in `Dia/DiaAssetRuntime/AssetScope.h`. Unknown asset sentinel: returns `kGlobal` with a warning log (confirmed from `GetAssetScope` implementation). |
| 4 | AC7–AC9 | For a global asset (scope = global), what does `GetAssetStageId` return — empty StringCRC, or a sentinel? | Returns empty `StringCRC` (Value() == 0) for global assets and unknown IDs — confirmed from `AssetRuntime.h` comment: "empty StringCRC if global or not found". |
| 5 | Coverage | Are there any other methods on `AssetRuntime` with zero test coverage besides the four gaps identified? | Audit found `GetAssetScope` and `GetAssetStageId` as the only untested query methods. Thread-safety assertions are intentionally out of scope (single-threaded architecture). |

## Status

`Approved` — Plan: @docs/specs/applications/dia/systems/diaassetruntime/asset-lifecycle-management-tests.plan.md
