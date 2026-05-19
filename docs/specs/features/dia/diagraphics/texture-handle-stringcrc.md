# Feature Spec: texture-handle-stringcrc

## Parent System
@docs/specs/systems/dia/diagraphics.md

**Cross-cutting system:** @docs/specs/systems/dia/render-backend.md (RB-007, RB-009 — gates DiaBgfx work and the async-loader integration)

**Coordinates with:** @docs/specs/systems/cluichetest/async-asset-loading.md (Approved 2026-05-17 — that system's TextureHandler-Two-Phase-Load feature must be re-planned against the new ITexture API; details in this spec's *Coordination* section)

**Research:** @docs/research/render_backend_swap/summary.md

## Status
`Approved`

## Summary

Refactor `Dia::Graphics::ITexture` from a thin "wraps a `void*` native handle" interface into the canonical asset-aware texture handle for the engine. After this feature:

- `ITexture` carries a `StringCRC` asset id (the same id used in `DiaAssetCatalogue`), a size, and a backend-agnostic ready/loading/failed state.
- `Graphics::SpriteDrawCommand` holds an `ITexture*` (renderer-owned, asset-system-managed) instead of an `unsigned int` runtime id.
- The DiaSFML side replaces `TextureHandler::GetTextureId(StringCRC) → unsigned int` + `GetTexture(unsigned int) → const sf::Texture*` with a single `LookupTexture(StringCRC) → ITexture*` accessor backed by `Dia::SFML::SfmlTexture` (concrete `ITexture` impl wrapping `sf::Texture`).
- The async-asset-loader's two-phase pump produces `ITexture` instances directly; its callbacks remain unchanged.
- `unsigned int` runtime handles are deleted entirely from public APIs. PD-001 / RB-007 compliance restored.

This is the *first* implementation feature in the [render-backend system](../../../systems/dia/render-backend.md) Phase 1 (per RB-009): it must land before any DiaBgfx code, because `DiaBgfx::TextureHandle` is a second `ITexture` implementer and the SpriteDrawCommand path consumes `ITexture*` regardless of backend.

## Problem

Two structural problems in the current code:

1. **PD-001 / RB-007 regression** — `Dia::Graphics::SpriteDrawCommand::textureId` is `unsigned int` (a runtime allocation counter), not a `StringCRC`. Asset code holds `StringCRC` ids; render code holds `unsigned int` ids; a translation layer (`TextureHandler::GetTextureId`) is needed every time a stage sprite is queued. Two id concepts coexist for no benefit (StringCRC is itself a `uint32_t`).
2. **Backend leak** — `TextureHandler::GetTexture(unsigned int) → const sf::Texture*` exposes SFML through what should be a renderer-agnostic API. Per RB-006 this is forbidden in DiaBgfx, but the existing surface forces every consumer to be backend-aware.

The `ITexture` interface today is a stub (`GetSize()` + `GetNativeHandle() → const void*`) with no concrete implementer in production code. It is not used by `SpriteDrawCommand` and not produced by `TextureHandler`. We need it to actually carry the engine's texture concept end-to-end.

## Goals

- Redefine `Dia::Graphics::ITexture` to be the engine's canonical texture handle: asset id, size, ready state, no native-handle leak
- Add a `Dia::SFML::SfmlTexture` concrete impl that owns one `sf::Texture*`
- Replace `Dia::SFML::TextureHandler`'s `GetTextureId` / `GetTexture` accessors with `LookupTexture(StringCRC) → ITexture*`
- Change `Dia::Graphics::SpriteDrawCommand::textureId` from `unsigned int` to `Dia::Graphics::ITexture* texture` (renderer-owned pointer; lifetime managed by the asset system and the ProcessingUnit lifecycle)
- Update every call site that constructs/inspects `SpriteDrawCommand` (CluicheTest sprite emitters, tests)
- Update the async-loader's `texturehandler-two-phase-load` feature plan to produce `ITexture` instances and to pass them via the existing `IAssetLoadCallback` flow without changing that contract
- Preserve thread safety: `ITexture::IsReady()` is atomic-readable from any thread; `LookupTexture` is shared-mutex-guarded the same way `GetTextureId` is today
- Maintain zero-allocation in the per-frame submit path: `SpriteDrawCommand` storage is unchanged size class (an `ITexture*` is the same width as `unsigned int` on x64 only because of pointer size — note this is a sprite-storage size *increase* from 4 to 8 bytes per command; acceptable for the abstraction win)

