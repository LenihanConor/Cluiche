# System Spec: Async Stage Asset Loading

## Parent Application
@docs/specs/applications/cluichetest.md

## Purpose

Async Stage Asset Loading establishes how CluicheTest stages load their assets off the main thread using the `Dia::Core::Threading::JobSystem` worker pool, while keeping graphics-API-affine work (`sf::Texture` upload) on the main thread. The system makes stage-grain async loading the canonical pattern for every CluicheTest stage and pulls JobSystem from "tested but unused" into a production-driven engine subsystem owned by a Module.

DummyStage is the first consumer and serves as the reference implementation. The same pattern (Module declares its stage assets in the catalogue → `DoStart` issues `RequestStageLoad` → polls `IsLoadComplete`) applies unchanged to every future stage.

## Responsibilities

- Own the lifecycle of `Dia::Core::Threading::JobSystem` (Initialize on app start, Shutdown on app stop) via a dedicated `JobSystemModule`
- Define the **two-phase texture load** contract: worker thread decodes `sf::Image`; main thread creates `sf::Texture` from the decoded image
- Provide a per-frame main-thread pump (`TextureHandler::Tick()`) that drains decoded-but-not-uploaded textures and fires `IAssetLoadCallback::OnLoadComplete/OnLoadFailed`
- Establish the stage-driven load pattern: stage modules call `AssetServiceModule::RequestStageLoad(<stageId>)` from `DoStart`, return `kLoading` while polling `IsLoadComplete`, then `kReady` (or `kFailed`)
- Convert DummyStage from "textures pre-loaded into stage.global" to the new pattern as the reference implementation
- Preserve `AssetRuntime`'s single-thread (owner thread) contract end-to-end — async work is contained to handlers, callbacks fire on the owner thread

## Non-Responsibilities

- **JobSystem itself** — `Dia::Core::Threading::JobSystem` already exists in DiaCore; this system consumes it, does not modify it
- **Asset catalogue / runtime / pipeline** — the Asset Pipeline system already provides catalogue, manifest format, PathStore aliasing, and stage-grain `RequestStageLoad`/`IsLoadComplete`; this system reuses them as-is
- **Other handlers' async strategies** — only `TextureHandler` is converted in this system. UIHandler and future handlers may adopt the same two-phase pattern when needed but are out of scope here
- **Loading screen UX** — `LoadingScreenModule` already covers visible loading state during transitions (see ApplicationFlow SD-004); this system only ensures `DoStart` returns `kLoading` long enough to be visible
- **Cross-PU asset reads** — Sim and Render PUs already read `TextureHandler` via the existing `std::shared_mutex`; this system verifies the contract still holds, doesn't redesign it
- **Thread-pool sizing or scheduling policy** — JobSystem decides how many workers and how to schedule

## Architecture

### Lifecycle Module

A new `JobSystemModule` (in `Cluiche/CluicheGameBaseline/Modules/`) wraps the JobSystem singleton, following the exact shape of `TimeServerModule`:

```
class JobSystemModule : public Dia::ApplicationFlow::Module {
    Dia::ApplicationFlow::StartResult DoStart() override;   // JobSystem::Initialize(0)
    void                              DoUpdate(float) override;  // empty
    Dia::ApplicationFlow::StopResult  DoStop() override;    // JobSystem::Shutdown()
};
```

Registered on MainPU with `"stages": ["all"]`, declared **before** `AssetServiceModule` so it's started first. Modules that submit jobs from `DoStart` add `JobSystemModule` to their `dependencies`.

### Two-Phase Texture Load

`TextureHandler::Load` becomes:

1. **Request time (caller thread = AssetRuntime owner thread):**
   - Take `mMutex`, check `mPathToId` for cache hit. If hit → fire `OnLoadComplete` synchronously, return.
   - Else: enqueue a decode job via `JobSystem::CreateJob` + `Run`. Job carries `{assetId, resolvedPath, callback}`.
