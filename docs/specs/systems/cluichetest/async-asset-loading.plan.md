# Implementation Plan: Async Stage Asset Loading

**Spec:** @docs/specs/systems/cluichetest/async-asset-loading.md
**Status:** Not Started

---

## Session Notes

**Spec decisions summary (Platform → App → System → Feature):**

Platform bindings: `StringCRC` for all IDs; no STL in public APIs (use DiaCore containers); C++20; `.diagame` is project root, `.diastage` for stages with typed imports.

CluicheTest App bindings: three PUs (Main/Sim/Render); entry point in `Main.cpp`; testbed (not production).

CluicheTest AppFlow System bindings: `LoadingScreenModule` keeps SimToRender fed during transitions; stage-specific modules come from `.diastage` files; all modules log at DoStart/DoStop entry/exit; `DoStart` returns `StartResult` (`kReady` / `kLoading` / `kFailed`).

**This system's bindings (Async Stage Asset Loading):**

- **SD-001**: `JobSystemModule` owns `JobSystem::Initialize/Shutdown`, registered `"stages": ["all"]` on MainPU
- **SD-002**: Two-phase texture load — `sf::Image::loadFromFile` on JobSystem worker; `sf::Texture::loadFromImage` on MainPU
- **SD-003**: `AssetServiceModule::DoUpdate` calls `TextureHandler::Tick()` once per frame on Main
- **SD-004**: `AssetRuntime` stays single-thread (owner = MainPU); callbacks marshalled back to owner
- **SD-005**: Stages declare assets via existing catalogue; no new manifest format
- **SD-006**: Stage loads are **transition-driven** — `AssetServiceModule::DoUpdate` watches `Application::GetCurrentStage()` and auto-issues `RequestStageLoad` on change. Stage Modules **only poll** completion
- **SD-007**: `AssetServiceModule::DoStart` no longer pre-loads DummyStage textures; only `stage.global`
- **SD-008**: `JobSystemModule` lives in `Cluiche/CluicheGameBaseline/Modules/` (matches `TimeServerModule` precedent)
- **SD-009**: Cache hits in `TextureHandler::Load` fire callback synchronously
- **SD-010**: Decode failures fire `OnLoadFailed` from `Tick()` on Main, never from worker
- **SD-011**: Cross-PU access is **read-only via thread-safe static accessor** (`AssetServiceModule::GetStatic()`); no cross-PU method invocations that mutate state
- **SD-012**: `IsStageLoadComplete` + `GetStageLoadState` distinguish `kIdle` / `kLoading` / `kComplete` / `kFailed`; empty manifests return `kComplete`

**Current codebase state (verified during planning):**

- `Dia::Core::Threading::JobSystem` is fully implemented and tested (`Dia/DiaCore/Threading/JobSystem.h`); never `Initialize`d in production code.
- `TextureHandler::Load` (`Dia/DiaSFML/TextureHandler.cpp:44–76`) is fully synchronous: `sf::Texture::loadFromFile()` does file I/O + decode + GPU upload in one call on Main.
- `AssetRuntime::AssertOwnerThread()` (`Dia/DiaAssetRuntime/AssetRuntime.cpp:691`) enforces single-thread access; owner thread captured at construction (= MainPU thread).
- `ModuleRef<T>` is **same-PU only** (`ModuleRefV2.h:18,85–89`; `ProcessingUnit::FindModule` searches only its own array). Cross-PU lookups silently return `nullptr`.
- Cross-PU state-read precedent: `KernelModule::GetStaticCanvas()` / `GetStaticTextureHandler()` (used from SimPU at `DummyLevelModule.cpp:102`).
- Asset-restructure already mostly DONE: `Assets/CluicheTest/Global/`, `Assets/Stages/DummyStage/dummy_stage.diastage`, `assets.catalogue.json` with `"scope": "stage"` for the 3 test textures, `DiaAssetCatalogue` library, and `AssetServiceModule` exist. **Gap:** the 3 PNG files aren't on disk at the declared paths; `dummy_stage.diaapp` is referenced but missing.
- `DummyLevelModule::DoStart` (`Cluiche/Stages/DummyStage/DummyLevelModule.cpp:29–40`) currently has a placeholder `kLoading→kReady` flip via `mStartLoadRequested`.

**Discovery results (Task 1 — 2026-05-17):**