## Non-Goals

- **bgfx implementation of ITexture** — `DiaBgfx::TextureHandle` lives in the `diabgfx-canvas-parity` feature. This feature defines the contract; the bgfx implementer comes later.
- **Async loading mechanism changes** — the worker-decode + main-thread-upload split (per `texturehandler-two-phase-load`) is unchanged. Only the *type* produced by `Tick()` changes from "register `unsigned int` in `mIdToTexture`" to "construct a `SfmlTexture` wrapping `sf::Texture*`".
- **Asset catalogue or runtime API changes** — `DiaAssetCatalogue` and `DiaAssetRuntime` are unchanged. `IAssetTypeHandler::Load` / `Unload` signatures are unchanged.
- **Texture sub-rect / atlas** — `SpriteDrawCommand::textureRect` is unchanged.
- **Texture eviction / streaming** — out of scope; `ITexture` lifetime continues to be tied to the asset's load/unload calls.
- **Hot-reload of textures** — out of scope; existing behaviour preserved.
- **Other asset types (shaders, fonts, audio)** — only `ITexture` here. `IShader` is part of `diabgfx-canvas-parity`.

## Coordination with `async-asset-loading` (CluicheTest)

The `async-asset-loading` system (Approved 2026-05-17) has a feature `texturehandler-two-phase-load` (Approved) that currently specifies an `unsigned int` runtime id flow. Under this feature's RB-009 commitment, that feature must be re-planned (not re-spec'd from scratch) to produce `ITexture` instances.

**Concrete coordination plan:**

1. This feature ships first.
2. After this feature is merged, `texturehandler-two-phase-load`'s **plan** is updated:
   - `mPendingUploads` carries `{StringCRC assetId, sf::Image image, IAssetLoadCallback* callback, success/reason}` (unchanged)
   - `Tick()` allocates a `SfmlTexture` (which internally allocates one `sf::Texture*`), calls `loadFromImage`, and registers the `SfmlTexture*` in `mAssetIdToTexture` (a new map replacing `mPathToId`/`mIdToTexture`/`mAssetToTextureId`)
   - The `IAssetLoadCallback::OnLoadComplete(assetId)` contract is unchanged. The caller now retrieves the texture via `TextureHandler::LookupTexture(assetId) → ITexture*`.
3. The `async-asset-loading` system spec gets a single line added to its *Decisions* table (a new `SD-013`) recording that `TextureHandler` produces `ITexture` instances. This is a one-line spec amendment, not a re-do.
4. The `async-asset-loading.plan.md` task list adds: "Re-plan `texturehandler-two-phase-load` against the new `ITexture` API once `texture-handle-stringcrc` is merged."

The async-asset-loader's *implementation* (job decode → main-thread upload, `Tick()` cadence, AssetServiceModule wiring, SD-001 through SD-012 decisions) is unchanged. Only the type produced changes.

If the async-loader's feature has *already started implementation* at the time this feature lands: pause the async-loader work, merge this feature, then resume the async-loader plan against the new types. The two implementations cannot interleave coherently because both touch `TextureHandler.h`.

## Public Interfaces

### `Graphics::ITexture` (refactored)

