**Spec:** @docs/specs/applications/dia/systems/diabgfx/cell-field-renderer.md
**Status:** Not Started. Build sign-off (AD-001) given by the user 2026-10-20; not started (implementation deferred by the user).

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
| 1 | Types: CellFieldDrawRequest, CellFieldLayerHandle, CellFieldShading in DiaCore/DebugDraw/CellFieldDrawRequest.h | Compiles | Done | haiku | POD, trivially copyable. `colourMap` is `uint32_t` (the CRC value), not `StringCRC`: StringCRC is not trivially copyable (64-byte string, user copy ops). Callers pass `StringCRC(name).Value()`; registry ids stay StringCRC-compatible |
| 2 | IDebugDraw::RequestDrawCellField hook (default no-op) + test default ignored | TestDebugDrawCellFieldDefaultNoOp PASSES | Done | sonnet | `CellFieldDebugDraw.TestDebugDrawCellFieldDefaultNoOp` passes (`--filter=CellField*`, 1 test); full GoogleTests build OK |
| 3 | CellFieldFrameData in DiaGraphics/Frame/ + FrameData Copy/Clear + test AC10 frame data | TestCellFieldFrameData Copy/Clear PASSES | Not Started | sonnet | TDD: RED (Copy drops data) → GREEN (Copy preserves, Clear empties) |
| 4 | ICellGridGeometryBuilder + HexFlatTopOddQBuilder (CPU pure) + test AC1 geometry | TestHexGeometryBuilder PASSES | Not Started | sonnet | TDD: RED (centres/offsets/weights wrong) → GREEN (all correct within 1e-4); test 1×1, 2×2, 5×4, edge cells |
| 5 | CellFieldRegistry (layers, CreateLayer/Update/Destroy, handles, command queue) + test AC11 handle lifetime | TestCellFieldRegistry handle generation/stale/destroy PASSES | Done | sonnet | RED: compile failure (no header); GREEN: `CellField*` 26/26 (12 registry tests). Consumer API: `ICellFieldCommandSink` + `Drain(sink)` + `ResolveHandle`; pimpl; slot freed and generation bumped only after the Destroy is drained. 8 MB cap and 32-layer / 128×128 rejections already implemented here; cap logic untested (Task 16), rejections partly covered (Task 17) |
| 6 | Registry RegisterPalette/RegisterRamp + tests palette/ramp storage and lookup | TestCellFieldRegistry palette/ramp PASSES | Done | sonnet | RED: compile errors (no `RegisterPalette`); GREEN: `CellField*` 39/39 (13 new, suite `CellFieldColourMaps`). Re-register replaces and bumps `revision`; keyed by `id.Value()`; `BakeRamp` (256) / `BakePalette` (64, padded with fallback) are pure free functions |
| 7 | Shader files: vs_cellfield.sc, fs_cellfield.sc, varying.def.sc under DiaBgfx/Shaders/ | Shaders cook for dx11/dx12/vulkan | Done | sonnet | Cooked dx11/dx12/vulkan, existing shaders still cook. Layout is documented at the top of `vs_cellfield.sc`/`fs_cellfield.sc`: vertex attrs TexCoord0 centre(2f), TexCoord1 offset(2f), TexCoord2 cellIndices(4f), Color0 weights(4×u8 norm); vec4 uniforms `u_params0..2`, `u_gridInfo`, `u_fallbackColour`, `u_backdropColour`, `u_lightDir`; samplers stage 0 palette/ramp, 1 value, 2 elevation (bind a dummy at 2 when no hillshade); mapMode 2 = solid backdrop. `varying.def.sc` must stay comment-free |
| 8 | CellFieldGeometryCache (keyed by topology/w/h, ref-counted) + test AC2 cache reuse | TestGeometryCache reuse/release PASSES | Done | sonnet | TDD: RED (two layers → two builds) → GREEN (two layers same size → one build, destroy → ref count) Done: RED compile failure; GREEN `CellField*` 47/47 (8 new). Seam `ICellFieldGpuGeometryFactory` (fake in tests); `BgfxCellFieldGpuGeometryFactory` packs vb id (low 16) / ib id (high 16) into an opaque handle; Acquire/Release(key)/Shutdown/Stats. Vertex layout corrected by orchestrator to TexCoord0/1/2 + Color0 to match `vs_cellfield.sc`. No MetricRegistry counter yet (Task 24). `dia.bgfx` module doc update left to Task 20 |
| 9 | CellFieldRenderer skeleton + value texture upload + tests AC3 idle, AC4 full update | TestCellFieldRenderer idle/update PASSES | Done | sonnet | TDD: RED (idle uploads) → GREEN (upload only on Create/Update); AC4: Update uploads w×h×4 bytes Done: RED compile failure; GREEN `CellField*` 55/55 (8 new, suite `CellFieldRenderer`). Seam `ICellFieldGpuTextureFactory`; second ctor injects geometry+texture factories; registry stats are the single source for uploads/bytes/builds; Create uploads nothing; null program → Draw drains+resolves only. Tasks 10–13 fill `SubmitLayer`; need a palette/ramp texture cache keyed by `revision` |
| 10 | Palette colour mapping in shader + renderer + test AC5 palette | TestCellFieldRenderer palette lookup PASSES | Done | sonnet | TDD: RED (wrong colour) → GREEN (category id → palette entry, out-of-range → fallback, counted) Done: RED stub failures; GREEN `CellField*` 66/66 (11 new). Pure `CellFieldDrawParams`/`BuildCellFieldDrawParams` + CPU `CellFieldPaletteLookup` mirror of the shader; `ICellFieldGpuColourMapFactory` colour-map cache keyed by id+revision; out-of-range category cells counted on the CPU (kept copy ≤ 64 KB, rescan only when valueVersion/palette/revision change). bgfx submit path compiled but unrun until Task 22 golden. Counters not yet in MetricRegistry (Task 24) |
| 11 | Ramp colour mapping in shader + renderer + test AC5 ramp | TestCellFieldRenderer ramp lookup PASSES | Done | sonnet | TDD: RED (wrong interpolation) → GREEN (float → ramp [min,max], clamp) Done: RED compile failure (`CellFieldRampLookup` missing); GREEN `CellField*` 80/80 (14 new). CPU `CellFieldRampLookup` mirrors the shader (`i0=floor(t*255)`, lerp to `i0+1`); NaN → first stop; min==max range taken as 1; reversed min/max inverts; ramp id on a Category layer (or palette id on Float) counts as unknown colour map |
| 12 | Inset and backdrop (vertex shrink, backdrop rect) + test AC6 | TestCellFieldRenderer inset PASSES | Done | sonnet | TDD: RED (gaps at inset=0) → GREEN (inset=0 seamless, inset>0 backdrop visible at shared edges) Done: RED compile errors; GREEN `CellField*` 91/91 (11 new). Inset clamp [0,0.499], NaN→0; per-layer bounds computed at Create; backdrop = 6-vertex transient quad, mapMode 2, submitted before the layer; stats `drawCalls`/`backdropDraws`; null program records the planned submits. **Task 14 must set view 8 to `ViewMode::Sequential`** so the backdrop-then-layer order is guaranteed (bgfx default mode sorts). Backdrop submit unrun until Task 22 |
| 13 | Hillshade (elevation texture, light uniforms, dFdx/dFdy shader) + test AC7 | TestCellFieldRenderer hillshade PASSES | Done | sonnet | TDD: RED (no shading) → GREEN (sloped → lit/shadowed, strength=0 → none, light change → no upload, zoom invariant) Done: RED compile errors; GREEN `CellField*` 104/104 (13 new); agent also ran full `dia run googletest` = 9688 pass. Light vector `(sin az·cos el, −cos az·cos el, sin el)`, az clockwise from screen-up; ambient 0.4; invalid/stale/Category/mismatched-grid elevation → hillshade off, colour layer still drawn; dummy 1×1 elevation texture. **Risk for Task 22:** the shader's dFdy sign differs between D3D and GL/Vulkan; the golden must check lighting direction on dx11 and vulkan |
| 14 | View 8 setup + shared camera helper BuildOrtho2D + test AC8 camera parity | TestCellFieldRenderer camera parity PASSES | Done | sonnet | TDD: RED (camera mismatch) → GREEN (cell centre and debug circle land on same pixel ±0.5 px at 3 pan/zoom) Done: RED `GetViewMatrix` missing; GREEN `CellField*` 109 (5 new `CellFieldCamera`), `*Debug*` 1009 pass. `Ortho2D.h/.cpp` (`BuildOrtho2D` calls the real `bx::mtxOrtho` — orchestrator replaced the agent's hand copy so DebugRenderer cannot drift; golden matrices still pass), y-down pixel convention, `kCellFieldViewId = 8` + `BuildCellFieldViewOrder` → {4,8,5,6,7} in `CellFieldViewOrder.h`; renderer sets view 8 Sequential. Canvas wiring is Task 15 |
| 15 | Canvas integration (create renderer, ProcessFrame wiring, view ordering) + test AC9 draw order | TestCellFieldRenderer draw order PASSES | Done (AC9 open) | sonnet | TDD: RED (wrong order) → GREEN (sprite under, debug line over, image check) Wired: `Canvas::SetCellFieldRegistry` (borrowed ptr, before init; no registry → no renderer), view 8 set via `setViewOrder(4,5,{4,8,5,6,7})` each frame, Draw between sprite and debug, Shutdown before bgfx. RED compile errors; GREEN `CellField*` 114 (5 new); `dia run cluichetest --build-only` and `dia run cow --build-only` pass. Also fixed `DiaWindow/SystemHandle.h` guard (`WIN32` → `WIN32 || _WIN32`) so Canvas.h compiles in GoogleTests. **AC9 NOT verified:** no GPU-backed render/capture test exists in GoogleTests; fold into Task 22 as a CluicheTest stage (`dia scaffold stage CellFieldDrawOrder`, capture + 3 pixel asserts via `dia test e2e`), which needs Task 18 |
| 16 | Cross-thread hand-off (command queue drain, coalescing, 8MB cap, counters) + test AC12 threading | TestCellFieldRegistry threading PASSES | Done (TSan n/a) | sonnet | TDD: 10k Update calls from 2nd thread while rendering → ASan/TSan clean, final image = last update, coalescing works, cap → drops/counts/logs RED 6/23 failed (no coalescing); GREEN `CellField*` 126 (12 new in `TestCellFieldRegistryThreading.cpp`). In-place coalescing keeps the first pending update's queue position; values copied before the lock; one cap warning and one coalesce log per drain cycle; new stats `coalescedCommands`, `capWarningsLogged`. `dia check sanitizer` (ASan) clean over 9777 tests. **UBSan config failed to build (not investigated); TSan not available on MSVC** — AC12 'TSan clean' is not demonstrated, only ASan + producer/consumer tests. Edited one Task 5 test (`StatsReflectDrainedWork`) for coalescing |
| 17 | Rejected requests (grid size, layer count, unknown palette, mismatched values) + test AC13 | TestCellFieldRegistry rejected PASSES | Done (RED not observed) | sonnet | TDD: RED (crashes on bad input) → GREEN (error result + log, no assert); grid>128×128, 33rd layer, unknown palette, mismatch count GREEN `CellField*` 135 (9 new in `TestCellFieldRejected.cpp`). **No observed RED:** validation was written before the tests (process deviation; RED was predicted, not run). Added `RegisterSupportedTopology` (default `hex.flat.oddq`; callers must now set a registered topology in `CellGridDesc`), `cellSize` > 0 and finite check, renderer stat `unbuiltLayerRequests`. Neighbour-range validation stays in the geometry builder. Retry-after-drain contract documented in the header |
| 18 | KernelModule publishes KernelCellFieldRegistry service (CluicheGameBaseline) | Compiles, service registered | Done | sonnet | Following KernelTextureHandler pattern; Deregister before shutdown Model raised haiku→sonnet by orchestrator (manifest orphan-provider risk). KernelModule creates the registry before `Canvas3D`, calls `SetCellFieldRegistry` before Initialize, registers `KernelCellFieldRegistry`, Deregister → delete canvas → delete registry. Stream declared in `cluiche_main.diaapp`, `cow_main.diaapp`, and `main.diaapp.j2`. **Stopgap:** KernelModule is listed as both provider and consumer to clear `SERVICE_STREAM_ORPHAN_PROVIDER`; when a real consumer exists (e.g. a RenderPU/CellLayers module), drop KernelModule's `consumes` in all three files. `dia validate manifest` 71 ok; cluichetest/cow build-only pass; both launched 40 s with no manifest errors and `KernelCellFieldRegistry` registered. Not verified: renderer creation (inferred from absent warnings), normal shutdown order |
| 19 | vcxproj updates via dia docs vcxproj-add for all new files | Builds | Done | haiku | 14 `dia docs vcxproj-add` commands per Implementation Patterns Done (incrementally, tasks 1–15); orchestrator confirmed all 14 files are in their vcxproj files |
| 20 | Module doc updates: dia.bgfx, dia.graphics.frame, new dia.graphics.cellfield | Docs consistent | Done | haiku | Update responsibilities, public headers, dependencies Done: dia.bgfx, dia.graphics.frame, dia.graphics, dia.core.debugdraw updated; new dia.graphics.cellfield; `dia docs registry` and `dia check deps` pass (0 missing) |
| 21 | Run `dia run googletest --filter=CellField*` — verify all tests pass | All CellField* tests PASS | Done | haiku | Verification gate; quote pass count `CellField*` 135/135 fresh run before Task 22 |
| 22 | Visual verification: AC14 golden image (12×8 grid, category + elevation + inset + backdrop) | Frame capture matches golden within tolerance 2 | Done (dx11 only) | sonnet | Use ICanvas::RequestFrameCapture, write golden to out/test/cellfield_golden.png, compare per-channel First real render found and fixed a shader bug (Flat shading left elevation per-cell → no relief; `fs_cellfield.sc` now always blends elevation through the four-cell weights). Stage `CellFieldRenderTestStage` + E2E `cellfield_render` (7 pass). AC9 draw order OK; AC14 golden max delta 0 (tol 2, 800×600 centre crop; golden `Cluiche/Assets/CluicheTest/captures/reference/CellFieldRender_golden.png`); AC8 centroid error ≤0.32 px; AC7 constant elevation spread 0, strength 0 = unshaded, light change 0 uploads; lighting direction correct on dx11 (NW light: NW-facing lum 117.7 vs 76.5 under SE light). User also eyeballed the live window: looked right. **Unverified:** Vulkan/dx12 (`dFdy` sign), other GPUs/DPI. **Visible triangle faceting** inside hexes (Q3 derivative limitation) — needs a quality decision (per-vertex normals upgrade). Also added `IFrameCaptureSource::ReleaseFrameCapture` (Dia capture ring slots were never released → max 4 captures/process). Other E2E scenarios (geometry2d etc.) fail independently of these changes (identical with capture changes stashed); full `dia test e2e` times out in `test_discover_stages` |
| 23 | Performance verification: AC15 (66×42 < 0.25ms, 128×128 < 1ms, draw calls, memory) | AC15 criteria met | Skipped | sonnet | Profile drain + draw timing, log draw call count, measure geometry memory, assert limits **Skipped by the user (2026-10-20).** AC15 is NOT verified: no timings, memory or draw-call measurements were recorded. Partial work from the stopped subagent (CellFieldPerfTestStage, TestCellFieldPerf.cpp, perf E2E scenario, small renderer/registry stat edits) is parked in `git stash` entry 'CellField task 23 (perf) partial work, killed by user' — unreviewed, unbuilt |
| 24 | Observation scan: check all touched files for missed DIA_LOG/TRACE/PROFILE/Metric/Health opportunities | Findings reported | Superseded by 25 | sonnet | Scan CellFieldRegistry, CellFieldRenderer, HexFlatTopOddQBuilder, CellFieldGeometryCache; report gaps (not blocking Done) Task 25 implements the instrumentation; the findings so far are in rows 8–17 notes |
| 25 | Implement the observation scan (do the work, not just report): register `MetricRegistry` counters for every CellField stat (`cellfield.uploads`, `.uploaded_bytes`, `.geometry_builds`, `.dropped_commands`, `.coalesced_commands`, `.stale_handles`, `.unknown_colour_maps`, `.out_of_range_cells`, `.unbuilt_layer_requests`, `.rejected_requests`, `.draw_calls`, queue depth/bytes, live layers, resident textures/geometry bytes), `DIA_PROFILE_SCOPE` on `Drain`/`Draw`/`BuildGeometry`/`SubmitLayer`, `DIA_TRACE_ZONE` on drain and draw, log lines for every state transition still missing (layer created/destroyed, palette/ramp registered or replaced, geometry built/released with size, shutdown summary), and an `IHealthReporter` for the renderer/registry (queue full, drops, unbuilt layers, shader program missing) registered by Canvas/KernelModule. Resolve the open items recorded in earlier rows: `DIA_PROFILE_SCOPE` in DiaGraphics (no dependency on DiaObservation today for the builder), the `kDiaBgfx` profile category, metrics wiring through `Canvas::GetCellFieldRendererStats` and `KernelModule` | Tests for each metric/health state (value after a scripted sequence; health transitions Healthy → Degraded → Healthy); zero overhead when disabled where the macros allow | Done | sonnet | Follows Task 24's findings; do not start until Task 22 is settled so it does not collide with render fixes. Needs sign-off if a new Dia module dependency is required (AD-001 style): flag, do not add silently RED compile failure; GREEN `CellField*` 143 (8 new `CellFieldObservation`); cluichetest/cow build-only pass; manifest 73 ok; `dia check deps` 0 missing. 16 `cellfield.*` metrics (`CellFieldObservation.h`), health reporter `DiaBgfx.CellField` (Failing = program missing; Degraded = dropped commands / unbuilt layers / unknown colour maps; OK after 120 clear frames), profile scopes, trace zones, rate-limited lifecycle logs (16 per drain cycle). No new module dependency. `kDiaGraphics` category used (no `kDiaBgfx`; enum is a fixed bitmask). `cellfield.geometry_builds` = registry Create count, not the cache's GPU builds. Stale-handle and unbuilt-request events counted but not logged (per-frame spam risk) |
| 26 | Exhaustive test-gap audit and fixing for every CellField file (DiaCore request types, FrameData hook, registry, geometry builder, geometry cache, renderer, draw params, Ortho2D, view order, Canvas wiring, KernelModule glue): list every public function, branch and AC (1–15) against the existing tests, write the missing tests (boundary values, error paths, NaN/Inf, empty/1-cell/max grids, odd widths for odd-q, handle generation wrap at 65536 recycles, slot reuse, cap exactly at 8 MB, palette/ramp edge counts, thread interleavings, shutdown idempotence, double Shutdown, draw before drain, Canvas without registry), and FIX every bug they expose in the code (RED-GREEN per bug, one commit per fix). Follow `/gen-tests` conventions and `.claude/skills/tdd.md`. Include a coverage report if `dia` offers one (check `dia --help` for coverage) and a table mapping AC1–AC15 to test names, noting honestly which ACs are only verified by the Task 22 image stage or are unverifiable (TSan, Vulkan) | `dia run googletest --filter="CellField*"` all pass; AC→test matrix checked in as a section of this plan | Not Started | sonnet | Run after Tasks 22 and 25 so tests cover the final code; dispatch per file group, not one giant agent. Any bug found gets its own RED test first |

## Step 2 (Later)

Step 2 tasks (after S-1's view works, before A4 needs them):
- **Smooth shading:** per-layer `shading` field (Flat/Smooth), shader uses four-cell weights for Smooth, category layers stay Flat.
- **Layer stacking and opacity:** raise cap from 1 to 8 requests per frame, draw in array order with `opacity` alpha blending.
- **State blend:** keep two value textures per layer (previous/current), swap on `Update`, shader interpolates values at `stateBlend` factor before colour lookup. Category layers ignore factor.
- **Partial updates:** `UpdateCells(handle, cellIndices[], values[], count)` uploads row-region writes via `bgfx::updateTexture2D`, untouched cells stay unchanged.
- **AC16–21:** tests for Step 2 behaviours, golden image with stacked layers, performance with 4 layers + state blend.

The Step 2 API fields (`shading`, `opacity`, `stateBlend`, `elevationLayer`) exist from Step 1 (defaulted and ignored), so Step 1 callers don't change when Step 2 ships. Setting them in Step 1 logs a Warning and is otherwise no-op (AC20 checks this).