- **Q1 — Application::GetCurrentStage():** `[[nodiscard]] Dia::Core::StringCRC GetCurrentStage() const` on `Application` class. `Dia/DiaApplicationFlow/Application.h:69`, `Application.cpp:356`. Returns `mCurrentStage` directly.
- **Q2 — Per-stage asset rollup:** `AssetRuntime::IsLoadComplete(stageId)` exists (`AssetRuntime.h:39`). `GetLoadProgress(stageId)` at `AssetRuntime.h:53`. `RuntimeStageEntry` stores `mAssetIds` array. Walk via `mStageTable.TryGetItemConst(stageId)->mAssetIds`. **No manual catalogue walk needed** — use `IsLoadComplete` directly.
- **Q3 — KernelModule storage style:** Plain `T*` pointers, NOT atomic. `KernelModule.h:45-46`: `static Dia::Graphics::ICanvas* sCanvas; static Dia::SFML::TextureHandler* sTextureHandler;`. Follow this precedent for `sInstance`.
- **Q4 — HashTable atomic value support:** HashTable uses copy-assignment internally (`HashTableNode::Create` at `HashTableNode.inl:20-25`). `std::atomic<T>` is non-copyable → will fail to compile. Need wrapper: `struct AtomicStageSlot { std::atomic<StageLoadState> v; AtomicStageSlot() : v(kIdle) {} AtomicStageSlot(const AtomicStageSlot&) : v(kIdle) {} AtomicStageSlot& operator=(const AtomicStageSlot&) { return *this; } };`
- **Q5 — PNG source location:** Files **already exist** at the correct stage path: `Cluiche/Assets/Stages/DummyStage/World/Textures/test_red.png`, `test_blue.png`, `test_green.png`. Task 5 only needs `dummy_stage.diaapp` + narrowing DoStart global pre-load.

---

## Implementation Patterns

Each task below should produce code matching these patterns. Reviewer (post-DONE) verifies actual implementation matches; reject if it diverges without explicit reason.

### Pattern P1 — JobSystemModule (Phase 1)

Mirror `TimeServerModule` exactly: header in `Cluiche/CluicheGameBaseline/Modules/`, namespace `Cluiche::AppFlow`, `kTypeId` is `StringCRC("JobSystemModule")`, `DIA_MODULE` registration at end of `.cpp`. `DoStart` calls `JobSystem::Initialize(0)` and returns `kReady` synchronously. `DoUpdate` is empty. `DoStop` calls `JobSystem::Shutdown()` and returns `kDone`. Logs at DoStart/DoStop entry/exit on `"Application"` channel.

### Pattern P2 — TextureHandler two-phase (Phase 2)

Public API surface unchanged for `Load` (signature, semantics on cache hit). Add public `void Tick()` — must be called on owner thread. Internally:
- New private `struct PendingUpload { StringCRC assetId; String512 resolvedPath; sf::Image image; IAssetLoadCallback* callback; bool success; const char* failureReason; };`
- New private `std::mutex mPendingUploadsMutex;` and `std::vector<PendingUpload> mPendingUploads;` (these are impl-only; no STL in public API per PD-004)
- `Load` cache miss: `JobSystem::CreateJob` lambda captures `[this, assetId, resolvedPath, callback]`, calls `sf::Image::loadFromFile`, pushes result under `mPendingUploadsMutex`. `JobSystem::Run(job)`. No `Wait` — fire-and-forget.
- `Tick()`: swap-drain `mPendingUploads` under `mPendingUploadsMutex`, then for each entry: failure → `OnLoadFailed`; success → `sf::Texture::loadFromImage` on Main, register IDs under existing `mMutex` (unique lock), `OnLoadComplete`. Re-check `mPathToId` in the unique-lock section to dedup if two jobs raced for the same path.

### Pattern P3 — AssetServiceModule stage-transition watcher (Phase 3)