```cpp
// Dia/DiaGraphics/Assets/ITexture.h
namespace Dia::Graphics {

class ITexture {
public:
    enum class State : unsigned char {
        Pending = 0,   // queued; decode/upload not yet complete
        Ready   = 1,   // upload complete; texture is usable
        Failed  = 2    // load or upload failed; size may be (0,0)
    };

    virtual ~ITexture() = default;

    // Asset id this texture corresponds to. Stable for the texture's lifetime.
    virtual Dia::Core::StringCRC GetAssetId() const = 0;

    // Size in pixels. Returns (0,0) if state is Pending or Failed.
    virtual Dia::Maths::Vector2D GetSize() const = 0;

    // Atomic-readable from any thread. Renderers must check IsReady()
    // (or equivalently GetState() == State::Ready) before sampling.
    virtual State GetState() const = 0;
    bool IsReady() const { return GetState() == State::Ready; }

protected:
    ITexture() = default;

private:
    ITexture(const ITexture&) = delete;
    ITexture& operator=(const ITexture&) = delete;
};

} // namespace Dia::Graphics
```

`GetNativeHandle()` is **removed**. Renderers do not look up native handles through this interface; each renderer's concrete impl exposes its own typed accessor (e.g. `Dia::SFML::SfmlTexture::GetSfTexture() → const sf::Texture*`) for use *only* inside that renderer's translation unit.

### `Graphics::SpriteDrawCommand` (changed field)

```cpp
// Dia/DiaGraphics/Frame/SpriteDrawCommand.h
namespace Dia::Graphics {

struct SpriteDrawCommand {
    SpriteDrawCommand();
    SpriteDrawCommand(ITexture* tex, const Dia::Maths::Vector2D& pos);

    ITexture*               texture;     // CHANGED from `unsigned int textureId`
    Dia::Maths::Vector2D    position;
    Dia::Maths::Vector2D    scale;
    float                   rotation;
    RGBA                    tint;
    Dia::Maths::AARect2D    textureRect;
    Dia::Maths::Vector2D    origin;
    int                     layer;
    int                     subOrder;
};

} // namespace Dia::Graphics
```

The renderer is allowed to dereference `texture` only when `texture && texture->IsReady()` — sprites with a `Pending` or `Failed` texture are silently skipped. Sprites with `texture == nullptr` are also skipped (no implicit "missing texture" placeholder; that is a higher-level concern).

### `SFML::SfmlTexture` (new concrete impl)

```cpp
// Dia/DiaSFML/SfmlTexture.h
namespace Dia::SFML {

class SfmlTexture : public Dia::Graphics::ITexture {
public:
    SfmlTexture(Dia::Core::StringCRC assetId);
    ~SfmlTexture() override;

    // ITexture
    Dia::Core::StringCRC GetAssetId() const override { return mAssetId; }
    Dia::Maths::Vector2D GetSize() const override;
    State                GetState() const override { return mState.load(std::memory_order_acquire); }

    // SFML-side: lifecycle controlled by TextureHandler::Tick().
    bool UploadFromImage(const sf::Image& image);  // sets mState Ready or Failed
    void MarkFailed(const char* reason);

    // Renderer-internal accessor (DiaSFML translation units only).
    const sf::Texture* GetSfTexture() const { return mTexture; }

private:
    Dia::Core::StringCRC mAssetId;
    sf::Texture*         mTexture;        // owned; nullptr until UploadFromImage()
    std::atomic<State>   mState{State::Pending};
};

} // namespace Dia::SFML
```

### `SFML::TextureHandler` (refactored API)

