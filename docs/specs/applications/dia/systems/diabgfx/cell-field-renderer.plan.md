**Spec:** @docs/specs/applications/dia/systems/diabgfx/cell-field-renderer.md
**Status:** Not Started

## Implementation Patterns

### Module Layout and Namespaces (Tasks 1–6)
- **DiaCore types** (`Dia/DiaCore/DebugDraw/CellFieldDrawRequest.h`): `CellFieldLayerHandle`, `CellFieldShading`, `CellFieldDrawRequest`. POD, trivially copyable. Namespace `Dia::Core`.
- **IDebugDraw hook** (`Dia/DiaCore/DebugDraw/IDebugDraw.h`): `virtual void RequestDrawCellField(const CellFieldDrawRequest&) {}` — default no-op so other implementers are unaffected.
- **DiaGraphics Frame** (`Dia/DiaGraphics/Frame/CellFieldFrameData.h/.cpp`): `CellFieldFrameData` is a `DynamicArrayC<CellFieldDrawRequest, 8>` (Step 1 cap: 1). Added to `FrameData::Copy` and `Clear`.
- **DiaGraphics CellField** (new directory `Dia/DiaGraphics/CellField/`): `CellFieldRegistry.h/.cpp`, `ICellGridGeometryBuilder.h`, `HexFlatTopOddQBuilder.h/.cpp`. Namespace `Dia::Graphics`. Registry is thread-safe (mutex for command submission, atomic for handle generation). No bgfx types, no STL containers in public headers (PD-004).
- **DiaBgfx Renderers** (`Dia/DiaBgfx/Renderers/CellFieldRenderer.h/.cpp`): consumes `CellFieldFrameData`, drains registry, creates GPU objects, draws. Namespace `Dia::Bgfx`. All bgfx types stay in .cpp.
- **DiaBgfx Resources** (`Dia/DiaBgfx/Resources/CellFieldGeometryCache.h/.cpp`): keyed by `{StringCRC topology, uint16 w, uint16 h}`, reference-counted, freed when last layer dies.

### Geometry Builder (Task 4)
- `ICellGridGeometryBuilder::Build(const CellGridDesc& desc, CellGridGeometry& out)` is CPU-pure: no GPU, no bgfx. Returns `bool` (false = unsupported topology or invalid data).
- `CellGridDesc`: `{StringCRC topology, uint16 width, uint16 height, float cellSize, const float* centresXY, const int32_t* neighbours}`. Caller copies data in; no pointers held after `CreateLayer` returns.
- `CellGridGeometry`: `{uint32 vertexCount, uint32 indexCount, VertexCellField* vertices, uint16* indices}`. Vertex format: `{float2 centre, float2 offset, float4 cellIndices, unorm8[4] weights}` (36 bytes).
- `HexFlatTopOddQBuilder` topology id: `StringCRC("hex.flat.oddq")`. Each hex emits 18 vertices (6 triangles, no shared vertices), consistent winding (CCW), cell references `{c, N(k−1), N(k), N(k+1)}` with renormalized weights at the edge. AC1 asserts centres equal `HexTopology::CellCenter` within 1e-4.

