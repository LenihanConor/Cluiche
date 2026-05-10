# Feature Spec: Asset Lifecycle Management

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetRuntime | @docs/specs/systems/dia/diaassetruntime.md |
| Feature | Asset Lifecycle Management | (this document) |

## Summary

Extends the DiaAssetRuntime state machine with truthful load/unload tracking via `IAssetTypeHandler` dispatch. The runtime currently conflates "staged" with "loaded" — consumers immediately acknowledge without performing real I/O. This feature adds a `Loading` state (entered when a handler is invoked), a `Failed` state (entered when a handler reports failure), a type-handler registry (keyed by asset ID prefix), and module-lifecycle integration so phases wait until loads genuinely complete.

## Problem

`AssetServiceModule::OnAssetReady` immediately calls `AcknowledgeAssetLoaded` without any real I/O occurring. This makes `IsLoadComplete()` unreliable — it returns true before textures, UI pages, or audio are actually in memory. Game code cannot trust the runtime to answer "is everything ready?" Systems (SFML, Ultralight) load resources independently with no coordination through the runtime, leaving no central authority for load state.

## Acceptance Criteria

### State Machine & Handler Framework

1. **AC1**: Assets follow a `Null -> Staged -> Loading -> Loaded -> Unloaded` state machine, with `Failed` reachable from `Loading`
2. **AC2**: `Loading` state is entered when an `IAssetTypeHandler::Load` is invoked for that asset
3. **AC3**: `Loaded` state is only entered when the handler confirms completion via callback
4. **AC4**: `Failed` state is entered when the handler reports failure (with error reason string); retry requires explicit re-request (not automatic)
5. **AC5**: Assert/error if `RequestStageUnload` is called while any asset in the stage is in the `Loading` state — this is a programming error in the game flow
6. **AC6**: Asset type is derived from asset ID prefix (everything before the first `.`, e.g. `texture.*` -> "texture")
7. **AC7**: `IAssetTypeHandler` interface provides `Load(assetId, resolvedPath, callback)` and `Unload(assetId)`
8. **AC8**: Handlers are registered by type prefix string in a type-handler registry on `AssetRuntime`
9. **AC9**: Module lifecycle integration: the module wrapping `AssetRuntime` returns `kDeferred` from `DoStart`, and the phase only advances when `IsLoadComplete()` returns true
10. **AC10**: Stage ref-counting remains the sole authority for load/unload decisions — no consumer-level refcount
11. **AC11**: Unload is immediate when refcount hits 0 (no grace period)
12. **AC12**: Architecture does not block future dependency ordering — no assumptions that prevent adding a DAG layer for load ordering later
13. **AC13**: `Failed` assets block `IsLoadComplete()` — a failed load is a hard error that must be resolved
14. **AC14**: `IsLoadComplete()` returns true only when ALL assets in the requested stage are in `Loaded` state (none in `Loading`, `Staged`, or `Failed`)
15. **AC15**: `OnLoadFailed` callback carries a `const char* reason` string describing the failure (e.g. "file not found", "corrupt data")
16. **AC16**: `GetLoadProgress(stageId)` returns loaded count and total count for the stage, enabling loading screens / progress bars
17. **AC17**: In Release builds (where asserts strip), a load timeout or failure callback prevents indefinite hangs — game code receives a `Failed` state it can react to (e.g. show error screen) rather than blocking forever

### Enforced Load Path

18. **AC18**: Raw subsystem loading APIs (`TextureManager::LoadTexture`, Ultralight resource loading, shader file loading) are internal — only callable by their respective `IAssetTypeHandler` implementation, not by application code
19. **AC19**: Application code retrieves loaded content via type-specific accessors on the handler (e.g. `TextureHandler::GetTextureId(assetId)`) — not by calling subsystem APIs directly
20. **AC20**: An asset not in `Loaded` state cannot produce rendered output — attempting to use an unloaded asset's resource triggers an assert in debug builds
21. **AC21**: Stage-scoped assets are only usable when their stage is loaded. The DummyStage textures (test_red, test_blue, test_green) must not render during kernel boot — only when DummyStage is the active stage.
22. **AC22**: Asset types are split into two categories:
    - **Handled types** (texture, ui, shader, audio): a registered `IAssetTypeHandler` performs real I/O into a subsystem. Handlers live in the owning engine module (e.g. `TextureHandler` in DiaSFML, `UIHandler` in DiaUIUltralight) — not in game code.
    - **Auto-validated types** (folder, manifest, config, entity): no handler registered. AssetRuntime auto-validates (file/folder exists on disk) and transitions to `Loaded` or `Failed`. Consumers parse on demand via `ResolveAssetPath`.