```cpp
// Dia/DiaSFML/TextureHandler.h
namespace Dia::SFML {

class TextureHandler : public Dia::AssetRuntime::IAssetTypeHandler {
public:
    TextureHandler();
    ~TextureHandler();

    // Lookup the texture for an asset id. Returns nullptr if not loaded.
    // Thread-safe; takes shared lock. Result pointer is stable for the
    // texture's lifetime (until Unload(assetId)).
    Dia::Graphics::ITexture* LookupTexture(const Dia::Core::StringCRC& assetId) const;

    unsigned int GetLoadedCount() const;

    // IAssetTypeHandler
    void Load(const Dia::Core::StringCRC& assetId,
              const Dia::Core::Containers::String512& resolvedPath,
              Dia::AssetRuntime::IAssetLoadCallback* callback) override;
    void Unload(const Dia::Core::StringCRC& assetId) override;

    // Async-loader pump (defined in `texturehandler-two-phase-load`).
    void Tick();

private:
    void UnloadAll();

    mutable std::shared_mutex                                   mMutex;
    std::unordered_map<unsigned int /*StringCRC.Value()*/, SfmlTexture*> mAssetIdToTexture;
    // ... mPendingUploads queue (added by texturehandler-two-phase-load)
};

} // namespace Dia::SFML
```

`GetTextureId(StringCRC) → unsigned int` and `GetTexture(unsigned int) → const sf::Texture*` are **deleted**. Every caller is updated to use `LookupTexture` and to consume `ITexture*`.

## Implementation

### Files modified

```
Dia/DiaGraphics/Assets/ITexture.h
   - Replace interface body: remove GetNativeHandle, add GetAssetId, add State enum + GetState/IsReady
   - Update DiaGraphics.vcxproj.filters if needed (file unchanged location)

Dia/DiaGraphics/Frame/SpriteDrawCommand.h / .cpp
   - SpriteDrawCommand: textureId (unsigned int) -> texture (ITexture*)
   - Constructor signatures updated

Dia/DiaSFML/SfmlTexture.h / .cpp
   - NEW — concrete ITexture impl wrapping sf::Texture*

Dia/DiaSFML/TextureHandler.h / .cpp
   - Replace GetTextureId/GetTexture with LookupTexture
   - mIdToTexture/mPathToId/mAssetToTextureId -> mAssetIdToTexture (single map)
   - mNextId field deleted (no more runtime ids)

Dia/DiaSFML/EntityFrameRenderer.cpp
   - Sprite render path: cmd.textureId -> cmd.texture
   - Skip if !cmd.texture || !cmd.texture->IsReady()
   - Cast ITexture* -> SfmlTexture* (static_cast safe; only SFML producer)
   - Read sf::Texture* via SfmlTexture::GetSfTexture()

Cluiche/Stages/DummyStage/DummyLevelModule.cpp
   - GetTextureId(StringCRC) -> LookupTexture(StringCRC) -> ITexture*
   - Build SpriteDrawCommand with ITexture* directly

Cluiche/Tests/GoogleTests/DiaSFML/TestTextureHandler.cpp
   - Replace GetTextureId/GetTexture-based tests with LookupTexture-based tests

Cluiche/Tests/GoogleTests/Graphics/TestFrameData.cpp
   - cmd.textureId -> cmd.texture (use a stub ITexture* constant for tests)
   - May need a TestITexture helper added to Dia/DiaGraphics/Testing/

Dia/DiaGraphics/Testing/MockITexture.h
   - NEW — minimal ITexture stub for unit tests (returns fixed assetId, size, state)

Dia/DiaSFML/dia.sfml.architecture.module.md
   - Update public_api.entry_points: add SfmlTexture, remove TextureHandler internal types
   - Document new public method LookupTexture
```

### Migration order (within this feature's plan)

1. Add new `ITexture` interface (header-only change) and `MockITexture` test helper.
2. Add `SfmlTexture.h/.cpp`.
3. Add `LookupTexture` to `TextureHandler` *alongside* the existing `GetTextureId`/`GetTexture`. Both APIs coexist briefly.
4. Migrate `SpriteDrawCommand` to `ITexture*` field. Update `EntityFrameRenderer.cpp`.
5. Migrate every call site (CluicheTest sprite emitter, tests).
6. Delete `GetTextureId`/`GetTexture`, `mPathToId`, `mIdToTexture`, `mAssetToTextureId`, `mNextId` from `TextureHandler`.
7. Run full test suite (`dia run googletest`).
8. Update `dia.sfml.architecture.module.md` and any docs.

