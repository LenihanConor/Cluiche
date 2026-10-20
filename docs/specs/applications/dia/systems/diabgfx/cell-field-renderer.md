# Feature Spec: CellField Renderer

## Parent System
@docs/specs/applications/dia/systems/render-backend/render-backend.md

**Gameplay Domains:** rendering

**Needs log:** CoW `dia-needs.md` #14. First consumer: S-1 World Generation Feature 7 (@docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md).

## Status
`Approved` (2026-10-15). No plan yet (write one when building). Build order: Step 1 (1a, 1b, 1c) before S-1 Feature 7; Step 2 after S-1's view works. Open questions are build-time details; start with `dia check deps` for where the hex adapter lives.

## Summary

CellField draws a value per grid cell as a coloured map, using GPU-retained geometry. A caller creates a **layer** for a grid (a hex map today), uploads one value per cell (a category id or a float), and each frame asks for the layer to be drawn with a palette or ramp, an inset for gap lines, an optional hillshade, and so on. The geometry is generated at runtime once per grid size and cached; the values live in a small texture and are re-uploaded only when they change. Switching the colour mapping uploads nothing.

It replaces drawing a map as thousands of immediate-mode debug primitives (a 66 × 42 hex map would be ~11,000 triangles plus ~8,400 grid lines per frame) with one draw call per layer.

The work is split into two steps so the first can ship on its own:

- **Step 1: Core** (blocks S-1 Feature 7): geometry and cache, value texture, palette and ramp colour mapping, inset, Relief hillshade, the `IDebugDraw` / `FrameData` hook, the cross-thread hand-off.
- **Step 2: Extras** (built after S-1's view works): smooth shading as a per-layer option, layer stacking with opacity, blending between two value states, partial cell updates.

The Step 2 API fields exist from Step 1 (defaulted and ignored), so callers written against Step 1 don't change.


**Logging** (amended 2026-10-19, consistency pass): channel `CFL`. Warning when a layer is rejected for exceeding a limit (128×128, 32 layers), and when the upload queue coalesces or overflows its 8 MB cap. Error when geometry generation fails.
## Problem

- `IDebugDraw` has no filled-polygon primitive, and the debug primitive buffer is a fixed per-frame array that is copied several times on its way to the renderer (see Findings). Drawing a 2,772-cell map as primitives needs ~19,500 of them every frame, rebuilt on the CPU even though the map changes rarely.
- Several upcoming views are "a colour per cell, redrawn every frame, updated rarely": S-1's terrain and biome view, other simulation overlays, fog of war. Each would pay the same cost and re-implement the same colour-mapping code.
- Live editing (sliders re-shaping a map every frame) needs value updates measured in kilobytes, not rebuilt geometry.
- Relief (light shading from an elevation field) must update live as inputs change, with no CPU work per change.

## Goals

1. **GPU-retained geometry** generated at runtime per (topology, width, height), uploaded once, cached and shared between layers of the same grid. Not a baked asset. *(Step 1)*
2. **Values in a small texture**, one texel per cell, holding a category id or a float. Re-uploaded only when the caller changes values. *(Step 1)*
3. **Colour mapping chosen per draw**: category ids through a palette, floats through a ramp (min, max). Changing palette, ramp or range uploads nothing. *(Step 1)*
4. **Inset** (shrink each cell toward its centre) with a backdrop colour, giving gap grid lines at no extra cost. *(Step 1)*
5. **Relief hillshade** computed in the shader from an elevation layer and a light direction: no CPU work per slider move. *(Step 1)*
6. **Hook for `IDebugDomain` drawers** (they only see `IDebugDraw`) and a thread-safe hand-off from the simulation thread to the render thread, with no cross-thread raw pointers. *(Step 1)*
7. **Shares the debug overlays' camera** and draws below debug overlays, so overlays and the field never drift apart under pan and zoom. *(Step 1)*
8. **Smooth shading** as a per-layer option: float values are interpolated across hex corners instead of one flat colour per hex. *(Step 2)*
9. **Layer stacking with opacity**: several layers per frame, drawn in request order with alpha blending. *(Step 2)*
10. **Blend between two value states**: each layer keeps its previous and current value state, and a draw takes a 0..1 factor between them. *(Step 2)*
11. **Partial cell updates**: change a list of cells without re-uploading the layer. *(Step 2)*
12. **Does not consume the debug-draw primitive budget** (`DebugFrameData::kGeometryCapacity`, 8,192 since dia-needs #13).

## Non-Goals

1. **Square-grid geometry.** Hex flat-top odd-q only in v1. The geometry builder sits behind an interface (below) so a square-grid builder is added later with no API change.
2. **Picking.** Screen-to-cell is `HexTopology::CellFromPoint`; this feature draws only.
3. **Directional glyph overlays** (arrows or similar per cell): a separate feature, deferred.
4. **Tile art, textures or sprites per cell.** CellField is flat colour with shading. Textured tiles are a different renderer.
5. **Simulation concepts.** CellField knows cells, values and colours. What the values mean, how often they change, and how they are named belong to the caller.
6. **Colour-space tricks** (gradients between hex centres in non-linear spaces, dithering). Linear RGB ramps only.
7. **Water, depth or terrain awareness.** If a caller wants water shaded by depth, it supplies a ramp or a second layer.
8. **Moving or animated geometry.** Geometry is static for a given grid.
9. **Grids larger than 128 × 128** or more than 32 live layers in v1 (rejected with an error result, not asserted).

## Findings: what was verified in the code (2026-10-15)

| Area | Finding | Source |
|------|---------|--------|
| Render views | `Canvas` fixes view ids: entity 4, debug 5, UI 6, ImGui 7; `Canvas3D` owns 0–3. Entity view clears to 0x303030FF; the others don't clear. | `Dia/DiaBgfx/Canvas.h:62-66`, `Canvas.cpp:183-187` |
| Debug camera | `DebugRenderer::Draw` builds the ortho matrix from `Camera2D` position and zoom and the canvas size, sets it with `bgfx::setViewTransform(mViewId, nullptr, ortho)`, and sets the view rect. It does this each frame, only when there are debug primitives or text. | `DebugRenderer.cpp` (`Draw`) |
| Debug draw submission | Per-frame `std::vector` batches (lines, tris, points) copied into `bgfx::TransientVertexBuffer`s; blend state `BGFX_STATE_BLEND_ALPHA`; submit in a fixed order: lines, tris, points. Immediate mode only. | `DebugRenderer.cpp` (`FlushLines`, `FlushTris`, `FlushPoints`) |
| Shader loading | `ShaderProgram::Load(root, backend, vsName, fsName)` loads `<root>/<backend>/vs_<name>.bin` and `fs_<name>.bin`; `Canvas::DeferredInit` loads sprite, debug and UI programs after `bgfx::init` and warns if any is missing. | `ShaderProgram.cpp:84-85`, `Canvas.cpp:189-220` |
| Shader cooking | Any `vs_*.sc` / `fs_*.sc` under a `pipeline.toml` `[bgfx_shaders] source_roots` directory is cooked per backend (dx11, dx12, vulkan) by `shaderc`; `varying.def.sc` is shared per directory. New shaders only need files in `Dia/DiaBgfx/Shaders/` and a `.vcxproj` `None` entry. | `pipeline.toml:9-13`, `bgfx_shader_cook.py` |
| Retained buffers | `MeshGpuCache` already creates static vertex and index buffers with `bgfx::createVertexBuffer(bgfx::copy(...))` and destroys them in `Evict`/`DestroyAll`, render thread only, with no bgfx types in the public header. | `Dia/DiaBgfx3D/Resources/MeshGpuCache.*` |
| Textures | `BgfxTextureHandle` wraps a bgfx texture handle and uploads from memory on the render thread. No existing code updates a texture in place. | `Dia/DiaBgfx/Resources/BgfxTextureHandle.h` |
| bgfx API | `bgfx.h` declares `updateTexture2D`, `createTexture2D`, `setViewOrder`, `ViewMode::Sequential`, texture format `R32F`, and the caps flag `BGFX_CAPS_FORMAT_TEXTURE_VERTEX`. `BgfxImGuiRenderer` already uses `setViewMode(..., Sequential)`. | `External/bgfx/include/bgfx/bgfx.h` |
| Which thread draws what | Debug drawers (`IVisualDebugger::Draw`) run on **SimPU** inside `VisualDebuggerModule::DoUpdate`, which writes a `FrameData` to a stream; `UICompositeModule` (SimPU) is the sole writer of `SimToRender` and merges scene, debug and UI frames into one `FrameData`; `RenderModule` (RenderPU) fetches it, copies it into `mLastFrame`, then calls `mCanvas->RenderFrame`. | `VisualDebuggerModule.cpp:92`, `UICompositeModule.h`, `RenderModule.cpp:90-102` |
| FrameData copies | `FrameData::Copy` is hand-written per member (debug buffer, UI buffer, sprites, camera, sizes). A new per-frame array must be added to `Copy` and `Clear` or it is silently dropped. A frame is copied several times between the drawer and the GPU. | `FrameData.cpp` |
| Service pattern | The canvas is created by `KernelModule` (MainPU); `TextureHandler` is shared to other PUs as a `ServiceStream` named `KernelTextureHandler` (`ServiceStreamWriter` in the kernel, `ServiceStreamReader` in consumers). | `KernelModule.h:70`, `RenderModule.h:38`, `AssetServiceModule.h:123` |
| Frame capture | `ICanvas::RequestFrameCapture` / `PollFrameCapture` are implemented (`FrameCaptureRingBuffer`) and `diarendertest` has a spec and plan. | `Canvas.h`, `docs/.../diarendertest/` |
| Backend decisions | `render-backend.md` is a stub; the RB decisions are only visible through the feature specs that cite them (RB-001 bgfx is the abstraction, RB-004 canvas implements `ICanvas` only, RB-005 no visitor pattern, RB-006 no backend types in DiaGraphics' public surface). | `canvas-parity.md`, `3d-renderers.md` |

**Not verified (assumed; check at plan time):**

- Whether `R32F` textures sample correctly on the target GPUs in the fragment shader (assumed yes on D3D11 feature level 11; the vertex-fetch caps flag is *not* relied on, see Open Question 1).
- That `bgfx::setViewOrder` for a new view id (8) doesn't conflict with how `Canvas3D` orders views 0–3.
- The module layering: whether DiaGraphics or DiaBgfx may depend on DiaPathfinding (`HexTopology`). The `dia.pathfinding` module doc wasn't found by the glob used; run `dia check deps` at plan time (see Open Question 2).
- That D3D12 and Vulkan cook the new shaders (the cook step is generic, but only dx11 was exercised by existing features).
- The cost of copying a `CellFieldDrawRequest` array through the stream chain: small by construction, but not measured.

## Architecture

### Layering and ownership

| Piece | Module | Responsibility |
|-------|--------|----------------|
| `CellFieldDrawRequest` (POD) and `IDebugDraw::RequestDrawCellField` | DiaCore (`DebugDraw/`) | The only thing drawers see. No handles to GPU objects, no backend types (RB-006). Default implementation is a no-op, so other `IDebugDraw` implementers are unaffected. |
| `CellFieldFrameData`, stored in `FrameData` | DiaGraphics (`Frame/`) | Per-frame list of draw requests; included in `FrameData::Copy` and `Clear`. |
| `CellFieldRegistry` | DiaGraphics (`CellField/`) | Thread-safe layer table, palette and ramp tables, and the command queue between the simulation thread and the render thread. Backend-agnostic. |
| `ICellGridGeometryBuilder`, `CellGridDesc` | DiaGraphics (`CellField/`) | Turns a grid description into CPU vertex arrays. Pure, testable without bgfx. |
| `CellFieldRenderer`, geometry cache, value textures, shader | DiaBgfx (`Renderers/`, `Resources/`, `Shaders/`) | Drains the registry on the render thread, creates and updates GPU objects, draws. All bgfx types stay here. |
| Registry sharing between PUs | CluicheGameBaseline (one small piece of glue) | `KernelModule` owns the registry and publishes it as a `ServiceStream` (`KernelCellFieldRegistry`), following `KernelTextureHandler`, with `Deregister()` before shutdown. A baseline edit, not a Dia edit: flag for sign-off under CoW AD-001. |

The registry is the shared object; `FrameData` carries only small draw requests. **Value arrays never travel inside `FrameData`**, because `FrameData` is copied several times per frame (Findings), and a 128 × 128 layer is 64 KB.

### Geometry (Step 1)

- **Per hex:** 6 triangles in a fan (centre, corner `k`, corner `k+1`), `k = 0..5`. Triangles don't share vertices (18 vertices per hex), so every vertex of a triangle can carry identical per-triangle cell references (below).
- **Vertex format** (36 bytes): centre position (2 floats), corner offset from the centre (2 floats, a unit hex at angles 0°, 60°, … 300°), four cell indices (4 floats, `-1` = none), four weights (4 × unorm8). The shader places the vertex at `centre + offset × cellSize × (1 − inset)`.
- **Cell references per triangle:** the triangle `(centre, corner k, corner k+1)` involves the hex's own cell `c` and three neighbours `N(k−1)`, `N(k)`, `N(k+1)` (the cells sharing its two corners). Over the triangle's four references `[c, N(k−1), N(k), N(k+1)]`, the vertex weights are: centre vertex `(1, 0, 0, 0)`; corner `k` `(⅓, ⅓, ⅓, 0)`; corner `k+1` `(⅓, 0, ⅓, ⅓)`. Missing neighbours (grid edge) take weight 0 and the remaining weights are renormalised. Interpolated across the triangle these give each pixel a blend of up to four cell values that is continuous across hex edges.
- **Flat versus smooth** is a shader choice: flat uses only the own-cell reference (`c`), smooth uses the weights. The geometry is the same, so switching costs nothing.
- **Size:** `18 × 36 = 648` bytes per hex: 1.8 MB at 66 × 42, 10.6 MB at 128 × 128 (the supported maximum).
- **Cache:** keyed by (topology id, width, height); reference-counted by layers; built on the render thread when the first layer for that key is first drawn; released when the last layer is destroyed and at shutdown. Geometry for another size is built separately.
- **Builder interface:** `ICellGridGeometryBuilder::Build(const CellGridDesc&, CellGridGeometry& out)`. `CellGridDesc` is plain data: topology id (`StringCRC`), width, height, a cell size, and per-cell centre positions and six neighbour indices (copied by the caller from the topology, so no module needs a topology type). A `HexFlatTopOddQ` builder is the only v1 implementation; a square builder later is another implementation of the same interface. Centres must equal `HexTopology::CellCenter` (acceptance test 1).

### Value storage and colour mapping (Step 1)

- One `R32F` texture per layer, `width × height`, point-sampled, clamped; texel `(x, y)` is cell `y × width + x` (matches the flat arrays callers already have). Category ids are stored as floats (exact up to 2^24).
- **Palette:** up to 64 `RGBA8` entries, registered once by `StringCRC` id through the registry and uploaded as a 64 × 1 texture. A category id outside the palette draws the palette's fallback colour and increments a counter.
- **Ramp:** a list of colour stops (up to 16) registered once by id and baked into a 256 × 1 texture. A draw gives `rampMin` and `rampMax`; values are normalised, clamped, and looked up.
- A draw names the palette or ramp by id, plus its range, so **changing the mapping or range uploads nothing**.

### Inset and backdrop (Step 1)

`inset` in `[0, 0.5)` shrinks each hex toward its centre. When `inset > 0` and the backdrop alpha is non-zero, a single rectangle covering the grid bounds is drawn first in `backdropColour`, so the gaps read as grid lines. Inset 0 gives seamless tiles.

### Relief hillshade (Step 1)

A draw may name a second layer as the **elevation layer** (any float layer of the same grid) with a light direction and strength.

- The fragment shader reads the elevation through the same four-cell blend as smooth shading, so the elevation surface is continuous even when the colour layer is flat. (This is why the interpolation machinery is Step 1 even though the per-layer *Smooth* option for colour is Step 2.)
- The shading normal comes from screen-space derivatives of the interpolated height and the world position (`dFdx` / `dFdy`), scaled by the view's world-units-per-pixel so it doesn't change with zoom. Result: `colour × (ambient + strength × max(0, dot(n, light)))`.
- Changing the light direction, strength or elevation scale is a uniform change per draw: no upload. Changing elevation values is one texture update.
- Known limitation: derivative normals are constant per 2 × 2 pixel quad, so shading can look faceted at high zoom. Open Question 3 covers the per-vertex-normal upgrade.

### Draw request and hand-off (Step 1)

```
simulation thread                                   render thread
─────────────────                                   ─────────────
registry.CreateLayer(desc) ─ handle ─►
registry.Update(handle, values)  ─┐
                                  ├─ command queue ─► Canvas::ProcessFrame: renderer.DrainRegistry()
debug drawer:                     │                    (create/update/destroy GPU objects)
  draw.RequestDrawCellField(req) ─┴─ FrameData ──────► renderer.Draw(frame.GetCellFieldRequests())
```

- **Commands** (`Create`, `Update`, `Destroy`, plus Step 2's `UpdateCells`) are copied into registry-owned blocks at submit time (copy-on-submit). Nothing the caller owns is read after the call returns. No raw pointers cross threads.
- **Coalescing:** a newer full `Update` for a layer replaces any older pending one. The queue has a byte cap (8 MB); past it, new commands are dropped, a counter is incremented and a warning is logged once. Submitting never blocks the simulation thread beyond a short mutex hold.
- **Handles** are `{index, generation}`. A draw request or command with a stale handle is skipped and counted (`staleHandleCount`), never asserted, so a destroy racing a draw in flight is safe. Destroy is deferred to the render thread, which frees the bgfx objects after draining.
- **Ordering inside a frame:** the render thread drains the whole queue before drawing, so a draw always sees the latest committed values. A value update takes effect on the next presented frame after it is drained.
- **Rejected requests** return an error result from the registry call (grid larger than 128 × 128, more than 32 layers, unknown palette), with a log line; none assert.

### View, camera and draw order (Step 1)

- CellField draws in **its own view id (8)**, ordered by `bgfx::setViewOrder` between the entity view (4) and the debug view (5): sprites, then cell fields, then debug overlays, UI, ImGui. This avoids changing the debug view's sort mode, which existing render captures may depend on (the alternative, forcing view 5 to `ViewMode::Sequential`, is Open Question 4's fallback).
- The view uses the **same ortho matrix and view rect as the debug view**. The matrix code is factored into a shared helper used by both renderers, and acceptance test 8 asserts they agree to ±0.5 px.
- Blend state: alpha blending (needed for the backdrop and, in Step 2, opacity). Depth test and write off.

### Step 2 behaviours

- **Smooth shading:** a per-layer `shading` field (`Flat`, `Smooth`). Smooth uses the four-cell weights for the colour layer's floats. Category layers always draw flat (averaging category ids has no meaning).
- **Layer stacking and opacity:** Step 1 honours one colour request per frame (extras are dropped and counted). Step 2 raises the cap to 8 requests per frame, drawn in array order with `opacity` alpha blending (opacity is a request field from Step 1, ignored there).
- **Blend between two value states:** on a full `Update` the layer's previous state is the value it held before (kept as a second texture; the two swap roles, no copy). A draw takes `stateBlend` in `[0, 1]`: 0 shows the previous state, 1 the current state, values between interpolate the *values* before the palette or ramp lookup. Category layers ignore the factor and show the current state. A layer's first update sets previous = current.
- **Partial updates:** `UpdateCells(handle, cellIndices, values, count)` changes listed cells in the current state only. Contiguous runs are uploaded as row-region writes (`updateTexture2D`); everything else is untouched. A later full `Update` supersedes earlier partial ones.

## Public Interfaces

Sketches; names and exact signatures are settled in the plan. All public headers are free of bgfx types and STL containers (RB-006, PD-004). Namespaces `Dia::Core` (DiaCore) and `Dia::Graphics` / `Dia::Bgfx`.

```cpp
// Dia/DiaCore/DebugDraw/CellFieldDrawRequest.h  (POD, trivially copyable)
namespace Dia { namespace Core {

struct CellFieldLayerHandle { uint16_t index = 0xFFFF; uint16_t generation = 0; bool IsValid() const; };

enum class CellFieldShading : uint8_t { Flat = 0, Smooth = 1 };   // Smooth: Step 2

struct CellFieldDrawRequest
{
    CellFieldLayerHandle layer;
    StringCRC            colourMap;        // palette id (category layer) or ramp id (float layer)
    float                rampMin = 0.0f;   // float layers
    float                rampMax = 1.0f;
    float                inset = 0.0f;     // [0, 0.5)
    uint32_t             backdropColour = 0;   // RGBA8; alpha 0 = no backdrop

    // Relief hillshade (Step 1). hillshade.layer invalid = off.
    CellFieldLayerHandle elevationLayer;
    float                lightAzimuthDeg = 315.0f;
    float                lightElevationDeg = 45.0f;
    float                hillshadeStrength = 0.0f;
    float                elevationScale = 1.0f;

    // Step 2 fields: present from Step 1, defaulted, ignored (a warning is logged if set)
    CellFieldShading     shading = CellFieldShading::Flat;
    float                opacity = 1.0f;
    float                stateBlend = 1.0f;        // 0 = previous state, 1 = current state
};

}}  // IDebugDraw gains:  virtual void RequestDrawCellField(const CellFieldDrawRequest&) {}   // default no-op
```

```cpp
// Dia/DiaGraphics/CellField/CellFieldRegistry.h  (thread-safe; backend-agnostic)
namespace Dia { namespace Graphics {

enum class CellFieldValueKind : uint8_t { Category, Float };

struct CellGridDesc
{
    StringCRC       topology;               // e.g. "hex.flat.oddq"
    uint16_t        width, height;
    float           cellSize;               // centre-to-corner
    const float*    centresXY;              // 2 * width*height, copied at CreateLayer
    const int32_t*  neighbours;             // 6 * width*height, -1 = none, copied at CreateLayer
};

struct CellFieldResult { bool ok; const char* error; };    // error is a static string

class CellFieldRegistry
{
public:
    CellFieldResult CreateLayer(const CellGridDesc&, CellFieldValueKind, Core::CellFieldLayerHandle& out);
    CellFieldResult Update(Core::CellFieldLayerHandle, const float* values, uint32_t count);          // full; copied
    CellFieldResult UpdateCells(Core::CellFieldLayerHandle, const uint32_t* cells,
                                const float* values, uint32_t count);                                  // Step 2
    void            Destroy(Core::CellFieldLayerHandle);                                               // deferred
    CellFieldResult RegisterPalette(Core::StringCRC id, const uint32_t* rgba8, uint32_t count, uint32_t fallbackRgba8);
    CellFieldResult RegisterRamp(Core::StringCRC id, const CellFieldRampStop* stops, uint32_t count);

    // Observation counters: uploads, uploadedBytes, geometryBuilds, droppedCommands, staleHandles, ...
    CellFieldStats  GetStats() const;
};

}}
```

```cpp
// Dia/DiaGraphics/CellField/ICellGridGeometryBuilder.h
class ICellGridGeometryBuilder {
public:
    virtual ~ICellGridGeometryBuilder() = default;
    virtual StringCRC Topology() const = 0;
    virtual bool Build(const CellGridDesc&, CellGridGeometry& out) const = 0;   // CPU vertex array, no GPU
};

// Dia/DiaBgfx/Renderers/CellFieldRenderer.h  (bgfx types stay in the .cpp)
class CellFieldRenderer {
public:
    CellFieldRenderer(unsigned short viewId, ShaderProgram* program, CellFieldRegistry* registry);
    void SetCamera(const Dia::Graphics::Camera2D&);
    void OnCanvasSizeChanged(const Dia::Maths::Vector2D&);
    void Draw(const CellFieldFrameData&);       // drains the registry, then draws requests in order
    void Shutdown();                            // frees every GPU object
};
```

`FrameData` gains `CellFieldFrameData` (a `DynamicArrayC<CellFieldDrawRequest, 8>`; Step 1 uses one), included in `Copy` and `Clear`. `Canvas` creates `CellFieldRenderer` in `DeferredInit` beside the other renderers and calls it from `ProcessFrame` between the entity and debug draws.

## Dependencies

**Dia modules touched:** DiaCore (`DebugDraw`), DiaGraphics (`Frame`, new `CellField`), DiaBgfx (`Canvas`, new renderer, shaders, `.vcxproj`, `dia.bgfx` module doc), `pipeline.toml` unchanged (shaders are picked up by the existing `source_roots`).

**Outside Dia:** CluicheGameBaseline `KernelModule` (publish `KernelCellFieldRegistry`). Needs user sign-off when built from a CoW branch (CoW AD-001).

**Consumers (they depend on this spec, not the reverse):**

| Consumer | Needs | Step |
|----------|-------|------|
| S-1 World Generation, Feature 7 | Tiles, colour modes, inset, Relief | Step 1 |
| A4 Weather | Float overlays, stacking, opacity, blend, partial updates, smooth shading | Step 2 |
| S-2.5, S-3 | The same as A4 | Step 2 |
| Fog of war, other grid heat maps | Category and float layers | Step 1 / Step 2 |
| `DiaScalarField` heatmap overlay (today one rect per cell) | Could render through CellField | Step 1 |

**Related, not blocking:** dia-needs #13 (debug-draw budget 8,192, for the line and marker overlays that sit on top of CellField).

## Suggested build breakdown

Two steps, each its own plan. Step 1 can be marked Done without Step 2.

### Step 1: Core (blocks S-1 Feature 7)

| Task | Files |
|------|-------|
| 1a. Types and hook: `CellFieldDrawRequest`, `IDebugDraw::RequestDrawCellField` (default no-op), `CellFieldFrameData` in `FrameData` (`Copy`, `Clear`), `CellFieldRegistry` (layers, palettes, ramps, command queue, counters), `ICellGridGeometryBuilder` + `HexFlatTopOddQ` builder (pure CPU) | `Dia/DiaCore/DebugDraw/{CellFieldDrawRequest.h,IDebugDraw.h}`, `Dia/DiaGraphics/Frame/{FrameData.h,FrameData.cpp,CellFieldFrameData.*}`, `Dia/DiaGraphics/CellField/*` |
| 1b. Renderer: shader pair, geometry cache, value/palette/ramp textures, hillshade, inset and backdrop, view 8 and shared camera helper, `Canvas` wiring | `Dia/DiaBgfx/Shaders/{vs_cellfield.sc,fs_cellfield.sc}` (+ `varying.def.sc`), `Dia/DiaBgfx/Renderers/CellFieldRenderer.*`, `Dia/DiaBgfx/Resources/CellFieldGeometryCache.*`, `Canvas.cpp/.h`, `DebugRenderer.cpp` (camera helper), `DiaBgfx.vcxproj` |
| 1c. Sharing and checks: kernel publishes the registry service; golden-image and counter tests; module docs | `Cluiche/CluicheGameBaseline/Modules/KernelModule.*`, `Cluiche/Tests/GoogleTests/DiaGraphics/*`, `Dia/DiaBgfx/dia.bgfx.architecture.module.md` |

### Step 2: Extras (after S-1's view works; before the first overlay consumer needs them)

| Task | Files |
|------|-------|
| 2a. Smooth shading option and stacking with opacity (cap 8 requests) | `fs_cellfield.sc`, `CellFieldRenderer.*`, `CellFieldFrameData` |
| 2b. Two value states and `stateBlend`; partial updates (`UpdateCells`, row-run uploads) | `CellFieldRegistry.*`, `CellFieldRenderer.*`, `fs_cellfield.sc` |

## Acceptance Criteria

Each criterion is tagged **[S1]** or **[S2]**. Step 1 is Done when every [S1] criterion passes. Image checks use `ICanvas::RequestFrameCapture` (and DiaRenderTest where available) on small fixed grids; counters come from `CellFieldRegistry::GetStats()`.

1. **[S1] Geometry.** For a W × H hex grid the builder emits 18 vertices per hex (6 triangles) with consistent winding. Each centre equals `HexTopology::CellCenter(col, row)` within 1e-4, each corner offset is a unit hex at 0°, 60°, … 300°, and every triangle's cell references are `{c, N(k−1), N(k), N(k+1)}` with `-1` at the grid edge and per-vertex weights summing to 1. Checked on 1 × 1, 2 × 2, 5 × 4 and 66 × 42 grids, including edge and odd-column cells.
2. **[S1] Cache reuse.** Two layers of the same (topology, width, height) cause one geometry build; destroying one keeps it; destroying the second releases it; a layer of another size builds a second geometry.
3. **[S1] No upload when idle.** After create and one `Update`, 300 frames with no commands leave the value-upload and geometry-upload counters unchanged. Changing palette id, ramp id or `rampMin`/`rampMax` leaves them unchanged too.
4. **[S1] Full update.** `Update` changes the rendered colours on the next presented frame, and uploads exactly `width × height × 4` bytes.
5. **[S1] Colour mapping.** Each category id draws its palette entry; an out-of-range id draws the fallback colour and increments a counter; a float equal to `rampMin` / `rampMax` draws the first / last stop; values outside clamp.
6. **[S1] Inset and backdrop.** Inset 0: no backdrop pixels at shared edges. Inset > 0 with a backdrop alpha: backdrop colour visible at shared-edge midpoints, and the cell centre pixel still shows the cell colour.
7. **[S1] Hillshade.** A constant elevation layer gives uniform shading. A sloped elevation layer is brighter on the lit side than the shadowed side. Changing light azimuth changes the image with all upload counters unchanged. Strength 0 equals no hillshade. Shading is unchanged by zoom (same slope, two zoom levels, same shade ±2%).
8. **[S1] Camera parity.** A cell centre and a debug-draw circle submitted at the same world point land on the same pixel (±0.5 px) at three pan and zoom settings.
9. **[S1] Draw order.** A sprite renders under the field and a debug line renders over it (image check at an overlap).
10. **[S1] Frame data.** A `CellFieldDrawRequest` survives `FrameData::Copy` and is removed by `Clear`; submitting requests leaves `DebugFrameData::DroppedCount()` at 0 and the debug primitive count unchanged; an `IDebugDraw` implementer that doesn't override `RequestDrawCellField` still compiles and ignores it. Extra requests beyond the Step 1 cap are dropped and counted.
11. **[S1] Handle lifetime.** A destroyed handle (generation mismatch) in a request or command is skipped and counted with no crash; destroy racing an in-flight draw is safe; shutdown frees every GPU object (bgfx handle counts return to their pre-init values).
12. **[S1] Cross-thread hand-off.** 10,000 `Update` calls from a second thread while the render thread draws produce no data race (ASan/TSan clean where supported) and the final image equals the last update. Pending full updates coalesce (queue length ≤ 1 per layer). Exceeding the byte cap drops commands, increments the counter, logs once and never blocks.
13. **[S1] Rejected requests.** A grid larger than 128 × 128, a 33rd layer, an unknown palette and a mismatched value count each return an error result with a message, and nothing asserts.
14. **[S1] Golden image.** A fixed 12 × 8 grid with a category layer, a sloped elevation layer, inset 0.1 and a backdrop renders to an image matching the stored golden within a per-channel tolerance of 2.
15. **[S1] Performance (Release).** One layer at 66 × 42: render-thread CPU for drain + draw < 0.25 ms per frame in steady state; 128 × 128 < 1 ms; first-use geometry build at 128 × 128 < 25 ms; draw calls = 1 per layer (+1 for the backdrop); geometry memory ≤ 12 MB at 128 × 128.
16. **[S2] Smooth shading.** With two adjacent cells holding 0 and 1 and `shading = Smooth`, the pixel at the shared-edge midpoint maps the ramp value 0.5 and each cell's centre pixel maps exactly its own value; with `Flat` the same cells are uniform. Category layers stay flat in `Smooth`.
17. **[S2] Stacking and opacity.** Two layers at opacity 0.5 draw the 50% blend of their colours; swapping request order changes the result; requests beyond 8 are dropped and counted; opacity 1 equals a single layer drawn alone.
18. **[S2] State blend.** `stateBlend = 0` renders pixel-identical to the previous state drawn alone and `1` to the current state drawn alone; 0.5 maps the average of the two values through the ramp. Category layers ignore the factor. After a layer's first update, previous equals current. A second `Update` makes the old current the new previous with no texture copy (swap counter).
19. **[S2] Partial updates.** `UpdateCells` of two cells changes only those two cells' pixels, uploads at most the covering row-runs (counter), and leaves previous-state and other cells untouched. Duplicate cells in one call: last value wins. An out-of-range index rejects the whole call. A following full `Update` supersedes earlier partial ones.
20. **[S2] Step independence.** The Step 1 build compiles and passes every [S1] criterion with the Step 2 request fields present; setting them in a Step 1 build logs a warning and is otherwise ignored.
21. **[S2] Performance (Release).** Four stacked layers with state blend at 66 × 42: render-thread CPU < 0.6 ms per frame; a 100-cell partial update < 0.05 ms CPU.

## Inherited Binding Decisions

Only those that constrain this design; `render-backend.md` is a stub, so the RB decisions are quoted from the specs that cite them.

| ID | Decision | How it applies |
|----|----------|----------------|
| RB-001 | bgfx is the GPU abstraction | All GPU work uses bgfx, in DiaBgfx |
| RB-005 | No visitor pattern | `CellFieldRenderer` reads `CellFieldFrameData` directly |
| RB-006 | No backend types in DiaGraphics' public surface | Handles are plain `{index, generation}`; no bgfx header in DiaCore, DiaGraphics or the registry |
| PD-004 | No STL containers in public APIs | Public APIs take pointer + count or Dia containers |
| PD-005 | Windows only | `R32F` / D3D11 assumptions are Windows-only; shaders still cook for all configured backends |

## Open Design Questions

| # | Question | Impact | Resolution |
|---|----------|--------|------------|
| 1 | Fragment-fetch with 18 expanded vertices per hex (10.6 MB at 128 × 128) versus vertex-texture fetch with 7 shared vertices per hex (about 4 MB, needs `BGFX_CAPS_FORMAT_TEXTURE_VERTEX` for `R32F`)? | Memory and build time; not behaviour | Start with fragment fetch (needs no caps beyond sampling). Revisit if the caps check shows vertex fetch on every target GPU and memory matters. All acceptance criteria apply to either |
| 2 | Where does the `HexTopology → CellGridDesc` adapter live, given DiaGraphics/DiaBgfx may not be allowed to depend on DiaPathfinding? | Module layering (`dia check deps`) | Callers build the `CellGridDesc` from their topology (centres + neighbours are data), with a small adapter helper beside the topology if layering allows. Verify at plan time |
| 3 | Is derivative-based hillshade good enough, or do per-vertex normals from the neighbour data look better? | Visual quality at high zoom | Ship derivative-based; judge on the golden image and in S-1's view. The upgrade changes the shader only |
| 4 | Own view (id 8, `setViewOrder`) or draw into the debug view (5) with `ViewMode::Sequential`? | Risk to existing captures | Own view, to leave view 5's sort mode untouched. If `Canvas3D`'s view ordering conflicts, fall back to Sequential on view 5 after re-checking existing render captures |