23. **AC23**: When `RequestStageLoad` encounters an asset with no registered handler, AssetRuntime validates the resolved path exists on disk and transitions directly to `Loaded` (if exists) or `Failed` (if missing). No handler class needed for these types.
24. **AC24**: Handlers live in their owning engine subsystem module, not in game code. Game code only registers handler instances and queries them for content access. This keeps the game layer (e.g. CluicheTest) as thin wiring — swapping a graphics backend means rewriting the handler in the new backend module, not changing game code.

## State Machine

```
Null ----[manifest load]----> Staged
Staged --[RequestStageLoad, handler invoked]----> Loading
Loading -[handler success callback]----> Loaded
Loading -[handler failure callback]----> Failed
Loaded --[RequestStageUnload, refcount=0]----> Unloaded
Unloaded -[re-staging]----> Staged
Failed --[explicit retry request]----> Loading
```

**Invalid transitions:**
- `Loading -> Unloaded` : ASSERT. Must not unload while loading. (AC5)
- `Failed -> Unloaded` : Allowed — game may choose to unload a failed stage and move on

**State descriptions:**
- **Null**: Not yet known to the runtime (pre-manifest)
- **Staged**: Known, path resolved, eligible for loading when stage is requested
- **Loading**: Handler has been called, I/O in progress
- **Loaded**: Handler confirmed content is in memory and usable
- **Failed**: Handler reported an error; blocks stage completion
- **Unloaded**: Previously loaded, content released, path still resolvable

## API Design

### IAssetTypeHandler

```cpp
namespace Dia::AssetRuntime
{
    class IAssetLoadCallback
    {
    public:
        virtual void OnLoadComplete(const Dia::Core::StringCRC& assetId) = 0;
        virtual void OnLoadFailed(const Dia::Core::StringCRC& assetId, const char* reason) = 0;
    };

    class IAssetTypeHandler
    {
    public:
        virtual ~IAssetTypeHandler() = default;

        virtual void Load(const Dia::Core::StringCRC& assetId,
                          const Dia::Core::Containers::String512& resolvedPath,
                          IAssetLoadCallback* callback) = 0;

        virtual void Unload(const Dia::Core::StringCRC& assetId) = 0;
    };
}
```

### Handler Registration and Progress on AssetRuntime

```cpp
namespace Dia::AssetRuntime
{
    struct LoadProgress
    {
        unsigned int loaded;
        unsigned int total;
        unsigned int failed;
    };

    class AssetRuntime
    {
    public:
        // Register a handler for a type prefix (e.g. "texture", "ui", "audio")
        void RegisterTypeHandler(const char* typePrefix, IAssetTypeHandler* handler);

        // Unregister a handler
        void UnregisterTypeHandler(const char* typePrefix);

        // Load progress for a stage (AC16)
        LoadProgress GetLoadProgress(const Dia::Core::StringCRC& stageId) const;
    };
}
```

### Extended AssetState Enum

```cpp
namespace Dia::AssetRuntime
{
    enum class AssetState
    {
        Null,
        Staged,
        Loading,
        Loaded,
        Failed,
        Unloaded
    };
}
```

### Load Flow (internal to AssetRuntime)

When `RequestStageLoad` fires for a stage:
1. For each asset in the stage with refcount transitioning 0->1:
   - Extract type prefix from asset ID (e.g. `"texture"` from `"texture.player_ship"`)
   - Look up handler in registry
   - If handler found: call `handler->Load(assetId, resolvedPath, this)`, transition to `Loading`
   - If no handler: log error, transition to `Failed`