Add to existing module (not a new module):
- `enum class StageLoadState { kIdle, kLoading, kComplete, kFailed };` public on `AssetServiceModule`.
- `static AssetServiceModule* GetStatic()` returning a `std::atomic<AssetServiceModule*>` value (set in `DoStart`, cleared in `DoStop`). Match the storage style used by `KernelModule::sCanvas` (Discovery Task 3 of feature 3 confirms — atomic vs plain pointer).
- `bool IsStageLoadComplete(const StringCRC&) const` — thin wrapper over `GetStageLoadState`.
- `StageLoadState GetStageLoadState(const StringCRC&) const` — atomic load on per-stage state slot; missing stage returns `kIdle`; empty-manifest stage returns `kComplete`.
- `DoUpdate(float)` body, in this order:
  1. Read `GetApplication()->GetCurrentStage()`. If `!= mLastWatchedStage`, set state slot to `kLoading`, call `mRuntime.RequestStageLoad(currentStage)`, update `mLastWatchedStage`. Log the transition.
  2. Call `mTextureHandler.Tick()` (per SD-003).
  3. For each `kLoading` entry in `mStageState`, recompute terminal state via `ComputeStageState(stageId)`; if terminal, store and log.
- `DoStart`: existing setup + `sInstance.store(this, release)` at end. Existing `stage.global` pre-load preserved.
- `DoStop`: `sInstance.store(nullptr, release)` first, then existing teardown.

Storage: `Dia::Core::HashTable<StringCRC, std::atomic<StageLoadState>>` if the container supports atomic values; otherwise a small `struct AtomicSlot { std::atomic<StageLoadState> v; };` wrapper. Discovery Task 4 of feature 3 resolves.

### Pattern P4 — DummyStage asset deployment (Phase 4)

