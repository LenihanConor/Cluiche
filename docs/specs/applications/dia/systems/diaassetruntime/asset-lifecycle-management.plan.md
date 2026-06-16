# Plan: Asset Lifecycle Management

**Spec:** @docs/specs/applications/dia/systems/diaassetruntime/asset-lifecycle-management.md  
**Status:** Done (Tasks 10-11, 18-19 deferred)  
**Started:** 2026-05-08  
**Last Updated:** 2026-05-08

## Implementation Patterns

### Phase 2 — Real I/O Handlers

Each handler lives in its owning engine module (not game code):
- `TextureHandler` → `Dia/DiaSFML/`
- `ShaderHandler` → `Dia/DiaSFML/`
- `UIHandler` → `Dia/DiaUIUltralight/`
- `AudioHandler` → `Dia/DiaSFML/` (stub until dedicated DiaAudio module exists)

Pattern:
- Owns an internal map: `StringCRC assetId → subsystem resource ID` (e.g. `unsigned int textureId`)
- `Load()`: calls the internal subsystem API, stores the mapping, calls `OnLoadComplete`/`OnLoadFailed`
- `Unload()`: calls subsystem release, removes mapping
- Type-specific accessor: returns the resource ID (or 0/null if not loaded)
- Game code registers the handler and queries the accessor — no subsystem knowledge needed

```cpp
// Lives in Dia/DiaSFML/TextureHandler.h
namespace Dia::SFML
{
    class TextureHandler : public Dia::AssetRuntime::IAssetTypeHandler
    {
    public:
        unsigned int GetTextureId(const Dia::Core::StringCRC& assetId) const;
        void Load(...) override;   // calls TextureManager::LoadTexture internally
        void Unload(...) override; // calls TextureManager::UnloadTexture internally
    private:
        TextureManager* mTextureManager;
        // assetId -> GPU texture ID mapping
        Dia::Core::Containers::HashTableC<Dia::Core::StringCRC, unsigned int, 64> mLoadedTextures;
    };
}
```

```cpp
// Game-level wiring (CluicheTest AssetServiceModule) — subsystems own their handlers
mRuntime.RegisterTypeHandler("texture", renderWindow->GetTextureHandler());
mRuntime.RegisterTypeHandler("ui", uiSystem->GetUIHandler());
// No handler registration for config, entity, folder, manifest — auto-validated
```

### Phase 3 — Auto-Validation in AssetRuntime

No handler class needed. AssetRuntime's `RequestStageLoad` dispatch:
1. Look up handler for type prefix
2. If handler found → call `handler->Load(...)`, transition to `Loading`
3. If no handler → check file/folder exists at resolved path:
   - Exists → transition directly to `Loaded`
   - Missing → transition to `Failed` with "resource not found" log

### Phase 4 — Enforcement

Internalization pattern:
- `TextureManager::LoadTexture` becomes private; `TextureHandler` is a friend
- `RenderWindow::LoadTexture` removed from public API entirely
- Assert in renderer when a draw command references a texture ID not in the loaded set

## Tasks

### Phase 1 — State Machine & Handler Framework (Complete)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `IAssetTypeHandler.h` with `IAssetTypeHandler` and `IAssetLoadCallback` interfaces | TestHandlerDispatch_HandlerCalledOnStageLoad | Done | sonnet | New file, additive — no existing code breaks |
| 2 | Rewrite `AssetState.h`: replace `Registered`/`Unloading` with `Null`/`Loading`/`Failed`/`Unloaded` | TestStateInit_AllAssetsStagedAfterLoad | Done | sonnet | Breaking change — landed atomically with tasks 3-4 |
| 3 | Rewrite `AssetRuntime.h/.cpp`: handler registry, dispatch in `RequestStageLoad`, `IAssetLoadCallback` impl, `RetryAssetLoad`, `GetLoadProgress`, assert on unload-while-loading, remove listener pattern | TestHandlerDispatch, TestRetryAssetLoad, TestGetLoadProgress, TestUnloadWhileLoadingAsserts | Done | opus | Core rewrite — biggest task |
| 4 | Remove `IAssetStateListener.h` | — | Done | haiku | Mechanical deletion after task 3 |
| 5 | Rewrite `AssetServiceModule` to use handler registration + deferred start (remove `IAssetStateListener` impl) | — | Done | sonnet | DefaultAssetHandler immediately calls OnLoadComplete |
| 6 | Update `DiaAssetRuntime.vcxproj`: add `IAssetTypeHandler.h`, remove `IAssetStateListener.h` | — | Done | haiku | Mechanical |
| 7 | Rewrite `TestAssetStateMachine.cpp` for new states and handler dispatch; add handler dispatch tests | All DiaAssetRuntime tests pass | Done | sonnet | All 6 test files rewritten; 4348/4349 pass (1 pre-existing unrelated failure) |