2. `AssetRuntime` implements `IAssetLoadCallback` internally:
   - `OnLoadComplete` -> transition `Loading -> Loaded`
   - `OnLoadFailed` -> transition `Loading -> Failed`

### Type-Specific Content Access (AC19)

Handlers expose type-specific accessors for loaded content. Application code queries the handler, not the raw subsystem:

```cpp
// TextureHandler — returns GPU texture ID for use in SpriteDrawCommand
class TextureHandler : public Dia::AssetRuntime::IAssetTypeHandler
{
public:
    // Returns texture ID usable in SpriteDrawCommand, or 0 if not loaded
    unsigned int GetTextureId(const Dia::Core::StringCRC& assetId) const;

    // IAssetTypeHandler
    void Load(const Dia::Core::StringCRC& assetId,
              const Dia::Core::Containers::String512& resolvedPath,
              Dia::AssetRuntime::IAssetLoadCallback* callback) override;
    void Unload(const Dia::Core::StringCRC& assetId) override;
};

// Usage in game code — draw sprite only if texture is loaded
unsigned int texId = textureHandler->GetTextureId(StringCRC("texture.test_red"));
if (texId != 0)
{
    SpriteDrawCommand cmd(texId, position);
    frameBuffer.RequestDrawSprite(cmd);
}
```

### Validation-Only Handler Pattern (AC22)

For types where "load" means "confirm exists" — consumers parse on demand:

```cpp
class ValidationAssetHandler : public Dia::AssetRuntime::IAssetTypeHandler
{
public:
    void Load(const Dia::Core::StringCRC& assetId,
              const Dia::Core::Containers::String512& resolvedPath,
              Dia::AssetRuntime::IAssetLoadCallback* callback) override
    {
        // Validate file/folder exists on disk
        if (FileExists(resolvedPath.AsCStr()))
            callback->OnLoadComplete(assetId);
        else
            callback->OnLoadFailed(assetId, "resource not found");
    }

    void Unload(const Dia::Core::StringCRC& assetId) override {}
};
```

### Module Integration Pattern

```cpp
// In AssetServiceModule (CluicheTest game code, not DiaAssetRuntime)
Dia::Application::StateObject::OpertionResponse AssetServiceModule::DoStart(const IStartData*)
{
    // ... setup runtime, register handlers ...
    mRuntime.RequestStageLoad(globalStageId);
    return StateObject::OpertionResponse::kDeferred;  // phase waits
}

// Module polls or receives callback to signal completion
void AssetServiceModule::OnAllAssetsLoaded()
{
    SignalStartComplete();  // unblocks the phase
}
```

## Files Affected

| File | Change |
|------|--------|
| `Dia/DiaAssetRuntime/AssetState.h` | Extend enum: add `Null`, `Loading`, `Failed`, `Unloaded` (rename `Registered`->`Staged`, `Unloading`->`Unloaded`) |
| `Dia/DiaAssetRuntime/IAssetTypeHandler.h` | New — handler interface + callback interface |
| `Dia/DiaAssetRuntime/IAssetStateListener.h` | Remove — replaced by handler pattern + query API |
| `Dia/DiaAssetRuntime/AssetRuntime.h` | Add `RegisterTypeHandler`, `UnregisterTypeHandler`, `RetryAssetLoad`, handler registry member. Remove `RegisterListener`/`UnregisterListener`. |
| `Dia/DiaAssetRuntime/AssetRuntime.cpp` | Implement handler dispatch in stage load path, implement `IAssetLoadCallback`, add assert in unload path |
| `Cluiche/CluicheTest/.../AssetServiceModule.h/.cpp` | Replace listener + immediate-acknowledge with handler registration + deferred start |
| `Dia/DiaSFML/TextureHandler.h/.cpp` | New — real I/O: calls `TextureManager::LoadTexture` in Load, releases in Unload, exposes `GetTextureId(assetId)` |
| `Dia/DiaSFML/ShaderHandler.h/.cpp` | New — real I/O handler for shader assets |
| `Dia/DiaUIUltralight/UIHandler.h/.cpp` | New — real I/O: loads UI resources, type-specific accessor |
| `Dia/DiaAudio/AudioHandler.h/.cpp` (or DiaSFML) | New — real I/O handler for audio assets (stub until audio subsystem exists) |
| `Dia/DiaAssetRuntime/AssetRuntime.cpp` | Add auto-validation path for assets with no registered handler (file/folder exists check) |
| `Cluiche/CluicheTest/.../AssetServiceModule.h/.cpp` | Remove `DefaultAssetHandler`, remove handler-less type registrations, simplify to just wiring |
| `Dia/DiaSFML/TextureManager.h` | Make `LoadTexture` non-public (friend or internal access only for handler) |
| `Dia/DiaSFML/RenderWindow.h` | Remove or internalize public `LoadTexture` method |

