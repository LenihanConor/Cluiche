**Spec:** Backlog item #10 — Eliminate remaining cross-PU statics
**Target:** `AssetServiceModule::GetStatic()` / `sInstance`
**Status:** Done

---

## Summary

Remove `AssetServiceModule::GetStatic()` by providing proper access patterns for each consumer:
- MainPU consumers → `ModuleRef<AssetServiceModule>` (same PU)
- SimPU consumers → `ServiceStream<AssetLoadStatus>` (new Main→Sim stream)
- RenderPU consumers → fold asset load state into existing `MainToRenderFrame`

Additionally, move `TestAssetRuntimeStageModule` from MainPU to SimPU so it exercises the same poll-via-stream pattern that real game modules will use.

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Define `AssetLoadStatus` payload type | Compiles | Not Started | haiku | New header in `CluicheGameBaseline/Types/`. Fields: `StringCRC stageId`, `StageLoadState state`. |
| 2 | Add `ServiceStream<AssetLoadStatus>` writer to `AssetServiceModule` | Compiles | Not Started | haiku | Add `ServiceStreamWriter<AssetLoadStatus>` member, `OnConnectStreams`, `Register()` in DoStart. Publish state each tick from DoUpdate or on state change. |
| 3 | Add `ServiceStream<AutomationService>` for cross-PU automation access | Compiles | Not Started | sonnet | `AutomationModule` (MainPU) publishes `AutomationService*` via `ServiceStreamWriter<AutomationService>`. SimPU consumers read via `ServiceStreamReader<AutomationService>`. Needed because `TestStageModuleBase` moves to SimPU — `ModuleRef<AutomationModule>` won't resolve cross-PU. |
| 4 | Migrate `TestStageModuleBase` from `ModuleRef<AutomationModule>` to `ServiceStreamReader<AutomationService>` | Compiles, existing tests pass | Not Started | sonnet | Replace `mAutomationRef` with `ServiceStreamReader`. Update `DoStart`/`DoStop`/`GetAutomationService` to use `IsAvailable()` + `Get()`. Add `OnConnectStreams` override. Add `consumes` channel to every stage manifest that uses a TestStageModuleBase subclass. |
| 5 | Declare both new streams in `cluiche_main.diaapp` | Manifest validates | Not Started | haiku | Two entries: `AssetLoadStatus` ServiceStream (MainPU provides), `AutomationService` ServiceStream (MainPU provides). Add `provides` channels to AssetServiceModule + AutomationModule entries. |
| 6 | Fold asset load state into `MainToRenderFrame` | Compiles | Not Started | haiku | Add `StageLoadState assetStageLoadState` field to `MainToRenderFrame`. Populate from `ModuleRef<AssetServiceModule>` in `MainStateProducerModule::DoUpdate`. |
| 7 | Add `ModuleRef<AssetServiceModule>` to `MainStateProducerModule` | Compiles | Not Started | haiku | Header already includes `AutomationModule` via ModuleRef pattern — same approach. Add dep in manifest entry. |
| 8 | Migrate `AssetRuntimeHUDModule` (RenderPU) | HUD still draws load state | Not Started | sonnet | Replace `AssetServiceModule::GetStatic()` with reading `MainToRenderFrame.assetStageLoadState` from existing `mMainStateInput` StreamReader. Remove `#include "Modules/AssetServiceModule.h"`. |
| 9 | Move `TestAssetRuntimeStageModule` to SimPU | Stage passes in pytest | Not Started | sonnet | Change `kAllowedPUs` to `kSim`. Move manifest entry from MainPU to SimPU in `asset_runtime_stage.diaapp`. Add `ServiceStreamReader<AssetLoadStatus>` for polling. Add `consumes` channel for `AssetLoadStatus` + `AutomationService`. Replace `GetStatic()` calls with stream reads. |
| 10 | Migrate `DummyLevelModule` (SimPU) | DummyStage starts correctly | Not Started | sonnet | Replace `AssetServiceModule::GetStatic()` in `DoStart()` with `ServiceStreamReader<AssetLoadStatus>`. Add `consumes` channel in `dummy_stage.diaapp`. Remove `#include "Modules/AssetServiceModule.h"`. |
| 11 | Delete `GetStatic()` / `sInstance` from `AssetServiceModule` | Compiles, no callers remain | Not Started | haiku | Remove static member, the accessor, and set/clear in DoStart/DoStop. |
| 12 | Build + run full test suite | 5475+ tests pass, pytest smoke passes | Not Started | sonnet | `dia run googletest` + `dia orchestrate cluichetest_smoke`. |

---

## Key Decisions

- **ServiceStream** (not EventStream) for Main→Sim because SimPU consumers need to poll state in `DoStart()` before event processing begins. ServiceStream provides a stable lifecycle handle.
- **ServiceStream for AutomationService** (option B) rather than duplicating AutomationModule on SimPU. AutomationModule owns the single `AutomationService` instance — duplicating it would create two competing checkpoint registries. Instead, SimPU modules get a read handle to the MainPU-owned service via ServiceStream.
- **MainToRenderFrame** for Main→Render because the stream + producer already exist — zero new plumbing.
- **TestAssetRuntimeStageModule moves to SimPU** to validate the real consumer pattern (Sim polls completion via stream). This exercises the same code path all future game modules will use.

---

## Dependency Order

```
T1 (AssetLoadStatus type)
T2 (AssetLoadStatus ServiceStream writer)  ← depends on T1
T3 (AutomationService ServiceStream)       ← independent of T1/T2
T4 (TestStageModuleBase migration)         ← depends on T3
T5 (manifest declarations)                 ← depends on T2, T3
T6 (MainToRenderFrame field)               ← independent
T7 (MainStateProducer ModuleRef)           ← independent
T8 (AssetRuntimeHUDModule)                 ← depends on T6, T7
T9 (TestAssetRuntime → SimPU)              ← depends on T2, T4, T5
T10 (DummyLevelModule)                     ← depends on T2, T5
T11 (delete GetStatic)                     ← depends on T8, T9, T10
T12 (final verification)                   ← depends on T11
```

Parallelisable groups:
- **Group 1:** T1, T3, T6, T7 (all independent)
- **Group 2:** T2, T4 (each depends on one from group 1)
- **Group 3:** T5, T8 (depends on group 2)
- **Group 4:** T9, T10 (depends on group 3)
- **Sequential tail:** T11 → T12

---

## Risks

- **Thread safety of `AutomationService*` across PUs (T3/T4):** `RegisterCheckpoint()` and `UnregisterCheckpoints()` are called from SimPU but `AutomationService` lives on MainPU. These calls happen during `DoStart`/`DoStop` (not per-frame), and stage transitions are synchronised — but document this as a contract. If contention appears, add a mutex to `RegisterCheckpoint`/`UnregisterCheckpoints`.
- **`Geometry2DTestStageModule` is already on SimPU** and uses `TestStageModuleBase` — it will also benefit from T4 (AutomationService stream). Confirm its manifest has the `consumes` channel added.