Three PNG files at `Cluiche/Assets/Stages/DummyStage/World/Textures/test_red.png`, `test_blue.png`, `test_green.png`. Source: locate existing PNGs in current global asset deployment (Discovery Task 1 of feature 4); if not found, generate trivial 64x64 solid-color PNGs. Move (don't copy) to avoid divergence.

`Cluiche/Assets/Stages/DummyStage/misc/ApplicationFlow/dummy_stage.diaapp` per the v2 overlay format from `applicationflow.md` (the existing approved spec). `DummyLevel` entry has `dependencies: ["TimeServer", "InputStream"]` — **not** `["AssetService"]` (per system SD-006, AppFlow SD-005, this system Feature 5 AC-6).

`AssetServiceModule::DoStart`: narrow the global pre-load to remove the 3 DummyStage texture entries. Verify `stage.global` doesn't end up empty/dangling after the removal — if it does, decide whether to drop the global request entirely or keep it for non-texture global assets.

### Pattern P5 — DummyStage consumer (Phase 5)

`DummyLevelModule::DoStart` rewrite (~10 lines):
```cpp
auto* assets = AssetServiceModule::GetStatic();
const StringCRC kStageId("DummyStage");
if (!mLoadEntryLogged) { DIA_LOG_INFO("Application", "DummyLevelModule DoStart entry — polling"); mLoadEntryLogged = true; }
auto state = assets->GetStageLoadState(kStageId);
if (state == StageLoadState::kFailed)   return StartResult::kFailed;
if (state == StageLoadState::kComplete) return StartResult::kReady;
return StartResult::kLoading;
```

Header: remove `bool mStartLoadRequested = false`; add `bool mLoadEntryLogged = false`. **No** `ModuleRef<AssetServiceModule>`. **No** AssetService include via ModuleRef pattern — include `Modules/AssetServiceModule.h` for the static accessor and the `StageLoadState` enum.

DummyLevelModule's draw-time texture lookup (lines 102–131) is **untouched**. `KernelModule::GetStaticTextureHandler()` continues to resolve IDs.

`DoStop`: reset `mLoadEntryLogged = false` for re-entry; existing log + `kDone` return preserved.

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Discovery — verify pre-coding facts (Application::GetCurrentStage signature; AssetCatalogue per-stage iteration API; HashTable atomic-value support; KernelModule sCanvas storage style; existing test_*.png location) | Discovery report committed to plan Notes | Done | sonnet | Must complete before tasks 4 and 5. Reports back as edits to this plan file documenting answers to all four discovery questions. |
| 2 | JobSystemModule — new module per Pattern P1; manifest entry in `cluiche_main.diaapp` with `"stages": ["all"]`, declared before AssetService; AssetService gains `"JobSystem"` in its `dependencies` | `dia run googletest --filter="*JobSystem*"` passes; `dia run cluichetest` boots without crash; log shows `"JobSystemModule DoStart entry"` followed by `"JobSystemModule DoStart ready"` | Done | sonnet | Feature: jobsystem-module.md. Manifest path: `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp`. |
| 3 | TextureHandler two-phase load — refactor `Load` per Pattern P2; add public `Tick()`; `AssetServiceModule::DoUpdate` calls `mTextureHandler.Tick()`; update `dia.diasfml.architecture.module.md` to declare JobSystem dependency | New unit test `Tests/GoogleTests/SFML/TestTextureHandler.cpp` covers cache hit, async load with `Tick` pump, decode failure, concurrent same-path race; `dia run cluichetest` boots and renders DummyStage's 3 sprites identically (visual no-regression) | Done | sonnet | Feature: texturehandler-two-phase-load.md. Concurrent-load test must use `JobSystem::Wait` for determinism, not `sleep`. |
| 4 | AssetService stage-transition watcher — implement Pattern P3 in existing `AssetServiceModule`; preserve `stage.global` pre-load; add `StageLoadState`, `GetStatic()`, `IsStageLoadComplete`, `GetStageLoadState`; `mStageState` storage | New unit test `Tests/GoogleTests/CluicheGameBaseline/TestAssetServiceModule.cpp` covers: idle stage returns kIdle; transition triggers RequestStageLoad once; `OnLoadComplete` for all assets transitions slot to kComplete; any `OnLoadFailed` transitions to kFailed; empty-manifest stage returns kComplete; `GetStatic` returns nullptr before DoStart and after DoStop | Done | sonnet | Feature: assetservice-stage-watcher.md. State stored in plain C array mStageStates[8] (HashTable has copy constraint). KernelModule::GetStaticTextureHandler() used for Tick. |
| 5 | DummyStage asset deployment — Pattern P4: place 3 PNGs at `Cluiche/Assets/Stages/DummyStage/World/Textures/`; create `dummy_stage.diaapp` overlay; narrow `AssetServiceModule::DoStart` to drop DummyStage textures from global pre-load | `dia pipeline --target cluichetest` deploys the new files; verify deployed bin contains the 3 PNGs at the expected stage path; `dia run cluichetest` still renders DummyStage sprites correctly (proves Asset Pipeline still resolves them) | Done | haiku | All pre-conditions already satisfied: PNGs exist at stage path, dummy_stage.diaapp exists, DoStart never loaded stage textures globally. |
| 6 | DummyStage consumer — Pattern P5: rewrite `DummyLevelModule::DoStart`; remove `mStartLoadRequested`; add `mLoadEntryLogged`; no `ModuleRef`/manifest-dep change | `dia run cluichetest` boots, DummyStage transitions cleanly, sprites render. Behavioral test: inject 100ms sleep into TextureHandler decode worker (compile-time switch or env var); confirm `DoStart` returns `kLoading` for ≥5 frames before `kReady` | Done | sonnet | Feature: dummystage-async-load-consumer.md. Depends on tasks 2, 3, 4, 5 all complete. |
| 7 | End-to-end build + smoke + verification gate | `dia run googletest` — all green (existing + new TextureHandler + new AssetServiceModule tests). `dia run cluichetest` — boots, DummyStage transitions, all 3 sprites render correctly with same positions/scales/rotations/tints as baseline. Behavioral check from task 6 still passes. Negative test: rename one PNG before run; confirm `DoStart` reaches `kFailed` and the failure logs | Done | haiku | pipeline: 3 passed · 0 failed. Log confirms: JobSystemModule lifecycle, stage transition detected, 5/5 assets complete. Build clean, no errors. |

---

## Dependency Order

```
Task 1 (Discovery)
    └─ Task 2 (JobSystemModule)               ─┐
    └─ Task 3 (TextureHandler two-phase)      ─┼─ Task 6 (DummyStage consumer) ─ Task 7
    └─ Task 4 (AssetService watcher)          ─┤
    └─ Task 5 (DummyStage asset deployment)   ─┘
```

- Task 1 must finish first (resolves Discovery questions for tasks 4 and 5).
- Tasks 2, 3, 4, 5 can run in **parallel** after Task 1: different files, no shared headers (Task 2 only edits the manifest; Task 3 edits DiaSFML; Task 4 edits AssetServiceModule; Task 5 edits assets + DoStart). Task 4 and Task 5 both touch `AssetServiceModule.cpp` so coordinate edits — sequence them if dispatched in parallel.
- Task 6 depends on all four (uses `GetStatic()` from Task 4 and PNGs from Task 5; load actually parallelizes via Tasks 2+3).
- Task 7 is the final integration gate.

**Coordination note:** Task 4 and Task 5 both edit `AssetServiceModule.cpp`. Recommend dispatching Task 4 first (the bigger structural change), then Task 5 as a focused diff on the narrowed `DoStart`.

---

## Files Created / Modified

| File | Task | Action |
|------|------|--------|
| `Cluiche/CluicheGameBaseline/Modules/JobSystemModule.h` | 2 | New |
| `Cluiche/CluicheGameBaseline/Modules/JobSystemModule.cpp` | 2 | New |
| `Cluiche/CluicheGameBaseline/CluicheGameBaseline.vcxproj` | 2, 4 | Update |
| `Cluiche/CluicheGameBaseline/CluicheGameBaseline.vcxproj.filters` | 2, 4 | Update |
| `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp` | 2 | Update — add JobSystem entry, add JobSystem to AssetService deps |
| `Dia/DiaSFML/TextureHandler.h` | 3 | Update — add `Tick()`, `PendingUpload`, queue + mutex |
| `Dia/DiaSFML/TextureHandler.cpp` | 3 | Rewrite `Load` body, add `Tick()` impl |
| `Dia/DiaSFML/dia.diasfml.architecture.module.md` | 3 | Update `dependent_modules` to include `DiaCore/Threading/JobSystem` |
| `Cluiche/Tests/GoogleTests/SFML/TestTextureHandler.cpp` | 3 | New |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` / `.filters` | 3, 4 | Update — register new test files |
| `Cluiche/CluicheGameBaseline/Modules/AssetServiceModule.h` | 4 | Update — add `StageLoadState`, `GetStatic`, `IsStageLoadComplete`, `GetStageLoadState`, state storage |
| `Cluiche/CluicheGameBaseline/Modules/AssetServiceModule.cpp` | 4, 5 | Update — Task 4 adds watcher + state machine + Tick call; Task 5 narrows DoStart pre-load |
| `Cluiche/Tests/GoogleTests/CluicheGameBaseline/TestAssetServiceModule.cpp` | 4 | New |
| `Cluiche/Assets/Stages/DummyStage/World/Textures/test_red.png` | 5 | New (or move from global) |
| `Cluiche/Assets/Stages/DummyStage/World/Textures/test_blue.png` | 5 | New (or move from global) |
| `Cluiche/Assets/Stages/DummyStage/World/Textures/test_green.png` | 5 | New (or move from global) |
| `Cluiche/Assets/Stages/DummyStage/misc/ApplicationFlow/dummy_stage.diaapp` | 5 | New (overlay file) |
| `Cluiche/Stages/DummyStage/DummyLevelModule.h` | 6 | Update — remove `mStartLoadRequested`, add `mLoadEntryLogged`, include `AssetServiceModule.h` |
| `Cluiche/Stages/DummyStage/DummyLevelModule.cpp` | 6 | Update — replace `DoStart`/`DoStop` bodies |

---

## Verification Gate

Per `.claude/skills/verify.md`, every task DONE must include a fresh run with quoted output. Cross-task signals:

- **Task 2 DONE proof:** `dia run cluichetest` log contains exactly one `"JobSystemModule DoStart entry"` followed by `"JobSystemModule DoStart ready"` before any `"AssetServiceModule DoStart entry"`.
- **Task 3 DONE proof:** new `TestTextureHandler.*` GoogleTest passes; `dia run cluichetest` renders 3 sprites at expected positions (visual smoke).
- **Task 4 DONE proof:** new `TestAssetServiceModule.*` GoogleTest passes; `dia run cluichetest` log shows `"AssetService: detected transition to '...', requesting load"` followed eventually by `"AssetService: stage '...' complete"`.
- **Task 5 DONE proof:** `dia pipeline --target cluichetest` succeeds; PNGs deployed at expected stage path; `dia run cluichetest` still renders.
- **Task 6 DONE proof:** without injected sleep, `DoStart` may return `kReady` on first poll (acceptable). With injected 100ms sleep, log shows ≥5 sequential `"DoStart entry — polling"` lines (or just one entry log + multiple `kLoading` returns observable via framework state) before `"DoStart ready"`.
- **Task 7 DONE proof:** all GoogleTests pass; `cluichetest` exe runs ≥20s without crash; negative test (renamed PNG) reaches `kFailed`.

Anti-rationalization reminder: do not say "should work" or "previously verified." Each DONE quotes its own command output.
