# Feature Spec: DiaBgfx3D Render Targets and Dynamic Textures (Feature A)

## Parent System
@docs/specs/applications/dia/systems/diabgfx3d/diabgfx3d.md (Amendment 2026-11-20)

**Gameplay Domains:** rendering
**Needed by:** Post-Process Stack (D), DraoiHexWorldRender3D (channel textures), DiaToonShading golden harness and CoW `cow.map.capture` (readback), Heightfield Mesh (E)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/alive.js` (`PostFX.ensure`), `lookdev/js/glutil.js` (`glTexture`), per-frame `texSubImage2D` of the weather and event textures in `app.js`

## Status
`Approved` (2026-11-24, user, after spec review: blocking test readback, headless AC). Drafted 2026-11-20 from the approved prototype. **Needs AD-001 sign-off before build** (DiaBgfx and DiaBgfx3D API additions).

## Summary

Two GPU building blocks DiaBgfx3D does not have today:

1. **Render targets.** An offscreen colour + sampleable depth target. `Canvas3D` can redirect its scene views into one, and a later pass reads both. Today the only render-to-texture is the shadow map, internal to `ShadowRenderer`.
2. **Dynamic textures.** A texture created once and refreshed from CPU memory (`Update`). Today `BgfxTextureHandle` can only be created from a decoded image, and only `CellFieldRenderer` updates textures at runtime (private R32F grids).

Both are generic: any 3D game needs offscreen targets and per-frame data textures.

## Problem

- The post-process stack needs the scene's colour and depth as textures. There is no way to render the scene anywhere but the backbuffer.
- DraoiHexWorldRender3D uploads per-cell channel textures (condition, weather, events) every frame or step. `BgfxTextureHandle` has no `Update`, and its formats are RGBA8 only.
- The headless capture path (`frame-capture-readback.md`) reads the backbuffer. A render target must be readable the same way so captures work with the post pass on.

## Goals

1. `RenderTarget`: colour (RGBA8, optionally RGBA16F) + depth (sampleable), created at a size, resizable, usable as a sampler input.
2. `Canvas3D::SetSceneTarget(RenderTarget*)` redirects the background, shadow-independent scene views (mesh, debug 3D, and the CellField view when asked) into the target. `nullptr` restores the backbuffer. Default is `nullptr`: no change to existing games.
3. `BgfxTextureHandle::CreateDynamic(w, h, format)` and `Update(x, y, w, h, data, bytes)` for RGBA8, RGBA16F and R32F, with linear or nearest filtering and clamp or wrap.
4. **Readback** of a render target to CPU memory and a PNG, through the existing frame-capture path.
5. Capability checks (RGBA16F, depth sampling) with a logged fallback or a clear failure, never a crash.

## Non-Goals

- Multiple render targets or MRT, MSAA resolve control, or HDR output.
- Texture arrays and mips (mips are the sprite atlas's concern, Feature B).
- A render graph. The post stack (D) orders its own passes.
- Changing view ids or the order of existing passes when no scene target is set.

## Design

```cpp
namespace Dia::Bgfx {
    enum class TextureFormat : uint8_t { RGBA8, RGBA16F, R32F };
    struct RenderTargetDesc { uint16_t width, height; bool hdrColour = false; bool sampleDepth = true; };

