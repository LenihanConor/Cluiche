# Feature Spec: TextureHandler Two-Phase Async Load

## Parent System
@docs/specs/systems/cluichetest/async-asset-loading.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | CluicheTest | @docs/specs/applications/cluichetest.md |
| System | Async Stage Asset Loading | @docs/specs/systems/cluichetest/async-asset-loading.md |
| Feature | TextureHandler Two-Phase Async Load | (this file) |

## Problem Statement

`Dia::SFML::TextureHandler::Load` (at `Dia/DiaSFML/TextureHandler.cpp:44–76`) currently does file I/O, image decode, and `sf::Texture` GPU upload in a single synchronous call on the AssetRuntime owner thread (Main). For a stage with N textures, the main thread blocks for the sum of all decode + upload times during stage transitions.

`sf::Texture::loadFromImage` is GL-affine — it must run on the thread that owns the GL context (Main). `sf::Image::loadFromFile` is pure CPU and thread-safe. Splitting the two allows parallel decode on JobSystem workers while keeping GL on Main, with no per-thread `sf::Context` fragility.

## Acceptance Criteria

1. `TextureHandler::Load` no longer calls `sf::Texture::loadFromFile` directly
2. **Cache hit path** (existing `mPathToId` lookup): unchanged — fires `OnLoadComplete` synchronously on caller thread
3. **Cache miss path:** creates a `JobSystem::CreateJob` + `Run` that decodes `sf::Image::loadFromFile` on a worker; pushes the result into a thread-safe `mPendingUploads` queue
4. **New public method `TextureHandler::Tick()`:** drains `mPendingUploads`, allocates `sf::Texture`, calls `texture->loadFromImage(image)`, registers IDs in existing maps (`mPathToId`, `mIdToTexture`, `mAssetToTextureId`) under existing `mMutex`, fires `OnLoadComplete(assetId)`
5. **Failure path** (decode failed): worker pushes a failure record; `Tick()` fires `OnLoadFailed(assetId, reason)` on Main (per SD-010)
6. `AssetServiceModule::DoUpdate(float)` calls `TextureHandler::Tick()` once per frame on Main (per SD-003)
7. `AssetServiceModule::DoStop` waits for outstanding texture-load jobs to complete before allowing `JobSystemModule::DoStop` to run (handled by manifest dependency: AssetService depends on JobSystem → AssetService stops first)
8. Existing `std::shared_mutex mMutex` continues to guard `mPathToId` / `mIdToTexture` / `mAssetToTextureId`. `Tick()` takes the unique lock; readers (Sim/Render PUs calling `GetTextureId`/`GetTexture`) take the shared lock — race-free
9. AssetRuntime's `AssertOwnerThread()` still passes for every callback fired (callbacks fire from `Tick()` on Main)
10. `dia.diasfml.architecture.module.md` updated to declare new dependency on `Dia::Core::Threading::JobSystem`

## Implementation Sketch

```cpp
// TextureHandler.h additions
class TextureHandler : public Dia::AssetRuntime::IAssetTypeHandler {
public:
    // Existing API unchanged.
    void Load(const Dia::Core::StringCRC& assetId,
              const Dia::Core::Containers::String512& resolvedPath,
              Dia::AssetRuntime::IAssetLoadCallback* callback) override;

    // New: drains decoded images, performs GL upload on caller's thread.
    // MUST be called on the AssetRuntime owner thread (Main).
    void Tick();

private:
    struct PendingUpload {
        Dia::Core::StringCRC assetId;
        Dia::Core::Containers::String512 resolvedPath;
        sf::Image image;            // populated on success
        Dia::AssetRuntime::IAssetLoadCallback* callback;
        bool      success;
        const char* failureReason;  // when !success; pointer to static string
    };

    std::mutex                  mPendingUploadsMutex;  // separate from mMutex
    std::vector<PendingUpload>  mPendingUploads;       // grows on workers, drains on Main
};
```

`Load` flow (cache-miss branch):