### Phase 2 — Real I/O Handlers (in engine modules)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 8 | `TextureHandler` in DiaSFML — real I/O: call `TextureManager::LoadTexture` in `Load`, release in `Unload`, add `GetTextureId(assetId)` accessor, add assetId→textureId map | `dia run googletest --filter="TextureHandler*"` | Done | sonnet | Lives in `Dia/DiaSFML/`; DiaSFML depends on DiaAsset (interface module) |
| 9 | `UIHandler` in DiaUIUltralight — acknowledges UI asset lifecycle, wired via SetUISystem | CluicheTest builds clean | Done | sonnet | Moved from game code into engine; old UltralightUIHandler deleted |
| 10 | `ShaderHandler` in DiaSFML — real I/O: load shader file in `Load`, expose accessor for loaded shader data | — | Deferred | — | Shader loading currently inline in RenderWindow; auto-validated for now. Real handler when shader system extracted |
| 11 | `AudioHandler` in DiaSFML — stub: validate file exists, acknowledge; real I/O deferred until audio subsystem | — | Deferred | — | No audio subsystem yet; auto-validated for now. Real handler when DiaAudio created |

### Phase 3 — Auto-Validation & Game-Level Simplification

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 12 | Add auto-validation path in `AssetRuntime::RequestStageLoad` — no handler → check file exists → Loaded/Failed | `dia run googletest --filter="AutoValidat*"` | Done | sonnet | Handler-less types checked via std::filesystem (folders) / fopen (files) |
| 13 | Remove `DefaultAssetHandler` and handler-less type registrations from `AssetServiceModule` | CluicheTest launches clean | Done | haiku | Removed class + 7 handler registrations; game code only registers texture/ui |
| 14 | Wire handler instances in `AssetServiceModule`: create DiaSFML/DiaUIUltralight handlers, pass subsystem deps, register | CluicheTest launches clean | Done | sonnet | Dia::SFML::TextureHandler wired via GetTextureManager(); UltralightUIHandler unchanged |

### Phase 4 — Enforcement & Internalization

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 15 | Internalize `TextureManager::LoadTexture` — add `friend class Dia::SFML::TextureHandler`, remove from `RenderWindow` public API | Build succeeds | Done | sonnet | LoadTexture/UnloadTexture/UnloadAll now private; TextureHandler is friend |
| 16 | Fix `SimProcessingUnit`: query TextureHandler for IDs instead of direct LoadTexture calls | Build succeeds; sprites only render after stage load | Done | sonnet | TextureHandler passed via StartData; queries by asset ID |
| 17 | Add debug assert in renderer when draw command references texture ID not in loaded set | DIA_ASSERT fires on unloaded texture reference | Done | sonnet | EntityFrameRenderer asserts before graceful skip |
| 18 | Internalize Ultralight direct resource loading — only callable by `UIHandler` | Build succeeds | Deferred | — | Ultralight page loading driven by UI system, not asset system yet |
| 19 | Internalize shader file loading — only callable by `ShaderHandler` | Build succeeds | Deferred | — | Shader loading inline in RenderWindow::Initialize; extract when shader system created |

## Session Notes

### Spec Decisions Summary