The intermediate state (steps 3–5) keeps the build green; the deletion in step 6 only happens once all consumers are off the old API.

### Concurrency notes

- `ITexture::GetState()` reads `std::atomic<State>` with `memory_order_acquire`. `SfmlTexture::UploadFromImage` performs the `sf::Texture` upload first, then stores `State::Ready` with `memory_order_release`. Render-thread `IsReady()` true implies the upload is fully visible.
- `TextureHandler::LookupTexture` takes `mMutex` shared lock (existing pattern). The returned `SfmlTexture*` is stable until `Unload(assetId)`. Callers must not retain it past an `Unload` of that asset.
- `SpriteDrawCommand` is copyable trivially (raw pointer field). FrameData copy semantics unchanged. The `ITexture*` is **not** ref-counted; `FrameData` may carry the pointer across the frame stream, but the texture is guaranteed to outlive any in-flight `FrameData` because `Unload` is gated by AssetRuntime ownership and the async loader's `JobSystem::Wait` on shutdown.

## Files Introduced / Modified

| File | Change |
|------|--------|
| `Dia/DiaGraphics/Assets/ITexture.h` | Refactored interface; remove `GetNativeHandle`; add `GetAssetId`, `GetSize`, `GetState`, `IsReady`, `State` enum |
| `Dia/DiaGraphics/Frame/SpriteDrawCommand.h` | `textureId` (unsigned int) → `texture` (ITexture*) |
| `Dia/DiaGraphics/Frame/SpriteDrawCommand.cpp` | Constructor signatures updated |
| `Dia/DiaGraphics/Testing/MockITexture.h` | NEW — minimal test stub |
| `Dia/DiaGraphics/DiaGraphics.vcxproj{,.filters}` | Add `Testing/MockITexture.h` |
| `Dia/DiaSFML/SfmlTexture.h` | NEW |
| `Dia/DiaSFML/SfmlTexture.cpp` | NEW |
| `Dia/DiaSFML/TextureHandler.h` | Replace `GetTextureId`/`GetTexture` with `LookupTexture`; collapse three maps to one |
| `Dia/DiaSFML/TextureHandler.cpp` | Body refactor matching header |
| `Dia/DiaSFML/EntityFrameRenderer.cpp` | Use `cmd.texture`; skip non-ready textures; static_cast to `SfmlTexture` |
| `Dia/DiaSFML/DiaSFML.vcxproj{,.filters}` | Add `SfmlTexture.h/.cpp` |
| `Dia/DiaSFML/dia.sfml.architecture.module.md` | Update public_api entry_points |
| `Cluiche/Stages/DummyStage/DummyLevelModule.cpp` | Use `LookupTexture` and `ITexture*` field |
| `Cluiche/Tests/GoogleTests/DiaSFML/TestTextureHandler.cpp` | Tests rewritten against `LookupTexture` |
| `Cluiche/Tests/GoogleTests/Graphics/TestFrameData.cpp` | Tests use `MockITexture` instances |
| `Cluiche/Tests/GoogleTests/Graphics/TestRenderStates.cpp` | Inspect for `textureId` references; update if any |
| `docs/specs/systems/cluichetest/async-asset-loading.md` | Add SD-013 noting TextureHandler produces ITexture instances |
| `docs/specs/features/cluichetest/async-asset-loading/texturehandler-two-phase-load.md` | (After this feature merges) Update plan section to integrate against ITexture |

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `bgfx-env-setup` | None | Independent of bgfx; can land before or after |
| `texturehandler-two-phase-load` (async-asset-loading) | Soft | This feature lands first per RB-009; that feature's plan re-targets ITexture afterwards |
| `asset-lifecycle-management` (Done) | Hard | Provides `IAssetTypeHandler` interface and `IAssetLoadCallback` contract — unchanged by this feature |