```cpp
// On caller thread (Main, AssetRuntime owner):
{
    std::unique_lock<std::shared_mutex> lock(mMutex);
    if (auto it = mPathToId.find(pathStr); it != mPathToId.end()) {
        unsigned int textureId = it->second;
        mAssetToTextureId[assetId.Value()] = textureId;
        // Cache hit — release lock before callback to avoid reentrancy.
        lock.unlock();
        callback->OnLoadComplete(assetId);
        return;
    }
}

// Cache miss — enqueue job. Capture by value; sf::Image moved on completion.
auto* job = Dia::Core::Threading::JobSystem::CreateJob(
    [this, assetId, resolvedPath, callback](Dia::Core::Threading::Job*) {
        sf::Image image;
        bool ok = image.loadFromFile(resolvedPath.AsCStr());

        std::lock_guard<std::mutex> lk(mPendingUploadsMutex);
        mPendingUploads.push_back({
            assetId, resolvedPath, std::move(image),
            callback, ok, ok ? nullptr : "failed to decode image"
        });
    });
Dia::Core::Threading::JobSystem::Run(job);
```

`Tick()`:

```cpp
void TextureHandler::Tick() {
    std::vector<PendingUpload> drained;
    {
        std::lock_guard<std::mutex> lk(mPendingUploadsMutex);
        drained.swap(mPendingUploads);
    }

    for (auto& p : drained) {
        if (!p.success) {
            p.callback->OnLoadFailed(p.assetId, p.failureReason);
            continue;
        }

        sf::Texture* texture = DIA_NEW(sf::Texture());
        if (!texture->loadFromImage(p.image)) {
            DIA_DELETE(texture);
            p.callback->OnLoadFailed(p.assetId, "failed to upload texture to GPU");
            continue;
        }

        unsigned int textureId;
        {
            std::unique_lock<std::shared_mutex> lock(mMutex);
            std::string pathStr(p.resolvedPath.AsCStr());
            // Re-check dedup: another job may have completed for the same path.
            if (auto it = mPathToId.find(pathStr); it != mPathToId.end()) {
                DIA_DELETE(texture);
                textureId = it->second;
            } else {
                textureId = mNextId++;
                mPathToId[pathStr] = textureId;
                mIdToTexture[textureId] = texture;
            }
            mAssetToTextureId[p.assetId.Value()] = textureId;
        }
        p.callback->OnLoadComplete(p.assetId);
    }
}
```

## Files Touched