2. **Worker thread (decode):**
   - `sf::Image image; image.loadFromFile(path)` — pure CPU, thread-safe per SFML.
   - Push `{assetId, path, std::move(image), callback, success/failure}` onto `mPendingUploads` (mutex-guarded queue).
3. **Main thread (`TextureHandler::Tick()`, called from `AssetServiceModule::DoUpdate`):**
   - Drain `mPendingUploads`. For each:
     - On success: allocate `sf::Texture`, `texture->loadFromImage(image)`, register IDs in `mPathToId` / `mIdToTexture` / `mAssetToTextureId` under `mMutex`, fire `OnLoadComplete(assetId)`.
     - On failure: fire `OnLoadFailed(assetId, reason)`.

Callbacks fire on the AssetRuntime owner thread (Main), preserving `AssertOwnerThread()`.

### Stage-Driven Load Pattern (canonical for all stages)

The load is **transition-driven**. `AssetServiceModule` (MainPU) watches the Application's current stage in its `DoUpdate`; on transition it issues `RequestStageLoad(newStageId)` to the runtime — same thread, no marshalling. Stage-scoped Modules (any PU) only **poll completion** via a thread-safe static accessor.

This is the canonical pattern for every future stage. Stage code is mechanical:

```cpp
StartResult ForestLevelModule::DoStart() {
    if (AssetServiceModule::GetStatic()->IsStageLoadComplete(StringCRC("ForestLevel"))) {
        return StartResult::kReady;
    }
    return StartResult::kLoading;
}
```

The stage **declares** its assets via the catalogue (`"scope": "stage"`) — that's the part it owns. The **trigger** is infrastructure and is automatic. Stage authors cannot forget to request their load, cannot request the wrong stage ID, and cannot introduce cross-PU thread bugs, because none of those concerns exist in their code.

`AssetServiceModule::DoStart` is narrowed: it loads `stage.global` only. Per-stage assets load when the framework transitions to that stage.

### Threading Boundary Diagram

Per-stage transition trigger (Main only; Sim never calls into AssetService):

```
[Main thread]                            [Sim thread]
Application::TransitionTo("DummyStage")
AssetServiceModule::DoUpdate
   ├ stage = App->GetCurrentStage()
   ├ if (stage != mLastStage) {
   │     mRuntime.RequestStageLoad(stage)   // same thread, no assert
   │     mLastStage = stage
   │ }
   └ TextureHandler::Tick()
                                        DummyLevelModule::DoStart
                                          ├ AssetServiceModule::GetStatic()
                                          │   ->IsStageLoadComplete("DummyStage")
                                          │   (atomic read, no lock contention)
                                          ├ false -> return kLoading
                                          └ true  -> return kReady
```

