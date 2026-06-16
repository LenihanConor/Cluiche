# Feature Spec: canvas-parity

## Parent System
@docs/specs/applications/dia/systems/render-backend/render-backend.md

**Research:** @docs/research/render_backend_swap/summary.md

## Status
`Done`

## Summary

Create the new `Dia/DiaBgfx/` module and implement `Dia::Bgfx::Canvas`, a concrete `Dia::Graphics::ICanvas` built on bgfx that reaches **functional parity** with `Dia::SFML::RenderWindow`'s render path for sprites, debug primitives, and UI overlay compositing. After this feature:

- A new `DiaBgfx.vcxproj` static library exists, links against `External/bgfx/lib/<config>/bgfx.lib`, and includes headers from `External/bgfx/include/`
- `Dia::Bgfx::Canvas` implements `ICanvas` only — **not** `IWindow` or `IInputSource` (per RB-004); SFML keeps owning those throughout Phase 1
- `DiaSFML::Window` (renamed from `RenderWindow` in this feature) hands its native HWND to `Bgfx::Canvas::AttachToNativeWindow(...)` at app wire-up
- `Bgfx::Canvas` consumes `FrameData` directly via three internal sub-renderers: `SpriteRenderer`, `DebugRenderer`, `UIOverlayRenderer` (no visitor pattern, per RB-005)
- `BgfxTextureHandle` implements `Dia::Graphics::ITexture` (the second implementer alongside `SfmlTexture`)
- `BgfxUIRenderOverlay` implements `Dia::UI::IUIRenderOverlay` (the second implementer alongside `SfmlUIRenderOverlay`)
- The first six `.sc` shader files arrive under `Dia/DiaBgfx/Shaders/` — three vertex/fragment pairs for sprite, debug, and UI overlay
- bgfx's default backend is selected at `Init` time via `pipeline.toml` config; D3D11 vs D3D12 deferred per RB-012 (default chosen here as D3D11 for stability)

This feature does **not** delete the SFML render path. Both `DiaSFML::Window::ProcessFrame` and `Bgfx::Canvas::ProcessFrame` exist side-by-side; the active renderer is selected at app construction. The SFML render path is deleted in `diasfml-render-removal` once parity is verified end-to-end.

## Problem

The system spec (`render-backend.md`) commits to bgfx as the new GPU abstraction. Every Phase 1 feature so far prepares the seam (`bgfx-env-setup`, `texture-handle-stringcrc`, `render-overlay-surface`, `bgfx-shader-cook`) but no actual rendering on bgfx exists. This feature is the central work item: *make bgfx draw what SFML draws today*.