| File | Action |
|------|--------|
| `Dia/DiaSFML/TextureHandler.h` | Update — add `Tick()`, add `PendingUpload` struct, add `mPendingUploads` + mutex |
| `Dia/DiaSFML/TextureHandler.cpp` | Rewrite `Load` body, add `Tick()` implementation |
| `Dia/DiaSFML/dia.diasfml.architecture.module.md` | Update `dependent_modules` to add `DiaCore/Threading/JobSystem` |
| `Cluiche/CluicheGameBaseline/Modules/AssetServiceModule.cpp` | Update `DoUpdate` — call `mTextureHandler.Tick()` |
| `Cluiche/Tests/GoogleTests/SFML/TestTextureHandler.cpp` | New test file — covers cache hit, async load, failure, concurrent decode |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` / `.filters` | Update — register new test file |

## Dependencies

- **DiaCore/Threading** — `Dia::Core::Threading::JobSystem::CreateJob`, `Run`
- **SFML** — `sf::Image::loadFromFile`, `sf::Texture::loadFromImage`
- **DiaAssetRuntime** — `IAssetLoadCallback` (existing seam, unchanged)
- **DiaCore/Containers** — `String512` (existing)
- **JobSystem Module** feature — provides initialised `JobSystem` at runtime (sibling feature)

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for IDs | Compliant — `assetId` carried as `StringCRC` end-to-end |
| PD-004 | Platform | No STL in public APIs | Compliant — public API (`Load`, `Tick`) unchanged signature; `std::vector<PendingUpload>` is private impl detail |
| PD-007 | Platform | C++20 | Compliant — uses lambdas, structured bindings |
| SD-002 (Async) | System | Two-phase: decode on worker, upload on Main | This feature **is** that contract |
| SD-003 (Async) | System | `AssetServiceModule::DoUpdate` calls `Tick()` | Compliant — single-line addition |
| SD-004 (Async) | System | AssetRuntime stays single-thread; callbacks marshalled to owner | Compliant — all `OnLoadComplete/OnLoadFailed` called from `Tick()` on Main |
| SD-009 (Async) | System | Cache hits fire callback synchronously, no job created | Compliant — see `Load` sketch |
| SD-010 (Async) | System | Decode failures fire `OnLoadFailed` from `Tick()`, not from worker | Compliant — failure record drained in `Tick()` |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Threading | Is `mPendingUploads` access truly race-free with separate `mPendingUploadsMutex`? | Yes — workers only push under `mPendingUploadsMutex`; `Tick()` swaps under the same mutex. The image data inside `PendingUpload` is moved to local `drained` and then accessed only by the calling thread. No concurrent access to image bytes. |
| 2 | Threading | Why a separate mutex for `mPendingUploads` instead of reusing `mMutex`? | `mMutex` (shared_mutex) is taken by readers (Sim/Render PUs reading texture IDs) every frame. Holding it for the workers' push would block the readers. A small dedicated `std::mutex` for the tiny push/swap critical section is faster and decoupled. |
| 3 | Lifetime | What if `TextureHandler` is destroyed while jobs are in flight? | `TextureHandler` is owned by `AssetServiceModule`. `AssetService` depends on `JobSystem` → `AssetService::DoStop` runs first. `AssetService::DoStop` MUST `JobSystem::Wait` on all outstanding decode jobs (or call a dedicated `TextureHandler::FlushPending()` that waits + drains) before destruction. Document in AssetServiceModule changes. |
| 4 | Lifetime | What if the same texture is requested twice while the first decode is in flight? | Two jobs are queued; both decode the same file; both reach `Tick()`. The re-check in `Tick()` (`if (mPathToId.find(pathStr) != end)`) catches this — second one frees its texture and reuses the first ID. Wasted decode work but correct. **Optimisation deferred:** add an "in-flight requests" set to short-circuit. Not worth it for the 3-texture case. |
| 5 | API | Should `Tick()` accept a per-frame upload budget (e.g., `Tick(int maxUploadsThisFrame)`)? | Not yet (per System SD-006 / AI Q6). Add when stage size grows. Today: drain everything per frame. |
| 6 | API | Why is `Tick()` public and not called from inside `Load`? | Caller (`AssetServiceModule`) decides *when* main-thread upload work happens. Pushing it inside `Load` would couple every load call to a synchronous frame tick. The public seam is cleaner and matches how other handlers (UIHandler) might join later. |
| 7 | Testing | How do we deterministically test the async path in GoogleTests? | Submit a load with a known-tiny PNG. Spin a loop calling `Tick()` until callback fires (or timeout). Assert `OnLoadComplete` got called with expected `assetId`. For determinism, use `JobSystem::Wait` on the decode job before calling `Tick()`. |
| 8 | Failure | What about the SFML `loadFromImage` returning false (decoded but GPU upload fails)? | Treated as failure; `OnLoadFailed(assetId, "failed to upload texture to GPU")`. Allocated `sf::Texture*` is `DIA_DELETE`d. The decoded `sf::Image` falls out of scope and frees its CPU buffer. |
| 9 | Module doc | Updating `dia.diasfml.architecture.module.md` to depend on JobSystem — does this risk a layering issue? | DiaSFML already depends on DiaCore (transitively through DiaCore types in its API). `JobSystem` is in `DiaCore/Threading/`. No new module-level dependency edge — just documenting an additional in-module use. Verify with `python Tools/dia_modules.py --validate` after the change. |
| 10 | Worker safety | `sf::Image::loadFromFile` is documented thread-safe, but does it touch any global SFML state? | Per SFML docs (sfml-graphics module), `sf::Image` is purely CPU — no GL state, no global state. Confirmed safe to call concurrently from multiple workers. |

## Open Questions

- Whether `AssetServiceModule::DoStop` already had any handler-flush logic, or whether a new `TextureHandler::FlushPending()` is needed (depends on prior asset-pipeline implementation; verify before coding).
- Whether `JobSystem::CreateJob`'s lambda is allowed to capture `IAssetLoadCallback*` by raw pointer (it should be — callback lives for AssetService duration).

## Status

`Approved` — 2026-05-17