Per-texture two-phase load (within a stage's load):

```
[Main thread]                            [JobSystem worker N]
TextureHandler::Load (called from Main, AssetRuntime owner)
   ├ cache hit? -> callback (sync, on Main)
   └ JobSystem::CreateJob/Run --------->  sf::Image::loadFromFile
                                          push to mPendingUploads
TextureHandler::Tick() (per frame, called from AssetServiceModule::DoUpdate)
   ├ drain mPendingUploads
   ├ sf::Texture::loadFromImage  (Main, GL-affine)
   └ callback->OnLoadComplete    (Main, AssetRuntime owner)
```

### Manifest Wiring

`cluiche.diaapp` adds `JobSystemModule` to MainPU **before** `AssetService`:

```json
{ "instance_id": "JobSystem",    "type_id": "JobSystemModule",     "stages": ["all"], "dependencies": [] },
{ "instance_id": "AssetService", "type_id": "AssetServiceModule",  "stages": ["all"], "dependencies": ["Logger", "JobSystem"] }
```

`dummy_stage.diaapp` does NOT add `AssetService` to `DummyLevel`'s `dependencies` — cross-PU manifest dependencies aren't supported, and aren't needed (AssetService is `"stages": ["all"]` and starts before any stage-scoped module). Stage modules access AssetService via `AssetServiceModule::GetStatic()` only (per SD-011).

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| JobSystem Module | New `JobSystemModule` wrapping `JobSystem::Initialize/Shutdown`; manifest wiring on MainPU | [jobsystem-module.md](../../features/cluichetest/async-asset-loading/jobsystem-module.md) | Approved |
| TextureHandler Two-Phase Load | Refactor `TextureHandler::Load` into worker decode + main-thread upload; add `Tick()`; `AssetServiceModule::DoUpdate` calls it | [texturehandler-two-phase-load.md](../../features/cluichetest/async-asset-loading/texturehandler-two-phase-load.md) | Approved |
| AssetService Stage-Transition Watcher | `AssetServiceModule::DoUpdate` watches `Application::GetCurrentStage()` and auto-issues `RequestStageLoad` on transition; adds thread-safe `GetStatic()`, `IsStageLoadComplete`, `GetStageLoadState` accessors | [assetservice-stage-watcher.md](../../features/cluichetest/async-asset-loading/assetservice-stage-watcher.md) | Approved |
| DummyStage Asset Deployment | Ensure `Stages/DummyStage/World/Textures/test_*.png` exist on disk; create `dummy_stage.diaapp` if missing; remove DummyStage textures from global pre-load | [dummystage-asset-deployment.md](../../features/cluichetest/async-asset-loading/dummystage-asset-deployment.md) | Approved |
| DummyStage Async Load Consumer | `DummyLevelModule::DoStart` polls `AssetServiceModule::GetStatic()->IsStageLoadComplete("DummyStage")`; replaces placeholder `mStartLoadRequested`. No cross-PU method calls. | [dummystage-async-load-consumer.md](../../features/cluichetest/async-asset-loading/dummystage-async-load-consumer.md) | Approved |

## Platform Primitives Used

- **DiaCore/Threading** — `Dia::Core::Threading::JobSystem` (`CreateJob`, `Run`, `IsComplete`)
- **DiaApplicationFlow** — `Module` base class, `ModuleRef<T>`, `StartResult { kLoading, kReady, kFailed }`
- **DiaSFML** — `sf::Image`, `sf::Texture`, `TextureHandler`
- **DiaAssetRuntime** — `AssetRuntime::RequestStageLoad`, `AssetRuntime::IsLoadComplete`, `IAssetLoadCallback`
- **DiaAssetCatalogue** — already declares `texture.test_red/blue/green` as `"scope": "stage", "stage_name": "DummyStage"`
- **DiaCore/CRC** — `StringCRC` for stage IDs, asset IDs

## Dependencies on Other Systems

**Required:**
- **ApplicationFlow** (sibling) — provides `JobSystemModule` and `AssetServiceModule` registration slots, `DummyLevelModule` lifecycle, and the `"stages": ["all"]` vs stage-scoped distinction
- **Asset Pipeline** (sibling) — provides catalogue, `.diastage` manifests, PathStore aliasing, and stage-grain async API; this system consumes it unchanged
- **DiaCore/Threading** (engine) — `JobSystem` API
- **DiaSFML** (engine) — `sf::Image` / `sf::Texture` decode/upload split

**Sibling Systems:**
- **CluicheTestScenarios** — adds an e2e scenario that proves `DoStart` actually returns `kLoading` for multiple frames and the textures render after upload completes

## Out of Scope

- **Asset streaming during gameplay** — only stage-transition loads. Hot-streaming individual assets mid-stage is a future system
- **Texture compression / GPU residency policy** — out of scope; same `sf::Texture` allocation as today
- **Audio / mesh / other handler async** — only `TextureHandler` in this iteration
- **JobSystem priority / parent-child dependencies** — flat job-per-texture is sufficient; advanced JobSystem features are not used here
- **Per-thread `sf::Context`** — explicitly rejected (see SD-002 rationale)
- **Manifest format changes** — none; existing catalogue/`.diastage`/`.diaapp` schemas are sufficient

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | JobSystem lifecycle owned by a `JobSystemModule`, registered with `"stages": ["all"]` on MainPU | Codebase convention is "engine singleton wrapped by a Module" (see TimeServerModule, AssetServiceModule). Module ordering provides clean `dependencies` declaration and stage-survival lifetime. Avoids free-standing init in `Main.cpp`. | All features | Accepted | Yes |
| SD-002 | Two-phase texture load: `sf::Image::loadFromFile` on worker, `sf::Texture::loadFromImage` on main thread | `sf::Texture` creation is GL-affine. Full off-thread upload needs per-thread `sf::Context` and is driver-fragile. Two-phase parallelises the decode (the expensive part) while keeping GL on Main. Standard AAA-engine pattern. | TextureHandler + future image handlers | Accepted | Yes |
| SD-003 | `AssetServiceModule::DoUpdate` owns the per-frame call to `TextureHandler::Tick()` | AssetServiceModule already coordinates handlers and runs on Main. No new module needed. Single line of new code. | TextureHandler integration | Accepted | Yes |
| SD-004 | `AssetRuntime` remains single-thread (owner thread); async work is contained to handlers; callbacks marshalled back to owner thread | Honors AssetRuntime's existing `AssertOwnerThread()` invariant and the architecture-doc constraint that AssetRuntime not depend on graphics modules. Localises threading to where it matters. | All features | Accepted | Yes |
| SD-005 | Stages declare their assets via the existing catalogue (`"scope": "stage"`); no new manifest format | Catalogue already supports stage-scoped assets. Reuse, don't rebuild. | DummyStage feature + future stages | Accepted | Yes |
| SD-006 | Stage asset loads are **transition-driven**: `AssetServiceModule::DoUpdate` (MainPU) watches `Application::GetCurrentStage()` and issues `RequestStageLoad` automatically on stage change. Stage Modules only poll completion. | Cross-PU `ModuleRef` lookup is same-PU only (`ModuleRefV2.h:18,85–89`); `AssetRuntime::AssertOwnerThread` (AssetRuntime.cpp:691) forbids non-Main calls. Manual per-stage `RequestStageLoad` is unsafe and adds linear bug surface. Automatic trigger is correct-by-default and scales to any number of stages with zero per-stage developer cost. | All stage modules + AssetServiceModule | Accepted | Yes |
| SD-007 | `AssetServiceModule::DoStart` no longer pre-loads DummyStage textures; only `stage.global` | Restores stage-grain ownership. Pre-loading every stage's assets at boot doesn't scale and defeats the point of stage-driven loading. | AssetServiceModule | Accepted | Yes |
| SD-008 | `JobSystemModule` lives in `Cluiche/CluicheGameBaseline/Modules/`, not in Dia | Symmetric with `TimeServerModule` and `AssetServiceModule` (both wrap Dia singletons but live in CluicheGameBaseline). Engine-provided modules can be lifted to a dedicated Dia library together later if/when there are 3+. | Module placement | Accepted | No |
| SD-009 | Cache hits in `TextureHandler::Load` fire callback synchronously (no job created) | Avoids round-tripping a no-op through the job system. Preserves current behavior for repeated requests. | TextureHandler | Accepted | Yes |
| SD-010 | Decode failures fire `OnLoadFailed` from `Tick()` on the main thread, not from the worker | Consistent thread for all callbacks. Caller never has to defend against worker-thread reentry. | TextureHandler | Accepted | Yes |
| SD-011 | Cross-PU access to `AssetServiceModule` state is **read-only via thread-safe static accessor** (`AssetServiceModule::GetStatic()` returning a pointer with atomic-backed `IsStageLoadComplete`). No cross-PU method invocations that mutate state. | `ModuleRef` is same-PU only by design (per DiaAppFlow SD-003). Static accessor matches existing precedent (`KernelModule::GetStaticTextureHandler` / `GetStaticCanvas`). Reads of completion state are atomic; writes only happen on Main. Streams remain the pattern for *data flow*; static accessor is the pattern for *state queries*. | All stage modules | Accepted | Yes |
| SD-012 | `IsStageLoadComplete(stageId)` returns true if the stage's catalogue declares no assets, or if all declared assets are loaded; returns false during in-flight loads. Failure surfaces via a separate `GetStageLoadState(stageId) → {Idle, Loading, Complete, Failed}` accessor. | Empty manifests (synthetic test stages) must not block. Failure must be distinguishable from in-progress for stage `DoStart` to return `kFailed` correctly. Two accessors keep the simple case simple and the failure case explicit. | AssetServiceModule public API | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for IDs | Stage IDs (`"DummyStage"`), asset IDs, module instance IDs are StringCRC |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `JobSystemModule` is a Module on MainPU; `DummyLevelModule` uses `DoStart`/`DoUpdate`/`DoStop` lifecycle |
| PD-004 | Platform | No STL containers in public APIs | New public `TextureHandler::Tick()` takes no STL types; internal `mPendingUploads` is implementation detail |
| PD-006 | Platform | Visual Studio project files are source of truth | New `JobSystemModule.h/.cpp` added to `CluicheGameBaseline.vcxproj` and `.vcxproj.filters` |
| PD-007 | Platform | C++20 | New code uses C++20 |
| PD-010 | Platform | `.diagame` root, `.diastage` for stages, typed imports | No manifest schema changes; reuse existing catalogue + `.diastage` + `.diaapp` |
| AD-001 | App | Three ProcessingUnits (Main/Render/Sim) | `JobSystemModule` lives on MainPU; doesn't add a fourth PU |
| AD-003 | App | Entry point in Main.cpp | `Main.cpp` does not call `JobSystem::Initialize` directly — module owns it |
| AD-005 | App | App is engine testbed | DummyStage is the canonical demonstration; pattern is validated here before applying to real games |
| SD-001 (AppFlow) | System | Three PUs: Main (infra), Sim (game logic), Render (drawing) | Async loading lives entirely on Main (handler + pump); Sim/Render only read finished textures |
| SD-002 (AppFlow) | System | Main never changes stage | `AssetServiceModule` on Main coordinates stage loads; stage transitions still triggered by Sim/Render |
| SD-005 (AppFlow) | System | Stage-specific modules come from `.diastage` files | DummyStage's `dummy_stage.diaapp` declares `DummyLevelModule` with `dependencies: ["AssetService"]` |
| SD-007 (AppFlow) | System | All modules log at DoStart entry/exit and DoStop entry/exit | `JobSystemModule` and refactored `DummyLevelModule` both log via DiaLogger Application channel |
| SD-013 (AppFlow) | System | DoStart returns StartResult | DummyLevelModule's `kLoading → kReady` is now real, not a placeholder |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Architecture | Does the worker job hold a reference to the `IAssetLoadCallback`, and is callback lifetime guaranteed across the decode → upload window? | The callback is `AssetRuntime` itself (lives for app duration; created in `AssetServiceModule::DoStart`, destroyed in `DoStop`). Stage transitions don't tear down the runtime. The worker's pointer remains valid for the decode duration. **Risk:** if the runtime is ever destroyed while jobs are in flight, dangling pointer. Mitigation: `AssetServiceModule::DoStop` must `JobSystem::Wait` on outstanding loads before destroying the runtime. Document in the JobSystem Module feature spec. |
| 2 | Architecture | What if `Tick()` is never called (e.g., AssetServiceModule disabled or its PU paused)? | Decoded images accumulate in `mPendingUploads` indefinitely; callbacks never fire; `DoStart` of any waiting stage never reaches `kReady`. Acceptable trade-off — it's a misconfiguration that surfaces as a hung load (visible via the per-stage `start_timeout_ms`). Don't add a watchdog. |
| 3 | Architecture | Why not give `JobSystem` a parent-child job tree per stage (parent = stage load, children = each texture)? | Flat-per-texture is simpler and the runtime already tracks per-stage completion via `IsLoadComplete`. Adding parent jobs duplicates that bookkeeping. Reconsider only if we add inter-asset dependencies (e.g., atlas needs sub-textures decoded first). |
| 4 | Architecture | Could the worker thread call `OnLoadComplete` directly to skip the `Tick()` round-trip? | No. `OnLoadComplete` mutates `AssetRuntime` state and `AssertOwnerThread()` would fire. The owner-thread invariant is the cheapest contract to keep — don't break it for one less frame of latency. |
| 5 | Threading | The Sim and Render PUs read `TextureHandler` via `GetTextureId`/`GetTexture`. Does the upload pump on Main race with those reads? | `TextureHandler` already uses `std::shared_mutex` (verified: `TextureHandler.cpp:46`). `Tick()` takes the unique lock when registering an ID; readers take the shared lock. No race. Verification step in plan: write a deterministic test where Sim queries while Main uploads. |
| 6 | Architecture | Should `TextureHandler::Tick()` cap how many uploads happen per frame to avoid hitching? | Not in this iteration. 3 textures don't need a budget. Add a `kMaxUploadsPerTick` constant later when stage sizes grow — call out in the TextureHandler feature spec as a follow-up note. |
| 7 | Boundary | If `TextureHandler` now depends on `JobSystem`, does that introduce a new module dependency? | Yes: `DiaSFML` gains a dependency on `DiaCore/Threading/JobSystem`. `JobSystem` already lives in `DiaCore` so `DiaSFML` already transitively links it; verify the include path and update `dia.diasfml.architecture.module.md`. |
| 8 | Pattern | What stops a future stage from forgetting to declare `["AssetService"]` as a dependency and racing the asset load? | Module dependency declaration is enforced at manifest validation time. If a stage uses `mAssetService.Get()` without declaring the dependency, the framework will refuse to start. Document in the canonical-pattern feature spec. |
| 9 | Failure | If `OnLoadFailed` fires for one texture in a stage's set, what's the stage-level outcome? | `AssetRuntime` already tracks per-asset failure state (`RetryAssetLoad` exists at `AssetRuntime.h:50`). `IsLoadComplete` returns false until all assets either succeed or are explicitly skipped. The stage's `DoStart` polls and eventually times out via `start_timeout_ms`, returning `kFailed`. Recovery UX is out of scope. |
| 10 | Scope | Should this system also convert `UIHandler` to two-phase async? | No. UIHandler is single-pass HTML/Ultralight init with different lifetime. Adding it expands scope without proving more pattern. Leave for a follow-up system once this one is validated. |
| 11 | Lifecycle | What happens if `JobSystemModule` is started before `AssetServiceModule` but DummyStage tries to load before `JobSystemModule` finishes init? | `JobSystemModule::DoStart` returns `kReady` synchronously (Initialize is fast, no I/O). By the time `AssetServiceModule::DoStart` runs, JobSystem is up. Document in JobSystem Module feature: `JobSystemModule` must complete startup synchronously. |
| 12 | Compatibility | Does global stage loading in `AssetServiceModule::DoStart` go through the new async path too, or stay synchronous? | Goes through the new async path. `DoStart` itself returns `kLoading` until `IsLoadComplete("stage.global")` returns true. This validates the pattern at boot and removes the only remaining synchronous load path. |

## Status

`Approved` — 2026-05-17. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete. Plan: [async-asset-loading.plan.md](async-asset-loading.plan.md)