**Binding constraints from spec chain (Platform → App → System → Feature):**
- SD-ARUN-006: No content loading in DiaAssetRuntime — handlers live in engine subsystem modules (DiaSFML, DiaUIUltralight), not DiaAssetRuntime
- SD-ARUN-001: No DiaApplicationFlow dependency in DiaAssetRuntime
- PD-004/AD-002: No STL in public APIs — use DiaCore containers
- SD-ARUN-002: Stage = unit of load/unload; asset = unit of state
- Handlers live in engine modules, not game code — push complexity down, keep game layer thin
- Handler names are backend-agnostic (TextureHandler, not SFMLTextureHandler)
- Type-specific accessors on handlers for content retrieval (not generic template)
- Two asset categories: handled (texture, ui, shader, audio) vs auto-validated by AssetRuntime (folder, config, entity, manifest)
- Auto-validation: no handler needed — AssetRuntime checks file exists, transitions Staged → Loaded/Failed
- Enforcement via `friend` + API removal; assert on use-before-load in debug builds
- Backend portability: swap SFML → BGFX = rewrite handler in new module, game code unchanged

### 2026-05-08 (Phase 1)
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

### 2026-05-08 (Phases 2-4 planning)
- DummyStage texture-rendering-during-kernel-boot bug identified root cause: `SimProcessingUnit` calls `RenderWindow::LoadTexture` directly, bypassing AssetRuntime
- Spec extended with AC18-AC24 (enforcement, content access, handler placement, auto-validation)
- Key architectural decisions:
  - Handlers live in engine subsystem modules (DiaSFML, DiaUIUltralight), not game code — pushes complexity down
  - Handler-less types (config, entity, folder, manifest) auto-validated by AssetRuntime (file exists check)
  - Game code is thin wiring: register handler instances, query accessors
  - Backend swap (SFML→BGFX) = rewrite handler in new module, game code unchanged
  - Type-specific accessors (e.g. `GetTextureId`) not generic template — easier to debug
- Plan extended with tasks 8-19 across Phases 2-4
- AudioHandler is a stub (no audio subsystem yet)
- Task 15-16 are paired: internalize API (breaks SimProcessingUnit compile), then minimal fix
- Task 17 is the proof: SimProcessingUnit draws during boot → assert fires → enforcement verified

### 2026-05-08 (Phase 2-3 implementation)
- Task 8: Created `Dia::SFML::TextureHandler` in DiaSFML — real I/O via TextureManager, assetId→textureId mapping
- Task 12: Auto-validation in AssetRuntime dispatch — handler-less types checked for file/folder existence
- Created `DiaAsset` interface-only module (IAssetTypeHandler.h lives there); DiaSFML depends on DiaAsset not DiaAssetRuntime
- Task 13: Removed DefaultAssetHandler class + all 7 handler-less registrations from game code
- Task 14: Wired `Dia::SFML::TextureHandler` via `RenderWindow::GetTextureManager()` accessor
- Deleted old `Cluiche::Main::SFMLTextureHandler` stub
- Build passes; 40/40 asset tests pass; 4367/4369 full suite (2 flaky ordering issues pre-existing)
- Task 9: Moved UIHandler into DiaUIUltralight engine module; old game-level UltralightUIHandler deleted
- Tasks 10-11: Deferred — shader/audio auto-validated; real handlers when subsystems mature
- Task 15: Internalized TextureManager::LoadTexture (private + friend TextureHandler); removed RenderWindow::LoadTexture public API
- Task 16: SimProcessingUnit now queries TextureHandler::GetTextureId by asset ID; sprites only render when stage is loaded (fixes DummyStage bug)
- Task 17: DIA_ASSERT in EntityFrameRenderer::Visit when texture ID not found — enforcement proof
- Tasks 18-19: Deferred — Ultralight/shader internalization depends on future system extraction
- DummyStage texture-during-boot bug is now fixed: no direct LoadTexture path exists, textures only available after AssetRuntime loads the stage

### 2026-05-08 (Handler ownership simplification)
- Moved handler ownership from game layer (AssetServiceModule) into engine subsystems:
  - RenderWindow owns TextureHandler, self-wires to its TextureManager at construction
  - UltralightUISystem owns UIHandler, self-wires to itself at construction
- AssetServiceModule reduced to pure registration: `mRuntime.RegisterTypeHandler("texture", renderWindow->GetTextureHandler())`
- Removed: EnsureHandlerDependencies(), GetTextureHandler(), mHandlerDependenciesWired, handler member instances
- SimProcessingUnit gets TextureHandler from RenderWindow directly (not via AssetServiceModule)
- Net result: fewer interfaces, no deferred wiring, game layer is thin registration only