## Tasks

### Phase 1 — State Machine & Handler Framework

| # | Task | Status |
|---|------|--------|
| 1 | Extend `AssetState` enum with new states | Not Started |
| 2 | Create `IAssetTypeHandler` and `IAssetLoadCallback` interfaces (with error reason string) | Not Started |
| 3 | Add handler registry to `AssetRuntime` (register/unregister by prefix) | Not Started |
| 4 | Implement handler dispatch in `RequestStageLoad` path | Not Started |
| 5 | Implement `IAssetLoadCallback` in `AssetRuntime` (advance state on complete/fail) | Not Started |
| 6 | Add assert in `RequestStageUnload` for assets in `Loading` state | Not Started |
| 7 | Update `IsLoadComplete` / `IsAssetReady` for new states | Not Started |
| 8 | Add `GetLoadProgress(stageId)` returning loaded/total/failed counts | Not Started |
| 9 | Add Release-build timeout/failure path (Failed state reachable without assert, game code can react) | Not Started |
| 10 | Rewrite `AssetServiceModule` to use deferred start + handler registration | Not Started |
| 11 | Remove `IAssetStateListener` interface and all usages | Not Started |
| 12 | Add tests for new state transitions, handler dispatch, failure path, and progress query | Not Started |

### Phase 2 — Real I/O Handlers

| # | Task | Status |
|---|------|--------|
| 13 | `TextureHandler` — real I/O: call `TextureManager::LoadTexture` in `Load`, release in `Unload`, expose `GetTextureId(assetId)` | Not Started |
| 14 | `UIHandler` — real I/O: load UI resources in `Load`, release in `Unload`, expose type-specific accessor | Not Started |
| 15 | `ShaderHandler` — real I/O: load shader files (e.g. `ui.frag`) in `Load`, expose accessor | Not Started |
| 16 | `AudioHandler` — real I/O: load audio resources in `Load`, release in `Unload`, expose accessor | Not Started |

### Phase 3 — Validation-Only Handlers

| # | Task | Status |
|---|------|--------|
| 17 | `ValidationAssetHandler` — replaces `DefaultAssetHandler`: validates file/folder exists on disk, calls `OnLoadComplete`/`OnLoadFailed` | Not Started |
| 18 | Register validation handler for config, entity, folder, manifest type prefixes | Not Started |

### Phase 4 — Enforcement & Internalization

| # | Task | Status |
|---|------|--------|
| 19 | Internalize `TextureManager::LoadTexture` — make non-public, only callable by `TextureHandler` | Not Started |
| 20 | Internalize Ultralight direct resource loading — only callable by `UIHandler` | Not Started |
| 21 | Internalize shader file loading — only callable by `ShaderHandler` | Not Started |
| 22 | Add debug assert when application code attempts to use a texture ID that isn't in `Loaded` state | Not Started |
| 23 | Verify `SimProcessingUnit` test sprites trigger assert during kernel boot (proves enforcement) | Not Started |

## Design Decisions

