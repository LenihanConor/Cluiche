# Implementation Plan: Coord2D Debug Overlay

**Spec:** @docs/specs/features/dia/diavisualdebugger/coord2d-debug-overlay.md
**Status:** Done

---

## Implementation Patterns

### Phase 1: Camera Infrastructure (DiaGraphics)

**Camera2D** — Header-only value type in `Dia::Graphics` namespace. Position (`Vector2D`), zoom (`float`, default 1.0), rotation (`float`, default 0.0 degrees). No STL. Trivially copyable (PD-004, SD-DBG-009 alignment).

**ViewportTransform** — Constructed from `Camera2D` + `Vector2D windowSize`. Internally computes a 3x3 affine matrix (translate by -position, rotate, scale by 1/zoom, then translate to window center). Exposes:
- `ScreenToWorld(Vector2D pixel) -> Vector2D`
- `WorldToScreen(Vector2D world) -> Vector2D`
- `GetWorldBounds() -> AARect` (min/max corners of visible world)

Matrix math uses `DiaMaths` types. No heap allocation.

### Phase 2: Renderer Integration (DiaBgfx)

**DebugRenderer::Draw** and **SpriteRenderer::Draw** currently build a fixed ortho: `bx::mtxOrtho(ortho, 0, cw, ch, 0, ...)`. With Camera2D:
- Compute view matrix from Camera2D (translate by -position, rotate by -rotation)
- Compute projection as `bx::mtxOrtho(-cw/(2*zoom), cw/(2*zoom), ch/(2*zoom), -ch/(2*zoom), ...)`
- Pass view matrix as first arg to `bgfx::setViewTransform(mViewId, view, proj)`

Camera2D is carried on `FrameData` (new field, since FrameData crosses the PU boundary via FrameStream).

### Phase 3: DebugLayerManager Viewport API

Add to `DebugLayerManager`:
```cpp
void SetViewport(const Camera2D& camera, const Vector2D& windowSize);
const ViewportTransform& GetViewportTransform() const;
```
Stored as a member. Updated once per frame by application code before `Draw()`. Overlay layers read it via the manager reference they hold.

### Phase 4: Overlay Layers (DiaVisualDebugger)

All drawers follow the `PhysicsShapesDrawer` pattern:
- `#ifdef DIA_DEBUG` guarded
- Constructor takes `const DebugLayerManager&` reference
- `GetLayerName()` returns the canonical constant from `DebugLayerNames.h`
- `Draw(FrameData&)` reads `mManager.GetViewportTransform()` for world bounds
- Uses `DebugColourPalette` colours exclusively
- Registered with stage tag `"coord2d"` and priority 50+

Layer files live in `Dia/DiaVisualDebugger/Coord2D/`.

### Phase 5: Console Tab

Coord2D layers register with stage tag `StringCRC("coord2d")`. The existing console's `RenderStageTabs()` picks this up automatically — no console code change needed. The tab label is the stage tag string `"coord2d"` rendered by `tag.AsChar()`.

Wait — the spec says "Coord2D" tab. The console renders `tag.AsChar()` directly. So the stage tag string should be `"Coord2D"` (matching desired display). Layer names still use lowercase prefix `"coord2d.*"` per convention.

---

## Task Table

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Camera2D class | Compiles; used in T13 | Done | haiku | Header-only value type: `DiaGraphics/Camera/Camera2D.h` |
| 2 | ViewportTransform class | Compiles; used in T13 | Done | sonnet | `DiaGraphics/Camera/ViewportTransform.h/.cpp` — matrix math, ScreenToWorld/WorldToScreen/GetWorldBounds |
| 3 | FrameData carries Camera2D | Builds | Done | haiku | Added `Camera2D mCamera`, `Vector2D mWindowSize`, `Vector2D mMousePixel`; Clear/Copy updated |
| 4 | DiaBgfx renderer integration | Visual: panning camera moves debug draws | Done | sonnet | DebugRenderer + SpriteRenderer SetCamera; ortho centred on camera position + zoom |
| 5 | DebugLayerManager SetViewport | Builds | Done | haiku | `SetViewport(Camera2D, Vector2D)` + `GetViewportTransform()` added |
| 6 | Layer name constants | Builds | Done | haiku | `kCoord2D*` constants + `kCoord2DStageTag` added to DebugLayerNames.h |
| 7 | Origin layer | Visual: crosshair at 0,0 | Done | sonnet | `Coord2D/Coord2DOriginDrawer.h/.cpp` |
| 8 | Axes layer | Visual: red X, green Y spanning viewport | Done | sonnet | `Coord2D/Coord2DAxesDrawer.h/.cpp` |
| 9 | Grid layer | Visual: grid lines + labels | Done | sonnet | `Coord2D/Coord2DGridDrawer.h/.cpp` |
| 10 | Bounds layer | Visual: corner labels | Done | sonnet | `Coord2D/Coord2DBoundsDrawer.h/.cpp` |
| 11 | Cursor layer | Visual: text at mouse position | Done | sonnet | `Coord2D/Coord2DCursorDrawer.h/.cpp`; mouse pixel via FrameData |
| 12 | vcxproj updates | Solution builds | Done | haiku | DiaGraphics.vcxproj + filters; DiaVisualDebugger.vcxproj + filters; GoogleTests.vcxproj + filters |
| 13 | Google Tests | 31/31 pass | Done | sonnet | `Tests/GoogleTests/Graphics/TestCamera2D.cpp` — 9 suites, 31 tests |