    class RenderTarget {
    public:
        bool Create(const RenderTargetDesc&);  void Destroy();
        bool Resize(uint16_t w, uint16_t h);           // recreates; handles stay valid for the caller via accessors
        uint16_t Width() const;  uint16_t Height() const;
        uint16_t ColourTextureIdx() const;  uint16_t DepthTextureIdx() const;   // renderer-internal, like BgfxTextureHandle
        uint16_t FramebufferIdx() const;
        // Asynchronous: bgfx::readTexture completes a couple of frames later. Returns a ticket (the frame number
        // the data is ready on), or 0 on failure; the caller polls IsReadReady(ticket) once per frame.
        uint32_t ReadColour(uint8_t* rgba, size_t bytes);
        bool     IsReadReady(uint32_t ticket) const;
        // For tests and the DiaToonShading golden harness: submits the read and pumps frames until it is ready
        // (or a frame limit passes, returning false). Never for use in a game loop.
        bool     ReadColourBlocking(uint8_t* rgba, size_t bytes);
    };
}
namespace Dia::Bgfx3D {
    void Canvas3D::SetSceneTarget(Dia::Bgfx::RenderTarget* target);   // null = backbuffer
    Dia::Bgfx::RenderTarget* Canvas3D::GetSceneTarget() const;
}
// BgfxTextureHandle additions
bool CreateDynamic(unsigned w, unsigned h, TextureFormat fmt, uint64_t bgfxFlags = 0);
bool Update(unsigned x, unsigned y, unsigned w, unsigned h, const void* data, size_t bytes);
```

- **View routing.** `setViewFrameBuffer(view, fb)` for views 0 (clear), 2 (mesh), 3 (debug 3D) and, when a target is set, the CellField view 8. Views 1 (shadow) already has its own framebuffer. Views 4–7 (entity, debug 2D, UI, ImGui) are never redirected: UI draws on the backbuffer after the post pass.
- **Size.** The target tracks the backbuffer size; `Canvas3D` resizes it on window resize.
- **Depth sampling.** Created as a depth-format texture flagged readable. bgfx's D3D11 path exposes it as a shader resource view. If the device lacks this (`BGFX_CAPS_TEXTURE_COMPARE`-style checks), `RenderTarget::Create` returns false with the reason logged.
- **Update** uses `bgfx::updateTexture2D` with `bgfx::copy`, on the render thread. Callers hand over data on the render side; no sim memory crosses.

## Acceptance Criteria

1. **Default unchanged.** With no scene target set, existing mesh, shadow, debug and CellField rendering is pixel-identical to before (existing golden-image tests pass).
2. **Create and size.** `Create` returns true for valid sizes (1..4096), false for 0 or oversize; `Resize` changes `Width`/`Height`; `Destroy` is idempotent.
3. **Redirect.** With a scene target set, a test mesh renders into the target (a pixel read-back from the target is the mesh colour) and the backbuffer receives nothing from the scene views until a pass draws the target.
4. **UI untouched.** Views 4–7 draw to the backbuffer while a target is set.
5. **Depth readable.** A depth value written by the scene is sampleable in a following full-screen draw (test shader outputs depth as colour; the read pixel is within 1e−3 of the expected depth).
6. **Dynamic update.** `CreateDynamic` + `Update` of a sub-rectangle changes exactly that rectangle (read back through a sampler draw). RGBA8, RGBA16F and R32F each round-trip a known pattern (within format precision).
7. **Bounds.** `Update` with a rectangle outside the texture, null data or too few bytes returns false and logs a warning; nothing is written.
8. **Capture.** The headless capture path (`frame-capture-readback`) can capture the final backbuffer with the target in use and read the scene target directly.
9. **Capability failure.** On a device without RGBA16F or depth sampling, `Create` / `CreateDynamic` return false with a specific log message, and `Canvas3D` keeps rendering to the backbuffer.
10. **Lifetime.** Resizing the window with a target set recreates it with no leak (bgfx resource counters are equal before and after 100 resizes).
11. **Observability.** Log channel `BGFX` for create/resize/failure; metrics `bgfx.rt_bytes` and `bgfx.dyn_tex_updates_per_frame`.
12. **Headless test configuration.** In the configuration the tests use (hidden window or the existing headless capture setup), a `RenderTarget` can be created, drawn into and read back with `ReadColourBlocking`, so DiaToonShading's golden harness can run. If the GoogleTests process cannot create a device, the spike (task 1) records that and the harness runs as a CluicheTest stage instead (DiaToonShading Feature 1 ODQ 1).
13. **Async readback.** `ReadColour` returns a ticket; `IsReadReady` is false before it and true on or after it; the buffer holds the pixels once ready; a failed read returns 0 and logs.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| RB-001 | bgfx is the GPU abstraction | All GPU work uses bgfx in DiaBgfx/DiaBgfx3D |
| RB-006 | No backend types in DiaGraphics' public surface | `RenderTarget` and handles live in DiaBgfx; consumers see indices/opaque classes only |
| AD-001 | Engine API changes need sign-off | **Sign-off required** before build |
| PD-004 | No STL in public APIs | Raw pointers and sizes |
| PD-005 | x64 / D3D11 default | Verified on D3D11; Vulkan/D3D12 checked, not required |

## Open Design Questions

1. **Depth sampling on D3D11 through bgfx.** The prototype ran on WebGL, which samples depth textures directly. bgfx D3D11 normally needs the depth texture created with the right format flags. Verify in the first task; the fallback is a second colour attachment that writes linear depth (costs one extra output in every scene shader).
2. **MSAA.** The backbuffer may be multisampled. A scene target with MSAA needs a resolve before it can be sampled. v1 renders the target without MSAA and relies on the silhouette ink and shader smoothing. Revisit if aliasing shows.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Spike: depth-as-texture on bgfx D3D11, RGBA16F caps, and can the test process create a device and read a target back | Open Q1 and AC 12 answered in plan notes | Not Started | sonnet | gates the rest; also unblocks DiaToonShading Feature 1 task 4 |
| 2 | `RenderTarget` create/resize/destroy | AC 2, 10 | Not Started | sonnet | |
| 3 | `Canvas3D::SetSceneTarget` + view routing | AC 1, 3, 4 | Not Started | sonnet | |
| 4 | Depth sampling test pass | AC 5 | Not Started | sonnet | |
| 5 | `BgfxTextureHandle::CreateDynamic` / `Update` | AC 6, 7 | Not Started | sonnet | |
| 6 | Readback (async ticket and blocking test variant) and capture integration | AC 8, 12, 13 | Not Started | sonnet | |
| 7 | Capability failure paths, logs, metrics | AC 9, 11 | Not Started | haiku | |
| 8 | Module docs (`dia.bgfx.architecture.module.md`, `dia.bgfx3d...`) | `dia check deps` | Not Started | haiku | |