| Decision | Rationale |
|----------|-----------|
| Type prefix derived from asset ID, not from manifest field | Convention already established (`texture.*`, `ui.*`); zero schema changes needed |
| Handlers live in game code, not DiaAssetRuntime | DiaAssetRuntime has no dependency on SFML/Ultralight (SD-ARUN-006) |
| `IAssetLoadCallback` rather than return value | Supports sync today (call callback immediately in `Load`) and async later (call callback on completion) |
| Assert on unload-while-loading rather than graceful cancel | This is a bug in game flow logic; should never happen in correct code |
| `Failed` blocks `IsLoadComplete` | Forces the issue to be addressed — silent failures are unacceptable |
| No load ordering/DAG now | Flat loading is sufficient; architecture allows adding dependency edges later without breaking existing handlers |
| Stage refcount is sole authority | No consumer-level refcount; simplifies the model |
| Type-specific accessors on handlers, not generic templated query | Easier to debug — callsite shows exactly which handler you're querying. No type-erasure complexity. Each handler knows its resource type. |
| Raw subsystem APIs internalized, not removed | Handlers still need them — they just become implementation details. `friend` or internal access pattern. |
| Two asset categories: handled vs auto-validated | Types with subsystem content (texture, shader, ui, audio) get handlers. Types without (folder, config, entity, manifest) are auto-validated by AssetRuntime — no handler class, no game-level code needed. |
| Handlers live in engine subsystem modules, not game code | TextureHandler belongs in DiaSFML, UIHandler in DiaUIUltralight. Game code registers instances and queries accessors. Backend swap = rewrite handler in new module, game code unchanged. Complexity pushed down. |
| Assert on use-before-load in debug builds | Catches the bug class immediately at the call site. Release builds skip gracefully (no render output for invalid ID). |
| SimProcessingUnit test sprites left in place to prove enforcement | Existing code that bypasses the asset pipeline will trigger the assert, proving the enforcement works. Moved to DummyStage in a follow-up step. |

## Open Questions