The constraints are tight. We need:
- bgfx-side equivalents of `EntityFrameRenderer` (sprite batch with texture-keyed batching), `DebugFrameRendererVisitor` (eight `DebugPrimitive` shape variants), and `SfmlUIRenderOverlay` (UI buffer composite)
- The `ICanvas::StartFrame`/`ProcessFrame`/`EndFrame` lifecycle satisfied with bgfx's view-and-frame model
- Multi-swapchain support (per AI review Q3 in the system spec — CluicheEditor needs editor-chrome + game-viewport swapchains)
- A texture pipeline that produces `BgfxTextureHandle` instances in the existing `TextureHandler` two-phase pattern (worker decode → main upload, but bgfx's `bgfx::createTexture2D` *can* be called from any thread that has called `bgfx::renderFrame`, so the upload-on-main constraint loosens slightly — see Threading section)
- All shaders cooked via `diapipeline-shaderc-cook` and loaded at `Canvas::Initialize` time

The risk is doing too much in one feature; the answer is splitting parity verification across the three sub-renderers and keeping each one's acceptance criterion concrete and testable.

## Goals

- Create `Dia/DiaBgfx/` module with `dia.bgfx.architecture.module.md` YAML frontmatter
- Implement `Dia::Bgfx::Canvas` implementing `Graphics::ICanvas`
  - `Initialize(Settings)` — initialise bgfx with selected backend, allocate views, load cooked shaders
  - `AttachToNativeWindow(SystemHandle hwnd, Vector2D size)` — bgfx-specific bring-up step called by app wire-up
  - `SetCanvasSize(size)` — recreate framebuffer at new size; propagate to sub-renderers
  - `SetActiveContext(active)` — bgfx noop (bgfx is multi-threaded internally; no GL context concept)
  - `StartFrame(FrameData)` — `bgfx::touch(viewId)`, ImGui `NewFrame` if DEBUG
  - `ProcessFrame(FrameData)` — invoke each sub-renderer in order: SpriteRenderer, DebugRenderer, UIOverlayRenderer
  - `EndFrame(FrameData)` — ImGui `Render` if DEBUG, `bgfx::frame()` to present
- Implement `Dia::Bgfx::SpriteRenderer` — consumes `EntityFrameData`, batches sprites by `ITexture*` key, builds dynamic vertex/index buffers, single draw call per texture, sub-order respected
- Implement `Dia::Bgfx::DebugRenderer` — consumes `DebugFrameData`, dispatches per-primitive-type into shape-specific dynamic-buffer batches (lines, triangles, points), one draw call per shape category. All eight `DebugPrimitive` variants (Circle2D, Line2D, Point2D, Rect2D, Arc2D, Ray2D, Triangle2D, Text2D) covered. Text2D uses bgfx's debug font (`bgfx::dbgTextPrintf`) for parity with SFML's font path
- Implement `Dia::Bgfx::UIOverlayRenderer` implementing `UI::IUIRenderOverlay` — uploads `UIDataBuffer` bytes into a bgfx 2D texture each frame, draws a fullscreen quad with the UI overlay shader
- Implement `Dia::Bgfx::BgfxTextureHandle` implementing `Graphics::ITexture` — owns a `bgfx::TextureHandle`, exposes asset id + size + state
- Add a `BgfxTextureHandler` (sibling of `SfmlTexture`) that the renderer-agnostic `TextureHandler` interface produces when the active backend is bgfx — but this is internal: the public `TextureHandler` remains in DiaSFML for Phase 1 (it's the asset handler, not a renderer concept) and the async-loader's `Tick()` is extended to dispatch decode results to either `SfmlTexture::UploadFromImage` or `BgfxTextureHandle::UploadFromMemory` based on a runtime flag
- Add the six initial `.sc` shaders under `Dia/DiaBgfx/Shaders/`:
  - `vs_sprite.sc` / `fs_sprite.sc` — textured quad with tint
  - `vs_debug.sc` / `fs_debug.sc` — per-vertex colour, no texture (lines, triangles, points)
  - `vs_ui_overlay.sc` / `fs_ui_overlay.sc` — equivalent to today's `ui.frag` (composites UI texture over backbuffer)
  - One `varying.def.sc` per shader directory
- Add app wire-up changes in CluicheTest's `Main.cpp`/kernel-equivalent: construct `DiaSFML::Window` first, construct `Bgfx::Canvas`, call `AttachToNativeWindow(window->GetSystemHandle(), window->GetSize())`
- Multi-swapchain confirmed feasible on bgfx (the system spec's AI review Q3 acceptance) — verified by a synthetic test that creates a second `Bgfx::Canvas` instance attached to a separate HWND and renders different content to each
- Performance parity goal: CluicheTest DummyStage frame time within ±20% of the SFML baseline. Tighter perf work (instancing, persistent buffers) is a later optimisation
- All existing visual debuggers (rig2d, rigidbody2d, softbody2d, ik2d, geometry2d, animation2d) render correctly through `Bgfx::Canvas::DebugRenderer`

## Non-Goals

- **Deleting the SFML render path** — that's the next feature (`diasfml-render-removal`); both paths must coexist throughout this feature so parity can be measured
- **ImGui backend on bgfx** — separate feature `diabgfx-imgui-backend`; until that lands, ImGui debug overlays are disabled when running on bgfx (a runtime flag suppresses `Dia::ImGui::NewFrame`/`Render`)
- **3D rendering** — Phase 2 work; this feature explicitly does not add `Mesh3DDrawCommand` consumption
- **Renderer selection in `pipeline.toml`** — the active renderer (SFML vs bgfx) is selected by a build-time `#define` for the duration of this feature; runtime selection is too much surface for parity work. The `diasfml-render-removal` feature deletes the SFML option entirely so the toggle becomes irrelevant
- **bgfx render thread mode** — bgfx's optional dedicated render thread (`BGFX_CONFIG_MULTITHREADED`) is left at bgfx's compile-time default; we don't pin it on or off
- **Persistent vertex buffers** — Phase 1 uses `bgfx::TransientVertexBuffer` for sprites and debug; this is bgfx's recommended path for per-frame data
- **Custom debug font** — Text2D uses bgfx's built-in debug font (`bgfx::dbgTextPrintf`). Same parity bar as SFML's default font; styled fonts are a future feature
- **Hot-reload** — out of scope (matches `bgfx-shader-cook`'s non-goals)
- **Shadow maps, lighting, post-processing** — Phase 2

## Active backend selection

Per RB-012, the default Windows backend is deferred to spec time. **Choosing D3D11 as default for this feature** because:

- D3D11 is the most mature bgfx backend on Windows
- PIX and RenderDoc both work cleanly on D3D11
- D3D12 introduces descriptor-heap pressure with many UI sprites and is harder to debug at parity time
- Switching to D3D12 later is a one-line `bgfx::Init` change

`pipeline.toml` `[bgfx]` section adds:

```toml
[bgfx]
default_backend = "dx11"   # dx11 | dx12 | vulkan
```

Read at `Bgfx::Canvas::Initialize` via the existing pipeline-config Python loader exposed to the C++ side via... actually, this is a C++-side decision at startup. The simplest path: a `BGFX_DEFAULT_BACKEND` `#define` in `Cluiche.sln`'s shared property sheet (`Directory.Build.props`?) — no, PD-008 forbids overrides there. Instead: a `Dia::Bgfx::CanvasSettings` struct with a `RendererType` enum field, defaulted to `Direct3D11`, settable from app wire-up. The `pipeline.toml` value is consumed by `dia run` which sets an environment variable that `Main.cpp` reads at startup. Captured in the implementation section.

## Public Interfaces

### `Dia::Bgfx::Canvas`

```cpp
// Dia/DiaBgfx/Canvas.h
#pragma once

#include <DiaGraphics/Interface/ICanvas.h>
#include <DiaWindow/Interface/SystemHandle.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia { namespace UI { class IUIRenderOverlay; } }

namespace Dia
{
    namespace Bgfx
    {
        class SpriteRenderer;
        class DebugRenderer;
        class UIOverlayRenderer;

        enum class RendererType : unsigned char {
            Direct3D11 = 0,
            Direct3D12 = 1,
            Vulkan     = 2
        };

        struct CanvasSettings : public Dia::Graphics::ICanvas::Settings
        {
            CanvasSettings();

            RendererType         rendererType;
            Dia::Maths::Vector2D initialSize;
            // Path to cooked shader root (e.g. "Cluiche/out/cluichetest/shaders").
            // Resolved per active backend at Initialize time.
            const char*          cookedShaderRoot;
        };

        class Canvas : public Dia::Graphics::ICanvas
        {
        public:
            Canvas();
            ~Canvas() override;

            // ICanvas
            void Initialize(const Dia::Graphics::ICanvas::Settings& settings) override;
            void SetCanvasSize(const Dia::Maths::Vector2D& size) override;
            void SetActiveContext(bool active) override;
            void StartFrame(const Dia::Graphics::FrameData& nextFrame) override;
            void ProcessFrame(const Dia::Graphics::FrameData& nextFrame) override;
            void EndFrame(const Dia::Graphics::FrameData& nextFrame) override;

            // Bgfx-specific bring-up: caller (DiaSFML::Window) supplies the HWND.
            // Must be called BEFORE Initialize.
            void AttachToNativeWindow(Dia::Window::SystemHandle hwnd,
                                     const Dia::Maths::Vector2D& size);

            // UI overlay surface accessor (consumed by app wire-up to register with FrameData → UI flow).
            Dia::UI::IUIRenderOverlay* GetUIRenderOverlay();

        private:
            Dia::Window::SystemHandle mHwnd;
            Dia::Maths::Vector2D      mSize;
            RendererType              mRendererType;
            unsigned short            mEntityViewId;       // bgfx view id for sprites
            unsigned short            mDebugViewId;        // bgfx view id for debug primitives
            unsigned short            mUIViewId;           // bgfx view id for UI overlay

            SpriteRenderer*           mSpriteRenderer;
            DebugRenderer*            mDebugRenderer;
            UIOverlayRenderer*        mUIOverlayRenderer;

            bool                      mInitialised;
        };
    }
}
```

### `Dia::Bgfx::SpriteRenderer`

```cpp
// Dia/DiaBgfx/Renderers/SpriteRenderer.h
namespace Dia { namespace Graphics { class EntityFrameData; class ITexture; } }

namespace Dia { namespace Bgfx {

class ShaderProgram;

class SpriteRenderer
{
public:
    SpriteRenderer(unsigned short viewId, ShaderProgram* spriteProgram);
    ~SpriteRenderer();

    void OnCanvasSizeChanged(const Dia::Maths::Vector2D& size);
    void Draw(const Dia::Graphics::EntityFrameData& sprites);

private:
    unsigned short  mViewId;
    ShaderProgram*  mSpriteProgram;     // not owned
    // ortho projection matrix, etc.
};

} }
```

### `Dia::Bgfx::DebugRenderer`

```cpp
// Dia/DiaBgfx/Renderers/DebugRenderer.h
namespace Dia { namespace Graphics { class DebugFrameData; struct DebugPrimitive; } }

namespace Dia { namespace Bgfx {

class DebugRenderer
{
public:
    DebugRenderer(unsigned short viewId, ShaderProgram* debugProgram);
    ~DebugRenderer();

    void OnCanvasSizeChanged(const Dia::Maths::Vector2D& size);
    void Draw(const Dia::Graphics::DebugFrameData& data);

private:
    void EmitCircle  (const Dia::Graphics::DebugPrimitive& p, /* batches */);
    void EmitLine    (const Dia::Graphics::DebugPrimitive& p, /* batches */);
    void EmitPoint   (const Dia::Graphics::DebugPrimitive& p, /* batches */);
    void EmitRect    (const Dia::Graphics::DebugPrimitive& p, /* batches */);
    void EmitArc     (const Dia::Graphics::DebugPrimitive& p, /* batches */);
    void EmitRay     (const Dia::Graphics::DebugPrimitive& p, /* batches */);
    void EmitTriangle(const Dia::Graphics::DebugPrimitive& p, /* batches */);
    void EmitText    (const Dia::Graphics::DebugPrimitive& p, /* batches */);

    unsigned short mViewId;
    ShaderProgram* mDebugProgram;
    // line-list batch, triangle-list batch, point-list batch — bgfx::TransientVertexBuffer per call
};

} }
```

### `Dia::Bgfx::UIOverlayRenderer`

```cpp
// Dia/DiaBgfx/Renderers/UIOverlayRenderer.h
#include <DiaUI/IUIRenderOverlay.h>

namespace Dia { namespace Bgfx {

class UIOverlayRenderer : public Dia::UI::IUIRenderOverlay
{
public:
    UIOverlayRenderer(unsigned short viewId, ShaderProgram* uiOverlayProgram);
    ~UIOverlayRenderer() override;

    // IUIRenderOverlay
    void OnCanvasSizeChanged(const Dia::Maths::Vector2D& size) override;
    void Composite(const Dia::UI::UIDataBuffer& buffer) override;

private:
    unsigned short      mViewId;
    ShaderProgram*      mUIOverlayProgram;
    // bgfx::TextureHandle mUIOverlayTex;  (recreated on size change)
    // fullscreen quad vertex buffer
};

} }
```

### `Dia::Bgfx::BgfxTextureHandle`

```cpp
// Dia/DiaBgfx/Resources/BgfxTextureHandle.h
#include <DiaGraphics/Assets/ITexture.h>

namespace Dia { namespace Bgfx {

class BgfxTextureHandle : public Dia::Graphics::ITexture
{
public:
    BgfxTextureHandle(Dia::Core::StringCRC assetId);
    ~BgfxTextureHandle() override;

    // ITexture
    Dia::Core::StringCRC GetAssetId() const override { return mAssetId; }
    Dia::Maths::Vector2D GetSize() const override { return mSize; }
    State                GetState() const override { return mState.load(std::memory_order_acquire); }

    // Async loader pump (called from main thread via TextureHandler::Tick).
    bool UploadFromMemory(const unsigned char* rgbaPixels, unsigned int width, unsigned int height);
    void MarkFailed(const char* reason);

    // Renderer-internal accessor (DiaBgfx translation units only).
    unsigned short GetBgfxHandle() const { return mBgfxHandle; }

private:
    Dia::Core::StringCRC mAssetId;
    Dia::Maths::Vector2D mSize;
    unsigned short       mBgfxHandle;     // bgfx::TextureHandle::idx; bgfx::kInvalidHandle until UploadFromMemory()
    std::atomic<State>   mState{State::Pending};
};

} }
```

### `Dia::Bgfx::ShaderProgram`

```cpp
// Dia/DiaBgfx/Resources/ShaderProgram.h
namespace Dia { namespace Bgfx {

// Loads a vs/fs pair from cooked shader binaries on disk and produces a bgfx program.
// Lifetime: owned by Canvas; one ShaderProgram per shader pair (sprite, debug, ui_overlay).
class ShaderProgram
{
public:
    ShaderProgram();
    ~ShaderProgram();

    // Load cooked .bin files from `<root>/<backend>/<vs|fs>_<name>.bin`.
    // Returns false on missing file or bgfx createShader failure.
    bool Load(const char* cookedShaderRoot,
              const char* backendSubdir,
              const char* vsName,
              const char* fsName);

    unsigned short GetProgramHandle() const { return mProgram; }

private:
    unsigned short mVS;
    unsigned short mFS;
    unsigned short mProgram;
};

} }
```

## Implementation

### Files introduced

```
Dia/DiaBgfx/                                   NEW MODULE
├── DiaBgfx.vcxproj                            NEW
├── DiaBgfx.vcxproj.filters                    NEW
├── dia.bgfx.architecture.module.md            NEW
├── Canvas.h                                   NEW
├── Canvas.cpp                                 NEW
├── Renderers/
│   ├── SpriteRenderer.h / .cpp                NEW
│   ├── DebugRenderer.h / .cpp                 NEW
│   └── UIOverlayRenderer.h / .cpp             NEW
├── Resources/
│   ├── BgfxTextureHandle.h / .cpp             NEW
│   └── ShaderProgram.h / .cpp                 NEW
├── Shaders/
│   ├── varying.def.sc                         NEW (one per directory; could be shared root if all shaders use same varyings)
│   ├── vs_sprite.sc                           NEW
│   ├── fs_sprite.sc                           NEW
│   ├── vs_debug.sc                            NEW
│   ├── fs_debug.sc                            NEW
│   ├── vs_ui_overlay.sc                       NEW
│   └── fs_ui_overlay.sc                       NEW
```

### Files modified

```
Cluiche/Cluiche.sln
   - Add DiaBgfx.vcxproj reference

Dia/DiaSFML/RenderWindow.h / .cpp
   - Renamed to Window.h / .cpp (concept clarification: this is now an IWindow + IInputSource only)
   - ICanvas interface no longer implemented; StartFrame/ProcessFrame/EndFrame stubs removed
   - mBackBuffer, mUIRenderOverlay, mTextureHandler members deleted
   - GetSystemHandle() unchanged; the entry point bgfx attaches to
   - NOTE: actual deletion of ICanvas-side code happens in `diasfml-render-removal`. In THIS feature, `RenderWindow` retains its existing ICanvas surface; `Bgfx::Canvas` is a separate ICanvas instance constructed alongside. The two coexist via an active-canvas selector at app wire-up time.

Cluiche/Cluiche/Main.cpp (or kernel module)
   - Construct DiaSFML::Window first
   - Construct Bgfx::Canvas with CanvasSettings (renderer type from env var or default Direct3D11)
   - bgfx_canvas->AttachToNativeWindow(window->GetSystemHandle(), window->GetSize())
   - bgfx_canvas->Initialize(canvasSettings)
   - Wire bgfx_canvas->GetUIRenderOverlay() into the UI subsystem (replaces SfmlUIRenderOverlay registration)
   - Active-canvas selection: if BGFX_BACKEND env var set, use Bgfx::Canvas; else fall back to DiaSFML::Window's ICanvas surface (preserved during this feature)

Dia/DiaSFML/TextureHandler.h / .cpp
   - Tick() upload step gains a backend dispatch:
       if (active backend == SFML)  -> SfmlTexture::UploadFromImage(image)
       if (active backend == bgfx)  -> BgfxTextureHandle::UploadFromMemory(image.getPixelsPtr(), w, h)
   - The TextureHandler now produces ITexture* (per `texture-handle-stringcrc`) of either concrete subtype based on the active backend
   - mAssetIdToTexture map's value type stays ITexture*; concrete type chosen at Tick time

pipeline.toml
   - Add [bgfx] section: default_backend = "dx11"
   - Add bgfx_shaders = true to [targets.cluichetest.build_deps]  (already added by previous feature; no-op here)

Dia/DiaGraphics/dia.graphics.architecture.module.md
   - No changes — DiaGraphics public surface unchanged

Dia/DiaUI/dia.ui.architecture.module.md
   - No changes — IUIRenderOverlay already added by `render-overlay-surface`

Cluiche/Tests/GoogleTests/...
   - Add a synthetic multi-swapchain test (creates two Bgfx::Canvas instances with separate HWNDs, renders different colours to each, validates via bgfx readback)
   - DiaBgfx::SpriteRenderer / DebugRenderer / UIOverlayRenderer get unit-test coverage with bgfx in null-renderer mode (bgfx supports `RendererType::Noop` for headless test runs)
```

### Threading model

bgfx is internally multi-threaded: the application calls into bgfx APIs on the "API thread" (main), and bgfx submits commands to its own internal render thread. The contract:

- All `bgfx::*` calls happen on the app's render thread (Cluiche's Render PU)
- `bgfx::frame()` advances the frame and synchronises with bgfx's render thread
- `bgfx::createTexture2D` is API-thread only (per bgfx docs); this matches the existing `sf::Texture::loadFromImage` constraint

The async-loader's existing two-phase pattern (worker decode → main upload via `Tick()`) is preserved exactly. Only the upload step's body changes from `sf::Texture::loadFromImage` to `bgfx::createTexture2D`.

### Multi-swapchain

For CluicheEditor (game viewport + editor chrome), two independent `Bgfx::Canvas` instances exist, each with its own native HWND. bgfx supports this via `bgfx::createFrameBuffer(nwh, w, h)` per swapchain. Each `Canvas` allocates its own three view ids (`mEntityViewId`, `mDebugViewId`, `mUIViewId`) and binds its framebuffer to those views. `bgfx::frame()` is called once per render PU tick, presenting all views to their respective framebuffers.

A synthetic test in this feature validates this end-to-end with two distinct `Canvas` instances rendering different content.

### Acceptance test plan

Three layers of validation:

1. **Unit** — `Bgfx::SpriteRenderer`, `DebugRenderer`, `UIOverlayRenderer` tested individually with `RendererType::Noop` (bgfx headless mode) — no GPU required, runs in CI/Docker
2. **Integration (visual diff)** — `dia run cluichetest` with `BGFX_BACKEND=dx11` set; manual visual diff of DummyStage frame against pre-feature SFML screenshot. Pass if no perceptible regressions
3. **Performance smoke** — frame-time histogram captured over 1000 frames of DummyStage running DiaPhysics2D + DiaRig2DVisualDebugger + DiaIK2DVisualDebugger active; compare median + p99 against SFML baseline. Pass if within ±20%

## Files Introduced / Modified

| File | Change |
|------|--------|
| `Dia/DiaBgfx/` (entire module) | NEW — Canvas, three sub-renderers, two resource types, six shader sources |
| `Dia/DiaBgfx/DiaBgfx.vcxproj{,.filters}` | NEW |
| `Dia/DiaBgfx/dia.bgfx.architecture.module.md` | NEW |
| `Cluiche/Cluiche.sln` | Add DiaBgfx.vcxproj reference |
| `Dia/DiaSFML/RenderWindow.h / .cpp` | Rename to `Window.h / .cpp`; ICanvas surface preserved during this feature, deleted in next feature |
| `Cluiche/Cluiche/Main.cpp` (or kernel module) | App wire-up: construct Bgfx::Canvas, AttachToNativeWindow, runtime backend selection |
| `Dia/DiaSFML/TextureHandler.h / .cpp` | `Tick()` upload step gains backend dispatch |
| `pipeline.toml` | `[bgfx]` section with `default_backend = "dx11"` |
| `Cluiche/Tests/GoogleTests/...` | Add multi-swapchain test, sub-renderer unit tests with bgfx Noop |

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `bgfx-env-setup` (Approved) | Hard | `External/bgfx/lib/<config>/bgfx.lib` and headers must exist |
| `bgfx-shader-cook` (Approved) | Hard | Cooked `.bin` shaders required at runtime; this feature provides the first six `.sc` source files |
| `texture-handle-stringcrc` (Approved) | Hard | `BgfxTextureHandle` implements `ITexture`; `SpriteDrawCommand::texture` is `ITexture*` |
| `render-overlay-surface` (Approved) | Hard | `BgfxUIOverlayRenderer` implements `Dia::UI::IUIRenderOverlay` |
| `diabgfx-imgui-backend` | Reverse | ImGui debug overlays disabled until that feature lands; suppression flag captured in implementation |
| `diasfml-render-removal` | Reverse | Deletes the SFML render path after this feature reaches parity |
| `texturehandler-two-phase-load` (Approved, async-asset-loading) | Soft | Plan re-targeted against `ITexture` per RB-009; this feature's `BgfxTextureHandle` plugs into that updated plan |

## Acceptance Criteria

1. `Dia/DiaBgfx/DiaBgfx.vcxproj` builds clean under `/std:c++20` for `Debug|x64` and `Release|x64`; links against `External/bgfx/lib/<Config>/bgfx.lib`
2. `Dia::Bgfx::Canvas` implements `Dia::Graphics::ICanvas`'s six pure-virtual methods (`Initialize`, `SetCanvasSize`, `SetActiveContext`, `StartFrame`, `ProcessFrame`, `EndFrame`)
3. `Dia::Bgfx::Canvas` does **not** implement `Dia::Window::IWindow` or `Dia::Input::IInputSource` (RB-004 verified by inspection)
4. `Dia::Bgfx::SpriteRenderer` consumes `EntityFrameData` directly (no visitor); batches sprites by `ITexture*` key; produces one bgfx draw call per unique texture
5. `Dia::Bgfx::DebugRenderer` consumes `DebugFrameData` directly (no visitor); covers all 8 `DebugPrimitive` variants; uses `bgfx::TransientVertexBuffer` for per-frame data
6. `Dia::Bgfx::UIOverlayRenderer` implements `Dia::UI::IUIRenderOverlay::Composite()` by uploading the `UIDataBuffer` bytes into a bgfx 2D texture and drawing a fullscreen quad
7. `Dia::Bgfx::BgfxTextureHandle` implements `Dia::Graphics::ITexture` with atomic `State` and `bgfx::TextureHandle` ownership
8. Six `.sc` shader files (`vs_sprite`, `fs_sprite`, `vs_debug`, `fs_debug`, `vs_ui_overlay`, `fs_ui_overlay`) plus `varying.def.sc` exist under `Dia/DiaBgfx/Shaders/`
9. `dia pipeline --target cluichetest --stage compile-code` cooks all six shaders for `dx11`, `dx12`, `vulkan`; cooked `.bin` files appear under `Cluiche/out/cluichetest/shaders/<backend>/`
10. `dia run cluichetest` with `BGFX_BACKEND=dx11` renders DummyStage with all three sprites visible at correct positions, matching pre-feature SFML output (manual visual diff in PR description)
11. All existing visual debuggers (rig2d, rigidbody2d, softbody2d, ik2d, geometry2d, animation2d) render correctly via bgfx (verified by enabling each layer in the visual debugger console and visually confirming primitives appear)
12. Multi-swapchain synthetic test (`TestBgfxMultiCanvas` in GoogleTests) creates two `Bgfx::Canvas` instances with distinct HWNDs (or noop framebuffers in CI), renders different colours to each, and confirms via bgfx readback that the two outputs differ
13. `dia run googletest` passes including the new `TestBgfxSpriteRenderer`, `TestBgfxDebugRenderer`, `TestBgfxUIOverlayRenderer` suites running in `RendererType::Noop` mode (no GPU required)
14. Frame time on DummyStage with full physics + visual debuggers active is within ±20% of the pre-feature SFML baseline (median + p99 over 1000 frames)
15. ImGui is suppressed when running on bgfx (`Dia::ImGui::NewFrame`/`Render` not called); a TODO comment + log line ("ImGui suppressed; bgfx backend pending diabgfx-imgui-backend") is emitted on first frame
16. The SFML render path is **still functional** — `BGFX_BACKEND` unset (or set to anything other than `dx11`/`dx12`/`vulkan`) falls back to `DiaSFML::RenderWindow`'s ICanvas surface; CluicheTest builds and runs identically to pre-feature
17. `bgfx::TextureHandle` does not appear in any DiaGraphics public header (RB-006 verified by `grep`)
18. `dia.bgfx.architecture.module.md` exists, declares `Dia::Bgfx::` namespace, lists all public entry points, validates against `python Tools/dia_modules.py --validate`
19. Multi-swapchain works for the editor pattern (this feature only validates the synthetic test; full CluicheEditor integration is its own future work but the *capability* is verified here)
20. No `std::vector`, `std::string`, etc. in any DiaBgfx public header (PD-004 verified by inspection)

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia/dia.md |
| System (primary) | RenderBackend | @docs/specs/applications/dia/systems/render-backend/render-backend.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Compliant — `BgfxTextureHandle::GetAssetId()` returns `StringCRC`; shader programs and view ids are bgfx ints (internal-only); no raw string identifiers in public surface |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — `Bgfx::Canvas` lives inside the existing Render ProcessingUnit; lifecycle calls map to existing Phases (Init/Update/Shutdown) |
| PD-003 | Platform | Component-based entities | Compliant — sprite emitters are existing components; this feature consumes their FrameData output, doesn't change the component model |
| PD-004 | Platform | No STL containers in public APIs | Compliant — `Canvas`, `SpriteRenderer`, `DebugRenderer`, `UIOverlayRenderer`, `BgfxTextureHandle`, `ShaderProgram` public surfaces use `StringCRC`, `Vector2D`, raw enums, `ITexture*`, `IUIRenderOverlay*`. Internal `.cpp` may use STL freely |
| PD-005 | Platform | x64 only | Compliant — bgfx libs are x64; backends scoped to D3D11/D3D12/Vulkan on Windows x64 |
| PD-006 | Platform | Visual Studio project files are source of truth | Compliant — `DiaBgfx.vcxproj{,.filters}` manually maintained; consumes prebuilt bgfx libs via `.vcxproj` references; no top-level CMake |
| PD-007 | Platform | C++20 required | Compliant — uses `enum class`, `std::atomic`, `= default`, `= delete`; bgfx headers verified to compile under `/std:c++20` per `bgfx-env-setup` |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir/PlatformToolset | Compliant — DiaBgfx.vcxproj inherits all settings; no overrides |
| PD-009 | Platform | Generated output under `Cluiche/out/<AppName>/` | Compliant — cooked shaders read from `Cluiche/out/<App>/shaders/<backend>/` (per `bgfx-shader-cook`) |
| PD-010 | Platform | `.diagame` typed imports | N/A — no manifest changes |
| AD-001 | Dia App | Module YAML frontmatter | Compliant — `dia.bgfx.architecture.module.md` created |
| AD-002 | Dia App | No STL in public APIs | Reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | Compliant — `Dia::Bgfx::` for all DiaBgfx code |
| AD-004 | Dia App | ProcessingUnit/Phase/Module | Reinforces PD-002 |
| AD-005 | Dia App | Component-based entities | Reinforces PD-003 |
| GD-001 | DiaGraphics | DebugPrimitive is hand-rolled tagged union | Compliant — `DebugRenderer` consumes the existing tagged union; no schema changes |
| GD-002 | DiaGraphics | FrameData copy must be trivially correct — no pointer members in debug buffers | Compliant — DiaBgfx does not modify `DebugFrameData`; `DebugPrimitive` storage layout unchanged |
| GD-003 | DiaGraphics | Debug renderer is a separate concern | Compliant — `Dia::Bgfx::DebugRenderer` is the bgfx-side debug renderer; preserves the separation |
| GD-004 | DiaGraphics | Debug primitives stored in insertion order; rendered in push order | Compliant — `DebugRenderer` iterates the tagged-union buffer in the order the visitor would have, dispatching by `DebugPrimitiveType` |
| RB-001 | RenderBackend | Adopt bgfx | Compliant — this is the bgfx adoption |
| RB-002 | RenderBackend | Two-phase delivery | Compliant — Phase 1 parity |
| RB-004 | RenderBackend | Canvas implements ICanvas only | **Compliant — explicitly verified by acceptance criterion 3** |
| RB-005 | RenderBackend | No visitor pattern in production render path | **Compliant — sub-renderers consume FrameData directly** |
| RB-006 | RenderBackend | No backend types in DiaGraphics public surface | **Compliant — verified by acceptance criterion 17 (`bgfx::TextureHandle` audit)** |
| RB-007 | RenderBackend | ITexture/IShader keyed by StringCRC | Compliant — `BgfxTextureHandle::GetAssetId()` returns `StringCRC` |
| RB-009 | RenderBackend | Async loading sequencing — ITexture refactor first, then DiaBgfx::TextureHandle implements it | **Compliant — this feature implements the second half of RB-009** |
| RB-010 | RenderBackend | bgfx prebuilt; consumed via .vcxproj | Compliant — `DiaBgfx.vcxproj` references prebuilt libs from `External/bgfx/lib/` |
| RB-011 | RenderBackend | shaderc cook step in DiaPipeline | Compliant — this feature consumes cooked binaries produced by `bgfx-shader-cook` |
| RB-012 | RenderBackend | Default Windows backend deferred | **Resolved here — D3D11 chosen as default for Phase 1 stability and tooling support** (PIX/RenderDoc) |
| RB-016 | RenderBackend | Phase 1 ship gate | Partial — this feature gets bgfx to parity; the ship gate fires at `diasfml-render-removal` |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Default backend | Why D3D11 over D3D12 as the default for Phase 1? | D3D11 is the most mature bgfx backend on Windows; PIX and RenderDoc both work cleanly without configuration; descriptor-heap pressure (a D3D12 issue with many UI sprites) is avoided. Switching to D3D12 later is a one-line `bgfx::Init` change. RB-012 explicitly defers this decision; this feature takes the call. |
| 2 | Active-canvas selector | An env-var-driven runtime selector between SFML and bgfx is brittle. Why not a build flag? | Build flags require recompiling to switch, which slows parity-testing iteration. The env var is a Phase 1 expedient; `diasfml-render-removal` deletes the SFML option entirely and the env var becomes meaningless (bgfx is the only path). The "expedient" period is intentionally short. |
| 3 | bgfx render thread | Should we explicitly enable or disable bgfx's internal render thread? | No — leave at bgfx's compile-time default (multi-threaded on Windows). Pinning the value adds risk without benefit. If perf testing reveals the render thread is causing trouble, a follow-up feature can disable it via `BGFX_CONFIG_MULTITHREADED=0` in `External/bgfx/lib/<config>/` rebuild. |
| 4 | Sprite batching | Do we batch by texture *id* or by texture *pointer*? | Texture pointer (`ITexture*`) — pointer equality is sufficient because each `BgfxTextureHandle` is a unique allocation per asset. Asset id collision at the pointer level is impossible. Saves a `StringCRC` comparison per sprite. |
| 5 | Debug font | bgfx's `dbgTextPrintf` font is monospaced and at integer screen coordinates. SFML's font is freely positionable. Is that a parity regression? | Mild regression — Text2D primitives in DebugFrameData carry float position. We map to integer screen coords by ortho projection + truncation. Visually different from SFML's pixel-perfect font but functionally adequate (it's debug text). A future feature could swap in a proper SDF font renderer. Not load-bearing for Phase 1. |
| 6 | TransientVertexBuffer scaling | bgfx's transient buffer pool has a fixed size. What if DebugFrameData has 1024 primitives × 8 vertices each = 8192 vertices in one frame? | bgfx default transient buffer is 6 MB / view, more than enough for 8192 vertices × ~32 bytes = 256 KB. Verified against bgfx's docs. Pool is shared across all transient allocations per `bgfx::frame()`. |
| 7 | Multi-swapchain test | The test requires two HWNDs. In CI/Docker (no display), how do we run it? | Use `RendererType::Noop` (bgfx's null renderer) and create two noop framebuffers via `bgfx::createFrameBuffer(BGFX_INVALID_HANDLE, w, h, ...)`. Validates the bgfx-side code path; visual readback is via `bgfx::readTexture`. Real GPU multi-swapchain is verified manually on a dev machine in the acceptance check. |
| 8 | TextureHandler dispatch | Touching DiaSFML's `TextureHandler::Tick()` to dispatch by backend is awkward — it puts bgfx logic inside DiaSFML. Why not a separate `BgfxTextureHandler`? | Phase 1 expedient. The cleanest answer is to move TextureHandler into a renderer-agnostic location (probably `DiaAssetRuntime`), but that's its own refactor. For Phase 1, the dispatch is a small backend-switch in `Tick()`. `diasfml-render-removal` extracts `TextureHandler` out of DiaSFML as part of the broader cleanup. |
| 9 | Coexistence overhead | While both SFML and bgfx render paths are alive, every change to FrameData requires updating two consumers. Acceptable? | Yes — for the duration of *one feature* (this one) and the next feature (`diasfml-render-removal`). Shouldn't extend further. The dispatch board / plan should sequence these two features back-to-back to minimise the coexistence window. |
| 10 | Shader root path | `cookedShaderRoot` is hardcoded to `Cluiche/out/<App>/shaders` per app. How does Canvas know `<App>`? | Caller supplies it in `CanvasSettings.cookedShaderRoot`. Resolution from app name to shader root is a pipeline/config concern, not a Canvas concern. CluicheTest's `Main.cpp` sets it to `Cluiche/out/cluichetest/shaders`; CluicheEditor's would set its own. |
| 11 | UI overlay shader equivalence | Today's `ui.frag` mixes UI overlay over backbuffer with a specific blend mode. Can the bgfx shader replicate it exactly? | Yes — read both textures, output `mix(backBuffer, uiOverlay, uiOverlay.a)` (or whatever today's shader does). The shader is short; pixel-equivalence is achievable. If perceptible differences arise, capture in PR review and tweak. |
| 12 | varying.def.sc shared or per-shader | One `varying.def.sc` per shader directory is bgfx convention. Should we use one shared file or per-shader files? | Start with one shared file under `Dia/DiaBgfx/Shaders/varying.def.sc` that declares all varyings used by sprite/debug/ui shaders. This is the most common bgfx pattern (see bgfx's own examples). If shader specialisation grows beyond this, split per-shader-folder. |
| 13 | Performance budget | ±20% frame time tolerance is generous. Is that the right bar? | For Phase 1, yes — bgfx vs SFML on the same workload should be roughly comparable; large divergence (>20%) is a red flag. Tighter perf work (instancing, persistent buffers, draw-call reduction) is a follow-up after parity is proven. Don't optimise prematurely. |
| 14 | DummyStage as the parity workload | DummyStage is small (3 sprites + minimal debug). Is it representative enough for parity? | Acceptable for Phase 1's parity bar. Larger real workloads (full CoW dragon scene, future games) will surface different concerns, but those are post-Phase-1. The visual debugger sweep (criterion 11) covers a much broader debug workload than DummyStage alone. |
| 15 | `RenderWindow` rename to `Window` | Is this rename in scope here, or part of `diasfml-render-removal`? | This feature renames it (concept clarification: it's now Window+Input only) but keeps the ICanvas surface intact. The next feature (`diasfml-render-removal`) deletes the ICanvas surface from `Window`. Splitting the rename + delete avoids one giant diff. |
| 16 | bgfx error handling | bgfx uses a callback interface (`bgfx::CallbackI`) for fatal errors. Do we implement it? | Yes — a minimal `Dia::Bgfx::CallbackHandler` that routes bgfx fatals through `DIA_LOG_ERROR` and triggers `DIA_ASSERT(false)` in DEBUG. Captured as part of `Canvas::Initialize`. |

---