### Registry (Tasks 5–6)
- `CellFieldRegistry`: fixed-capacity layer table (32 entries, generation per slot), palette table (128 entries), ramp table (128 entries). Command queue: 8 MB byte cap, dropped commands increment `mDroppedCommandCount` and log once (channel `CFL`, Warning).
- Handle: `{uint16 index : 12, uint16 generation : 4}`. `IsValid()` checks `index < kMaxLayers`. Stale handles are skipped and counted (`mStaleHandleCount`), never asserted.
- Commands: `Create`, `Update` (full), `Destroy`. Each copied into a registry-owned block at submit time (caller's pointers never held). Coalescing: a newer `Update` for the same layer replaces any pending one. `Destroy` is deferred to render thread drain.
- `RegisterPalette(id, rgba8[], count, fallbackRgba8)`: max 64 entries. `RegisterRamp(id, stops[], count)`: max 16 stops, baked into 256×1 texture.

### Shaders (Task 7)
- `Dia/DiaBgfx/Shaders/vs_cellfield.sc` and `fs_cellfield.sc`. Share `varying.def.sc` with existing shaders.
- Fragment shader samples the value texture (point, clamp), looks up palette or ramp, applies inset via vertex offset `centre + offset × cellSize × (1 − inset)`, and computes hillshade via `dFdx`/`dFdy` of interpolated elevation.
- Uniforms: `u_paletteOrRamp` (texture sampler), `u_rampMin`, `u_rampMax`, `u_inset`, `u_backdropColour`, `u_lightDir`, `u_hillshadeStrength`, `u_elevationScale`, `u_valueTexture`, `u_elevationTexture`.
- Cooked per backend (dx11, dx12, vulkan) by the existing `bgfx_shader_cook.py` (picked up from `pipeline.toml [bgfx_shaders] source_roots`).

### Renderer (Tasks 8–17)
- `CellFieldRenderer(viewId, program, registry)`: owns geometry cache, texture cache (per layer: value texture `R32F`, optional elevation texture), palette/ramp texture cache.
- `DrainRegistry()` (called from `Canvas::ProcessFrame` before drawing): process Create → build geometry if new size → upload static vertex/index buffers; process Update → `bgfx::updateTexture2D`; process Destroy → decrement cache ref, free if 0.
- `Draw(const CellFieldFrameData& frame)`: for each request, set view transform (shared camera helper), bind textures, set uniforms, draw backdrop rect if `inset > 0 && backdropAlpha > 0`, then `bgfx::submit(mViewId, program, request.layer.index)`. Blend state `BGFX_STATE_BLEND_ALPHA`. Depth off.
- View id 8 (`Canvas.h` shows 4=entity, 5=debug, 6=UI, 7=ImGui). Ordered via `bgfx::setViewOrder([..., 4, 8, 5, ...])` so CellField sits between entity and debug.
- Shared camera helper (Task 14): factor ortho matrix computation from `DebugRenderer::Draw` into `BuildOrtho2D(camera, canvasSize, float outMatrix[16])` in a shared header, used by both.

### Canvas Integration (Task 15)
- `Canvas::DeferredInit()`: create `CellFieldRenderer` alongside sprite/debug/UI renderers, load `vs_cellfield`/`fs_cellfield` programs.
- `Canvas::ProcessFrame(frame)`: call `mCellFieldRenderer->DrainRegistry()`, then `Draw(frame.GetCellFieldFrameData())` between entity and debug draws.
- `Canvas::Shutdown()`: destroy renderer, free all GPU objects.

### Cross-Thread Hand-Off (Task 16)
- Simulation thread (SimPU) calls `registry->CreateLayer/Update/Destroy` → command copied into queue, returns immediately (mutex held only for queue push).
- Render thread (RenderPU) calls `renderer->DrainRegistry()` at frame start → drains whole queue, creates/updates GPU objects.
- Ordering: drain before draw, so a draw always sees the latest values. A value update takes effect on the next presented frame.
- Thread safety: `std::mutex` for command queue, `std::atomic` for handle generation. No cross-thread raw pointers (all data copied).

### Rejected Requests and Limits (Task 17)
- `CreateLayer`: grid > 128×128 → `{ok:false, error:"Grid exceeds 128×128"}`, log Warning. Layer count ≥ 32 → `{ok:false, error:"Max 32 layers"}`, log Warning. Unknown topology → `{ok:false, error:"Unsupported topology"}`, log Error.
- `Update`: mismatched value count → `{ok:false, error:"Value count mismatch"}`, log Warning.
- Draw request: unknown palette/ramp id → skip request, increment `mUnknownColourMapCount`, log Warning once.
- Nothing asserts; all errors return `CellFieldResult` or are silently skipped and counted.

### KernelModule Sharing (Task 18)
- `CluicheGameBaseline/Modules/KernelModule.h/.cpp`: owns `CellFieldRegistry*`, publishes as `ServiceStream<CellFieldRegistry>` named `"KernelCellFieldRegistry"` (following `KernelTextureHandler` pattern).
- `Deregister()` called before shutdown. Other PUs access via `ServiceStreamReader`.

### Logging and Observation
- **Channel:** `CFL` (CellField). Info: layer created/destroyed. Warning: command dropped (queue overflow), layer rejected (limit), stale handle, unknown colour map. Error: geometry build failed.
- **Profiling:** `DIA_PROFILE_SCOPE("CellField.DrainRegistry")`, `DIA_PROFILE_SCOPE("CellField.Draw")`, `DIA_PROFILE_SCOPE("CellField.BuildGeometry")`.
- **Metrics:** `MetricRegistry` counters: `cellfield.uploads`, `cellfield.uploaded_bytes`, `cellfield.geometry_builds`, `cellfield.dropped_commands`, `cellfield.stale_handles`, `cellfield.unknown_colour_maps`.

### Test Files and Strategy
- GoogleTest suite: `Cluiche/Tests/GoogleTests/DiaGraphics/TestCellField.cpp` and `Cluiche/Tests/GoogleTests/DiaBgfx/TestCellFieldRenderer.cpp`.
- AC1 (geometry): test builder on 1×1, 2×2, 5×4, 66×42 grids; assert centres, offsets, cell references, weights.
- AC2 (cache): two layers same size → one build; destroy one → still cached; destroy both → freed.
- AC3 (idle): 300 frames with no commands → upload counters unchanged.
- AC4 (full update): `Update` changes colours next frame, uploads `w×h×4` bytes.
- AC5 (colour mapping): category ids → palette entries; out-of-range → fallback; float → ramp; clamp.
- AC6 (inset/backdrop): inset 0 → no gaps; inset > 0 → backdrop visible at shared edges.
- AC7 (hillshade): constant elevation → uniform shade; sloped → lit/shadowed; light change → no upload; strength 0 → no shade; zoom invariant.
- AC8 (camera parity): cell centre and debug circle at same world point land on same pixel (±0.5 px) at 3 pan/zoom settings.
- AC9 (draw order): sprite under field, debug line over field (image check).
- AC10 (frame data): `CellFieldDrawRequest` survives `Copy`, removed by `Clear`; no primitive budget consumed; extra requests beyond cap dropped and counted.
- AC11 (handle lifetime): stale handle → skipped and counted; destroy racing draw → safe; shutdown → all GPU objects freed.
- AC12 (cross-thread): 10,000 `Update` calls from second thread while rendering → ASan/TSan clean, final image equals last update; coalescing works; cap exceeded → drops, counts, logs once, never blocks.
- AC13 (rejected): grid > 128×128, 33rd layer, unknown palette, mismatched value count → error result, no assert.
- AC14 (golden): 12×8 grid, category layer, sloped elevation, inset 0.1, backdrop → matches golden within tolerance 2.
- AC15 (performance): 66×42 drain+draw < 0.25 ms; 128×128 < 1 ms; first geometry build 128×128 < 25 ms; draw calls = 1 per layer (+1 backdrop); memory ≤ 12 MB at 128×128.

### vcxproj Updates (Task 19)
- `dia docs vcxproj-add DiaCore "DebugDraw\\CellFieldDrawRequest.h"`
- `dia docs vcxproj-add DiaGraphics "Frame\\CellFieldFrameData.h" --filter Frame`
- `dia docs vcxproj-add DiaGraphics "Frame\\CellFieldFrameData.cpp" --filter Frame`
- `dia docs vcxproj-add DiaGraphics "CellField\\CellFieldRegistry.h" --filter CellField`
- `dia docs vcxproj-add DiaGraphics "CellField\\CellFieldRegistry.cpp" --filter CellField`
- `dia docs vcxproj-add DiaGraphics "CellField\\ICellGridGeometryBuilder.h" --filter CellField`
- `dia docs vcxproj-add DiaGraphics "CellField\\HexFlatTopOddQBuilder.h" --filter CellField`
- `dia docs vcxproj-add DiaGraphics "CellField\\HexFlatTopOddQBuilder.cpp" --filter CellField`
- `dia docs vcxproj-add DiaBgfx "Renderers\\CellFieldRenderer.h" --filter Renderers`
- `dia docs vcxproj-add DiaBgfx "Renderers\\CellFieldRenderer.cpp" --filter Renderers`
- `dia docs vcxproj-add DiaBgfx "Resources\\CellFieldGeometryCache.h" --filter Resources`
- `dia docs vcxproj-add DiaBgfx "Resources\\CellFieldGeometryCache.cpp" --filter Resources`
- `dia docs vcxproj-add DiaBgfx "Shaders\\vs_cellfield.sc" --filter Shaders` (as `None` item)
- `dia docs vcxproj-add DiaBgfx "Shaders\\fs_cellfield.sc" --filter Shaders` (as `None` item)

### Module Doc Updates (Task 20)
- `Dia/DiaBgfx/dia.bgfx.architecture.module.md`: add `CellFieldRenderer` to public headers and responsibilities.
- `Dia/DiaGraphics/Frame/dia.graphics.frame.architecture.module.md` (if exists): add `CellFieldFrameData` to public headers.
- `Dia/DiaGraphics/CellField/dia.graphics.cellfield.architecture.module.md` (new): describe registry, builder interface, responsibilities.

### Consumer Fit: DiaCellLayers and DiaValueSchema (amended 2026-10-19)

CellField's only shipped caller is the DiaCellLayers viewer (@docs/specs/applications/dia/systems/diacelllayers/diacelllayers.md). Its colours come from DiaValueSchema's named ramps (@docs/specs/applications/dia/systems/diavalueschema/diavalueschema.md). Step 1 needs **no API change** for either; these are the usage rules the tests must respect:

- **Lazy layers.** The viewer calls `CreateLayer` only for the visible base layer (and later, in Step 2, stacked layers). The DiaCellLayers registry holds up to 128 logical layers; CellField's 32 GPU layers are a cache.
  - `CreateLayer` beyond 32 returns an error result (Task 17), so the viewer can evict its least-recently-visible layer and retry.
  - A test creates 32 layers, then a 33rd: it must fail cleanly, and succeed after one `Destroy`.
- **Version-gated uploads.** The viewer calls `Update` only when a provider's `version` changes. Switching back to a layer whose values haven't changed must not re-upload; this is a viewer test. CellField's part is that changing palette or ramp on the draw request uploads nothing (spec), and a test asserts zero value-texture uploads on a mapping-only change.
- **Ramps by name.** The viewer registers each DiaValueSchema ramp (`sequential.green`, `diverging.blue_red`, …) once via `RegisterRamp(StringCRC(name), stops, count)`. Category palettes from schema enums go through `RegisterPalette`. CellField never depends on DiaValueSchema, and names are just StringCRCs.
  - Tests register a ramp under a dotted name and draw with it.
  - Re-registering the same name replaces the ramp (or is rejected; choose one in Task 5 and test it).
- **Legend data.** The viewer builds legends itself from the schema `min`/`max` and its own "seen in view" range. CellField exposes nothing extra.
- **Picking.** This is not CellField's job. DiaCellLayers uses `HexTopology::CellFromPoint` (spec non-goal, unchanged).
- **Sizes.** Size policy 2026-10-19: Small 66×42, Standard 80×52 and Large 104×64 are all supported. The AC15 memory check and the perf tasks add Large (104×64) alongside the spec's 66×42 and 128×128 figures.

## Resolved Build Questions

### Q1: Hex adapter placement (spec Open Question 2)
**Resolution:** Callers build `CellGridDesc` from their topology; adapter lives with caller code, not in DiaGraphics/DiaBgfx.

**Evidence:** `dia.graphics.architecture.module.md` and `dia.bgfx.architecture.module.md` list no dependency on `dia.pathfinding`. DiaGraphics and DiaBgfx cannot depend on DiaPathfinding (layering rule). `CellGridDesc` is plain data (centres + neighbours arrays), so the caller copies the data from `HexTopology` at layer creation time. **The shipped caller is DiaCellLayers** (Approved 2026-10-19). It owns the `HexTopology` → `CellGridDesc` adapter, and DiaCellLayers may depend on DiaPathfinding. Games never call CellField directly for map layers; they register layers with DiaCellLayers. Until DiaCellLayers exists, tests use a `HexTopologyToCellGridDesc(HexTopology, float size)` helper in `GoogleTests/DiaGraphics/` (amended 2026-10-19, consumer fit).

### Q2: View ordering (spec Open Question 4)
**Resolution:** Use view id 8, ordered between entity (4) and debug (5) via `bgfx::setViewOrder`.

**Evidence:** `Canvas.h:77-80` defines `kEntityViewId=4, kDebugViewId=5, kUIViewId=6, kImGuiViewId=7`. View 8 is unused. `bgfx::setViewOrder` with `ViewMode::Sequential` controls draw sequence without changing view 5's sort mode (which existing captures may depend on). No conflict with `Canvas3D` (which owns 0–3).

### Q3: Derivative-based hillshade quality (spec Open Question 3)
**Resolution:** Ship derivative-based (`dFdx`/`dFdy`) in Step 1; assess quality at AC14 and AC7.

**Evidence:** Fragment shader can compute slope normal from `dFdx(elevation)` and `dFdy(elevation)` scaled by world-units-per-pixel, then `dot(normal, lightDir)` for shading. Known limitation: constant per 2×2 quad, may look faceted at high zoom. If AC14 golden or S-1's view shows unacceptable quality, upgrade to per-vertex normals (shader change only, no API impact). AC3 gate confirms the upgrade doesn't break Step 1 behaviour.

### Q4: R32F fragment fetch vs vertex fetch (spec Open Question 1)
**Resolution:** Fragment-fetch (sample value texture in fragment shader) for Step 1.

**Evidence:** All target GPUs (D3D11 feature level 11+) support sampling `R32F` textures in the fragment shader. Vertex-texture fetch would need `BGFX_CAPS_FORMAT_TEXTURE_VERTEX` and reduce memory (7 vertices/hex vs 18), but adds caps-check complexity. Geometry at 128×128 is 10.6 MB (18 vertices/hex) vs ~4 MB (7 vertices/hex) — acceptable for v1. Revisit if memory profiling shows 10 MB is problematic. AC15 measures memory; acceptance criterion is ≤ 12 MB at 128×128, so the 10.6 MB budget fits.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Types: CellFieldDrawRequest, CellFieldLayerHandle, CellFieldShading in DiaCore/DebugDraw/CellFieldDrawRequest.h | Compiles | Not Started | haiku | POD, trivially copyable, no logic |
| 2 | IDebugDraw::RequestDrawCellField hook (default no-op) + test default ignored | TestDebugDrawCellFieldDefaultNoOp PASSES | Not Started | sonnet | Non-breaking interface extension |
| 3 | CellFieldFrameData in DiaGraphics/Frame/ + FrameData Copy/Clear + test AC10 frame data | TestCellFieldFrameData Copy/Clear PASSES | Not Started | sonnet | TDD: RED (Copy drops data) → GREEN (Copy preserves, Clear empties) |
| 4 | ICellGridGeometryBuilder + HexFlatTopOddQBuilder (CPU pure) + test AC1 geometry | TestHexGeometryBuilder PASSES | Not Started | sonnet | TDD: RED (centres/offsets/weights wrong) → GREEN (all correct within 1e-4); test 1×1, 2×2, 5×4, edge cells |
| 5 | CellFieldRegistry (layers, CreateLayer/Update/Destroy, handles, command queue) + test AC11 handle lifetime | TestCellFieldRegistry handle generation/stale/destroy PASSES | Not Started | sonnet | TDD: RED (stale handle crashes) → GREEN (skipped and counted); thread-safe submission |
| 6 | Registry RegisterPalette/RegisterRamp + tests palette/ramp storage and lookup | TestCellFieldRegistry palette/ramp PASSES | Not Started | sonnet | TDD: RED (unknown id crashes) → GREEN (stored and retrieved) |
| 7 | Shader files: vs_cellfield.sc, fs_cellfield.sc, varying.def.sc under DiaBgfx/Shaders/ | Shaders cook for dx11/dx12/vulkan | Not Started | sonnet | Add to pipeline, verify cook output exists |
| 8 | CellFieldGeometryCache (keyed by topology/w/h, ref-counted) + test AC2 cache reuse | TestGeometryCache reuse/release PASSES | Not Started | sonnet | TDD: RED (two layers → two builds) → GREEN (two layers same size → one build, destroy → ref count) |
| 9 | CellFieldRenderer skeleton + value texture upload + tests AC3 idle, AC4 full update | TestCellFieldRenderer idle/update PASSES | Not Started | sonnet | TDD: RED (idle uploads) → GREEN (upload only on Create/Update); AC4: Update uploads w×h×4 bytes |
| 10 | Palette colour mapping in shader + renderer + test AC5 palette | TestCellFieldRenderer palette lookup PASSES | Not Started | sonnet | TDD: RED (wrong colour) → GREEN (category id → palette entry, out-of-range → fallback, counted) |
| 11 | Ramp colour mapping in shader + renderer + test AC5 ramp | TestCellFieldRenderer ramp lookup PASSES | Not Started | sonnet | TDD: RED (wrong interpolation) → GREEN (float → ramp [min,max], clamp) |
| 12 | Inset and backdrop (vertex shrink, backdrop rect) + test AC6 | TestCellFieldRenderer inset PASSES | Not Started | sonnet | TDD: RED (gaps at inset=0) → GREEN (inset=0 seamless, inset>0 backdrop visible at shared edges) |
| 13 | Hillshade (elevation texture, light uniforms, dFdx/dFdy shader) + test AC7 | TestCellFieldRenderer hillshade PASSES | Not Started | sonnet | TDD: RED (no shading) → GREEN (sloped → lit/shadowed, strength=0 → none, light change → no upload, zoom invariant) |
| 14 | View 8 setup + shared camera helper BuildOrtho2D + test AC8 camera parity | TestCellFieldRenderer camera parity PASSES | Not Started | sonnet | TDD: RED (camera mismatch) → GREEN (cell centre and debug circle land on same pixel ±0.5 px at 3 pan/zoom) |
| 15 | Canvas integration (create renderer, ProcessFrame wiring, view ordering) + test AC9 draw order | TestCellFieldRenderer draw order PASSES | Not Started | sonnet | TDD: RED (wrong order) → GREEN (sprite under, debug line over, image check) |
| 16 | Cross-thread hand-off (command queue drain, coalescing, 8MB cap, counters) + test AC12 threading | TestCellFieldRegistry threading PASSES | Not Started | sonnet | TDD: 10k Update calls from 2nd thread while rendering → ASan/TSan clean, final image = last update, coalescing works, cap → drops/counts/logs |
| 17 | Rejected requests (grid size, layer count, unknown palette, mismatched values) + test AC13 | TestCellFieldRegistry rejected PASSES | Not Started | sonnet | TDD: RED (crashes on bad input) → GREEN (error result + log, no assert); grid>128×128, 33rd layer, unknown palette, mismatch count |
| 18 | KernelModule publishes KernelCellFieldRegistry service (CluicheGameBaseline) | Compiles, service registered | Not Started | haiku | Following KernelTextureHandler pattern; Deregister before shutdown |
| 19 | vcxproj updates via dia docs vcxproj-add for all new files | Builds | Not Started | haiku | 14 `dia docs vcxproj-add` commands per Implementation Patterns |
| 20 | Module doc updates: dia.bgfx, dia.graphics.frame, new dia.graphics.cellfield | Docs consistent | Not Started | haiku | Update responsibilities, public headers, dependencies |
| 21 | Run `dia run googletest --filter=CellField*` — verify all tests pass | All CellField* tests PASS | Not Started | haiku | Verification gate; quote pass count |
| 22 | Visual verification: AC14 golden image (12×8 grid, category + elevation + inset + backdrop) | Frame capture matches golden within tolerance 2 | Not Started | sonnet | Use ICanvas::RequestFrameCapture, write golden to out/test/cellfield_golden.png, compare per-channel |
| 23 | Performance verification: AC15 (66×42 < 0.25ms, 128×128 < 1ms, draw calls, memory) | AC15 criteria met | Not Started | sonnet | Profile drain + draw timing, log draw call count, measure geometry memory, assert limits |
| 24 | Observation scan: check all touched files for missed DIA_LOG/TRACE/PROFILE/Metric/Health opportunities | Findings reported | Not Started | sonnet | Scan CellFieldRegistry, CellFieldRenderer, HexFlatTopOddQBuilder, CellFieldGeometryCache; report gaps (not blocking Done) |

## Step 2 (Later)

Step 2 tasks (after S-1's view works, before A4 needs them):
- **Smooth shading:** per-layer `shading` field (Flat/Smooth), shader uses four-cell weights for Smooth, category layers stay Flat.
- **Layer stacking and opacity:** raise cap from 1 to 8 requests per frame, draw in array order with `opacity` alpha blending.
- **State blend:** keep two value textures per layer (previous/current), swap on `Update`, shader interpolates values at `stateBlend` factor before colour lookup. Category layers ignore factor.
- **Partial updates:** `UpdateCells(handle, cellIndices[], values[], count)` uploads row-region writes via `bgfx::updateTexture2D`, untouched cells stay unchanged.
- **AC16–21:** tests for Step 2 behaviours, golden image with stacked layers, performance with 4 layers + state blend.

The Step 2 API fields (`shading`, `opacity`, `stateBlend`, `elevationLayer`) exist from Step 1 (defaulted and ignored), so Step 1 callers don't change when Step 2 ships. Setting them in Step 1 logs a Warning and is otherwise no-op (AC20 checks this).