None — all resolved during design discussion.

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | Asset IDs and type prefixes use StringCRC for lookups. Handler registry keyed on StringCRC of the prefix. |
| PD-002 | PU/Phase/Module architecture | DiaAssetRuntime remains a library; module integration (AC9) is in game-level AssetServiceModule, not the runtime itself. |
| PD-004 | No STL in public APIs | `IAssetTypeHandler` uses `Dia::Core::Containers::String512` for paths, `Dia::Core::StringCRC` for IDs. No std types in interface. |
| PD-005 | x64 Windows only | No cross-platform considerations. |
| PD-007 | C++20 required | Uses `enum class`, `= default`/`= delete`. No C++23 features. |
| PD-008 | Directory.Build.props owns output | No vcxproj output path changes. |
| AD-002 | No STL in public APIs | Same as PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | All new types in `Dia::AssetRuntime::` namespace. |
| SD-ARUN-001 | No DiaApplicationFlow dependency | `IAssetTypeHandler` and state machine are pure library code. Module lifecycle integration is game-code responsibility. |
| SD-ARUN-002 | Stage = unit of load/unload; asset = unit of state | Preserved — `RequestStageLoad` dispatches per-asset handlers. |
| SD-ARUN-004 | Consumer acknowledgement advances state | Replaced with handler callback pattern — same contract, typed dispatch. |
| SD-ARUN-006 | No content loading in DiaAssetRuntime | Handlers perform loading; they live in game code, not the runtime. |
| SD-ARUN-008 | No DiaAssetCatalogue dependency | No new dependencies introduced. |
| SD-ARUN-006 | No content loading in DiaAssetRuntime | Enforcement (AC18-AC23) modifies subsystem modules (DiaSFML, DiaUI) and game-level handlers — DiaAssetRuntime itself gains no content-loading code or subsystem dependencies. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | State Machine | How does the rename from `Registered` to `Staged` and `Unloading` to `Unloaded` affect existing code? | Migration task: update all existing call sites. The states map 1:1 conceptually — `Registered` meant "known but not loaded" which is now `Staged`, and `Unloading` was the teardown state which becomes `Unloaded` (content released). |
| 2 | Handler Dispatch | What happens if no handler is registered for an asset's type prefix? | AssetRuntime auto-validates: checks file/folder exists at the resolved path. If exists → `Loaded`. If missing → `Failed` with error log. This supports handler-less types (config, entity, folder, manifest) and makes missing files visible without requiring explicit handlers. |
| 3 | Callback Timing | Can `Load` call the callback synchronously (before returning)? | Yes — this is the expected pattern for sync loads today. The callback interface is designed to work both sync and async. |
| 4 | Thread Safety | Are handler callbacks safe to call from any thread? | For now, all loads are synchronous and single-threaded (AC9 uses deferred module start, not async dispatch). Thread safety is a future concern when async loads are added. |
| 5 | State Machine | With the rename, what is the initial state after manifest load? | `Staged` — assets are known and path-resolved but not yet loaded. This replaces the old `Registered` state. |
| 6 | Retry | How does explicit retry work? Who calls it? | Game code calls a new `RetryAssetLoad(assetId)` on `AssetRuntime`. This re-invokes the handler and transitions `Failed -> Loading`. Not automatic — requires deliberate game-code decision. |
| 7 | Existing Listeners | Does `IAssetStateListener` still exist alongside `IAssetTypeHandler`? | `IAssetStateListener` is fully replaced. Handlers own load/unload. Debug tools query state via `GetAssetState` / DiaAPI commands — no passive observer interface needed. |
| 8 | AC12 | How specifically does the architecture not block DAG ordering? | The handler dispatch iterates a flat list today. A future layer could topologically sort assets before dispatching to handlers — handlers don't know or care about ordering, they just receive Load calls. No handler API change needed. |
| 9 | Enforcement | How is `TextureManager::LoadTexture` internalized — `friend`, nested access, or separate internal header? | `friend class TextureHandler` on `TextureManager`. Keeps the access explicit and grep-able. `RenderWindow::LoadTexture` is removed from public API entirely — handler accesses `TextureManager` directly. |
| 10 | Enforcement | What about existing code that calls `RenderWindow::LoadTexture` (SimProcessingUnit)? | Left in place intentionally to prove the assert fires. After verification, it's moved to DummyStage-owned code that loads via the asset pipeline. |
| 11 | Content Access | Can `GetTextureId` return a stale ID after `Unload`? | No — `Unload` removes the mapping. `GetTextureId` returns 0 for unloaded assets. The texture GPU resource is freed by `TextureManager::UnloadTexture`. |
| 12 | Validation Handlers | What does "validate exists" mean for folder assets? | `std::filesystem::is_directory(resolvedPath)` or equivalent. If the folder doesn't exist at the expected deploy path, handler calls `OnLoadFailed`. |
| 13 | Thread Safety | `TextureHandler::Load` calls `TextureManager::LoadTexture` which takes a mutex. Is this safe from the main thread? | Yes — all handler dispatch is synchronous from `RequestStageLoad` on the main thread today. The mutex in TextureManager exists for future async use but is uncontested in current usage. |
| 14 | Dependencies | Handlers live in DiaSFML/DiaUIUltralight — does this create a dependency from those modules on DiaAssetRuntime? | Yes — DiaSFML gains a dependency on DiaAssetRuntime for `IAssetTypeHandler`. This is acceptable: DiaSFML already depends on DiaCore, and DiaAssetRuntime is a lightweight library. The dependency is one-way (DiaSFML → DiaAssetRuntime). DiaAssetRuntime still has no dependency on DiaSFML. |
| 15 | Auto-validation | What counts as "exists" for auto-validation? | File assets: `fopen_s` succeeds (or `std::filesystem::exists`). Folder assets: `std::filesystem::is_directory`. Both checked against the absolute resolved path from the manifest. |
| 16 | Dependencies | DiaSFML gains a dependency on DiaAssetRuntime — is this acceptable? | Yes. DiaAssetRuntime is a lightweight leaf library (no transitive baggage). The dependency is only on `IAssetTypeHandler.h` (2-method interface). If layering becomes a concern later, the interface can be extracted to DiaCore as a trivial refactor. |

## Status

`Approved`