## Acceptance Criteria

1. `Dia::Graphics::ITexture::GetNativeHandle()` is removed; `GetAssetId()`, `GetSize()`, `GetState()`, `IsReady()`, and `State` enum are present
2. `Dia::SFML::SfmlTexture` exists, compiles, and is the sole production implementer of `ITexture`
3. `Dia::SFML::TextureHandler::GetTextureId` and `GetTexture(unsigned int)` are removed; `LookupTexture(StringCRC) → ITexture*` is the only accessor
4. `Dia::Graphics::SpriteDrawCommand::texture` is `ITexture*`; `textureId` field is gone
5. `EntityFrameRenderer` skips sprites with `texture == nullptr` or `!texture->IsReady()` (no crash, no flicker)
6. `dia run googletest` is green; all `TestTextureHandler` and `TestFrameData` tests rewritten against the new API
7. `Cluiche/Stages/DummyStage/DummyLevelModule.cpp` builds and DummyStage renders the three test sprites correctly when run via `dia run cluichetest`
8. `dia.sfml.architecture.module.md` reflects the new `SfmlTexture` entry point and the removal of internal handler types from public surface
9. No `unsigned int` runtime texture ids remain anywhere in the engine or game code (verified by `grep -r "GetTextureId\|GetTexture(unsigned"` returning zero matches)
10. `ITexture::IsReady()` is callable from any thread without lock (verified by inspection: `mState` is `std::atomic`)
11. `MockITexture` test helper is usable from any DiaGraphics test
12. `async-asset-loading` system spec gains a one-line `SD-013` decision recording the `ITexture` integration; its plan adds a re-plan task for the two-phase-load feature
13. Build clean under `/std:c++20` with zero new warnings
14. Performance: per-frame sprite submit cost is unchanged within noise (existing `TestFrameData` micro-benchmarks pass; sprite storage grows from 4 to 8 bytes per command — acceptable per Goals)

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System (primary) | DiaGraphics | @docs/specs/systems/dia/diagraphics.md |
| System (cross-cutting) | RenderBackend | @docs/specs/systems/dia/render-backend.md |
| System (coordinated) | Async Stage Asset Loading | @docs/specs/systems/cluichetest/async-asset-loading.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Compliant — ITexture carries `StringCRC` asset id; `LookupTexture` keyed by `StringCRC`; `unsigned int` runtime ids deleted from public APIs |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — no PU/Phase changes; existing Render PU consumes ITexture via SpriteDrawCommand |
| PD-003 | Platform | Component-based entities | Compliant — sprites remain component-driven; only the texture field's *type* changes |
| PD-004 | Platform | No STL containers in public APIs | Compliant — `ITexture`, `SpriteDrawCommand`, `SfmlTexture` public APIs use `StringCRC`, `Vector2D`, raw enum, `ITexture*`. `TextureHandler::mAssetIdToTexture` (`std::unordered_map`) is private/internal — same pattern as existing `mIdToTexture` |
| PD-005 | Platform | x64 only | Compliant — `ITexture*` is 8 bytes on x64; SpriteDrawCommand width grows from 4→8 bytes for the texture field, accepted per Goals |
| PD-006 | Platform | Visual Studio project files are source of truth | Compliant — `SfmlTexture.h/.cpp` and `MockITexture.h` added to respective `.vcxproj{,.filters}` manually |
| PD-007 | Platform | C++20 required | Compliant — uses `std::atomic`, `enum class`, default-constructed inheritance |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — no per-project overrides |
| PD-009 | Platform | Generated output under `Cluiche/out/<AppName>/` | N/A — no generated output |
| PD-010 | Platform | `.diagame` typed imports | N/A — no manifest changes |
| AD-001 | Dia App | Module YAML frontmatter documentation | Compliant — `dia.sfml.architecture.module.md` updated |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | Compliant — `Dia::Graphics::`, `Dia::SFML::` |
| AD-004 | Dia App | ProcessingUnit/Phase/Module for app structure | N/A |
| AD-005 | Dia App | Component-based entities | N/A |
| GD-001 | DiaGraphics | DebugPrimitive is hand-rolled tagged union | N/A — DebugPrimitive untouched |
| GD-002 | DiaGraphics | FrameData copy must be trivially correct — no pointer members in debug buffers | **Compliant with caveat** — `SpriteDrawCommand::texture` is a raw pointer member of `EntityFrameData`, NOT `DebugFrameData`. GD-002 explicitly scopes "no pointer members" to *debug* buffers. EntityFrameData has always carried trivially-copyable members; raw pointers are trivially copyable; FrameData copy semantics are preserved. ITexture lifetime guarantees outlive any in-flight FrameData per the Concurrency Notes section |
| GD-003 | DiaGraphics | Debug renderer is a separate concern | Compliant — sprite rendering is the Entity path; debug path unchanged |
| GD-004 | DiaGraphics | Debug primitives are stored in insertion order | N/A |
| RB-006 | RenderBackend | No backend types in DiaGraphics public surface | Compliant — `ITexture` no longer exposes `GetNativeHandle()`; `sf::Texture*` is reachable only through `SfmlTexture::GetSfTexture()` which is a `Dia::SFML::` accessor not in DiaGraphics |
| RB-007 | RenderBackend | ITexture/IShader keyed by StringCRC | Compliant — this feature implements exactly that for ITexture |
| RB-009 | RenderBackend | Async loading sequencing: ITexture refactor first, then DiaBgfx::TextureHandle implements it | Compliant — this feature is the first half of RB-009; the *Coordination* section captures the second half |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Lifetime | What guarantees `ITexture*` in `SpriteDrawCommand` outlives the in-flight FrameData being rendered? | The `ITexture` instance is owned by `TextureHandler::mAssetIdToTexture` and lives until `Unload(assetId)`. Unload is called from `AssetServiceModule::DoStop` (on shutdown) or stage transition unload (which only runs after the prior stage's render frames are flushed). The Render PU's frame stream is drained before any handler `Unload` call by the existing ProcessingUnit shutdown ordering. No new lifetime contract — same guarantee that already protects the existing `unsigned int → sf::Texture*` lookup. |
| 2 | GD-002 | Does adding a pointer member to `SpriteDrawCommand` violate the trivially-copyable invariant on `FrameData`? | `SpriteDrawCommand` lives in `EntityFrameData`, not `DebugFrameData`. GD-002's "no pointer members" applies to debug buffers (and is motivated by the rewind/replay seam in SD-DBG-009). Raw pointers are still trivially copyable; `FrameData::operator=` and `Copy()` continue to work. The replay seam is unaffected because Entity sprites are not part of the debug rewind story (sprites would need their full asset state captured separately if rewind ever extends to entities). |
| 3 | Async coordination | What if the async-loader feature has already been partially implemented when this feature lands? | Pause the async-loader work; merge this feature; resume the async-loader plan against the new ITexture API. Both touch `TextureHandler.h`; concurrent implementation produces a merge conflict and inconsistent intermediate states. Coordinate the order in the dispatch board / plan. |
| 4 | static_cast | `EntityFrameRenderer.cpp` static_casts `ITexture*` to `SfmlTexture*`. Is that safe? | Yes: `EntityFrameRenderer` lives in `DiaSFML.vcxproj`. The only producer of `ITexture` instances inside CluicheTest is `TextureHandler`, which produces `SfmlTexture`. `DiaBgfx` will produce `BgfxTextureHandle` instances, but those will never reach `DiaSFML::EntityFrameRenderer` because the bgfx feature deletes that file (per RB-016 ship gate). Until that deletion, `static_cast` is safe by construction. Add a `DIA_ASSERT(dynamic_cast<SfmlTexture*>(cmd.texture))` in DEBUG builds to fail loudly if the invariant ever breaks. |
| 5 | Sprite skip | Should non-ready sprites silently skip, or render a placeholder magenta texture? | Silently skip. Placeholders are a higher-level UX decision (e.g. an editor wants visible "missing" markers) and would belong in a future debug layer (`DiaVisualDebugger` already has the infrastructure to render rectangles for missing assets). The renderer's job is "draw what's ready"; everything else is policy. |
| 6 | Test stub | Should `MockITexture` live in `DiaGraphics/Testing/` or `DiaGraphics/Tests/` or in the GoogleTests project? | `Dia/DiaGraphics/Testing/` — per the test-utilities-ship-with-libraries memory: test helpers live alongside their library, not in GoogleTests. Same pattern as the existing `Dia/DiaGraphics/Testing/MockVisitors.h`. |
| 7 | Storage size | SpriteDrawCommand grew from 4 to 8 bytes for the texture field on x64. Does that matter for `EntityFrameData`'s `DynamicArrayC<SpriteDrawCommand, 256>`? | The total `SpriteDrawCommand` struct grows by 4 bytes (with padding likely 8). For 256 sprites, that's an extra 1–2 KB per frame at worst. Negligible vs the existing struct size (~64 bytes). No pooling/cache-line concerns triggered. |
| 8 | Removal of GetNativeHandle | Anything outside DiaSFML currently calls `ITexture::GetNativeHandle()`? | Audit: the interface today has no production implementer (the interface is used as a contract, not produced by any handler). `GetNativeHandle()` callers in the codebase: zero. Removing it is a no-op for current consumers and a correction for the contract. |
| 9 | LookupTexture nullptr semantics | What does `LookupTexture(unknownAssetId)` return — nullptr, or a "Failed" placeholder ITexture? | nullptr. Distinguishing "not loaded yet" from "load failed" from "wrong id" is the caller's job, and the runtime already tracks per-asset state. `SpriteDrawCommand` skip-on-nullptr handles this uniformly. |
| 10 | dynamic_cast in DEBUG | Why DEBUG-only assertion instead of always-on `dynamic_cast`? | `dynamic_cast` requires RTTI and incurs a per-call cost. The release build's invariant is enforced at the architecture level: the only reachable producers of `ITexture` for `DiaSFML::EntityFrameRenderer` are `SfmlTexture` instances (post-bgfx-cutover, EntityFrameRenderer is deleted). DEBUG-mode RTTI assertion catches refactor mistakes; release skips the cost. Same pattern as `DIA_ASSERT` elsewhere. |
| 11 | Async load callback wait | The async-loader (per existing system) has `AssetServiceModule::DoStop` waiting for outstanding loads. Does this feature change that contract? | No. `DoStop` continues to `JobSystem::Wait` on outstanding decode jobs before destroying the runtime. The only difference: when `Tick()` runs in the wind-down, it produces `SfmlTexture` instances that are immediately destructible (the runtime is shutting down). Existing teardown ordering preserved. |
| 12 | Sprite cmd default ctor | `SpriteDrawCommand()` defaults `texture` to nullptr. Renderer skips nullptr sprites. Is that the right default? | Yes — matches existing behaviour: today `textureId` defaults to 0, and `EntityFrameRenderer` skips id=0 sprites. nullptr-default preserves "default-constructed sprite renders nothing" without changing semantics. |
| 13 | Architecture doc | Should `dia.graphics.architecture.module.md` get a public_api entry for the State enum and IsReady? | Yes — add `Dia::Graphics::ITexture::State` to `entry_points`. The interface is part of DiaGraphics' contract and consumers (DiaSFML, future DiaBgfx) implement it. |

---
