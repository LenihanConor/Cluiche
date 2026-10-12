# Feature Spec: Layered Debug Rendering

**Parent:** @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md

**Gameplay Domains:** simulation

## Summary

Layered Debug Rendering provides debug-grade visualization of generated worlds to enable fast visual inspection during CoW's testbed iteration workflow. It renders biome-colored tiles (base layer, always on) plus toggleable overlays (Sites, Regions, Dynasty Starts, Design Reference reading A1 biome-affinity data), preventing visual clutter when debugging specific concerns. This is not V-1's real tile art — it's functional colored squares and site markers sufficient for eyeballing "does this placement look broken?" without leaving the game.

GDD decision 6 chose to include visual rendering despite S-1 being a "data pipeline testbed" because fast iteration requires seeing what you're iterating on.

## Problem

Without debug rendering:
- **Blind iteration** — Designers cannot see biome distribution, site placements, or region boundaries without exporting to JSON and manually parsing (too slow for iteration)
- **Forced context-switching** — "Export → switch to S-3 → import → inspect" workflow takes minutes, breaking flow (GDD decision 6 rationale)
- **No placement sanity checks** — Cannot eyeball "are Hearth Sites well-distributed?" or "did dynasty starts land too close to edges?" without inspection tools
- **Feature 1-3 opaque** — Generation, validation, retry all work but produce invisible output (only logs and JSON files)

With debug rendering:
- **Instant feedback** — Click Regenerate → see biome grid + site markers in <1 second (meets GDD §7's <5-second feedback target)
- **Layered inspection** — Toggle Sites overlay to inspect Hearth/Nexus/world-throne placement; toggle Dynasty Starts to check spacing; toggle Design Reference to compare biome distribution against A1 ecology data
- **Visual placement validation** — Spot obvious failures (Hearth on isolated island, dynasty starts clustered in corner) before running Feature 2's reachability validator

## Goals

1. **Base layer (always on):** Biome-colored tiles — each cell rendered as colored square (color per biome type), grid overlay optional
2. **Toggleable overlay: Sites** — Markers for Hearth Sites (circles), Nexus Nodes (triangles), world-thrones (crowns), each with distinct color/shape
3. **Toggleable overlay: Regions** — Region boundary lines or fill overlays (e.g., Ancient Interiors = red outline, Verdant Lowlands = green outline)
4. **Toggleable overlay: Dynasty Starts** — Player start (large blue marker), rival starts (large red markers), with distance circles showing minimum spacing
5. **Toggleable overlay: Design Reference** — Per-cell text or tooltip showing A1 species biome-affinity data (e.g., "Forest → deer (preferred), elk (preferred), wolf (tolerated)")
6. **Fast rendering** — Renders typical world (128×128 grid = 16k cells) in <1 second, no frame drops during pan/zoom
7. **Toggles in UI** — Checkboxes in F-1 debug host panel (Feature 6) to enable/disable each overlay independently

## Non-Goals

1. **V-1 real tile art** — Debug-grade only (solid-color squares, simple shapes); V-1 (Map Visuals testbed) owns sprite-based tile rendering, camera controls, fog of war
2. **Animation** — Static rendering only (no animated water, waving grass, etc.)
3. **Lighting / shaders** — Flat unlit rendering (no shadows, no bloom, no post-processing)
4. **Terrain elevation** — 2D grid only (no 3D heightmap visualization)
5. **Live ecology state** — Renders world structure (biomes, sites) only; A1 ecology runtime state (species populations, resource levels) is S-2/S-3's job
6. **Camera persistence** — Pan/zoom state resets on regeneration (future enhancement if designers request it)

## Architecture

### Rendering Layers

**Layer stack (bottom to top):**

1. **Base: Biome Tiles** (always rendered)
   - One colored quad per cell
   - Color palette: arbitrary debug colors per biome type (e.g., Forest = green, Mountain = gray, Desert = yellow)
   - Grid lines: optional toggle (1px black lines between cells)

2. **Overlay 1: Regions** (toggle on/off)
   - Region boundary rendering: outline mode (draw lines along region edges) or fill mode (semi-transparent overlay)
   - Color per region (e.g., Ancient Interiors = red, Verdant Lowlands = green, per Phase 2 region types)

3. **Overlay 2: Sites** (toggle on/off)
   - Hearth Sites: small circles (orange)
   - Nexus Nodes: small triangles (blue)
   - World-Thrones: small crowns or stars (gold)
   - Each site rendered at cell center (grid position → screen position)

4. **Overlay 3: Dynasty Starts** (toggle on/off)
   - Player start: large circle (blue, 2x cell size)
   - Rival starts: large circles (red, 2x cell size)
   - Minimum spacing circles: dashed circle around each start (radius = `ValidationParams.minimumDynastySpacing`)

5. **Overlay 4: Design Reference** (toggle on/off)
   - Per-cell text or tooltip (on hover): reads A1 species biome-affinity data, displays "Biome X → species list (preferred/tolerated)"
   - Tooltip mode preferred (avoids text clutter on 128×128 grid = 16k cells with text)

**Toggle state:** Stored in S-1 stage's UI state (Feature 6 checkboxes write to `bool mShowSites`, `bool mShowRegions`, etc.; rendering reads these flags).

### Rendering Pipeline

**Input:** `WorldStructure` (from Feature 1 or Feature 4 Import)

**Output:** Rendered frame to screen (via DiaGraphics / DiaBgfx)

**Render loop (once per frame):**

```cpp
void S1DebugRenderer::Render(const WorldStructure& world, const Camera2D& camera) {
    // Always render base layer
    RenderBiomeTiles(world, camera);
    
    // Render enabled overlays
    if (mShowRegions) RenderRegionOverlay(world, camera);
    if (mShowSites) RenderSiteOverlay(world, camera);
    if (mShowDynastyStarts) RenderDynastyStartOverlay(world, camera);
    if (mShowDesignReference) RenderDesignReferenceOverlay(world, camera);
}
```

**Biome tile rendering (batched):**

```cpp
void RenderBiomeTiles(const WorldStructure& world, const Camera2D& camera) {
    for (uint32_t y = 0; y < world.dimensions.height; ++y) {
        for (uint32_t x = 0; x < world.dimensions.width; ++x) {
            const Cell& cell = world.cells[y * world.dimensions.width + x];
            Color color = BiomeToDebugColor(cell.biome);
            
            // Render quad at (x, y) with biome color
            DrawQuad(camera.WorldToScreen({x, y}), CELL_SIZE, color);
        }
    }
}
```

**Performance optimization:** Batch all quads into single draw call (instanced rendering or dynamic vertex buffer); do not issue 16k individual draw calls for 128×128 grid.

**Site rendering (unbatched, low count):**

```cpp
void RenderSiteOverlay(const WorldStructure& world, const Camera2D& camera) {
    // Hearth Sites
    for (const auto& hearth : world.hearths) {
        DrawCircle(camera.WorldToScreen(hearth.position), SITE_MARKER_SIZE, Color::Orange);
    }
    
    // Nexus Nodes
    for (const auto& nexus : world.nexuses) {
        DrawTriangle(camera.WorldToScreen(nexus.position), SITE_MARKER_SIZE, Color::Blue);
    }
    
    // World-Thrones
    for (const auto& throne : world.thrones) {
        DrawCrown(camera.WorldToScreen(throne.position), SITE_MARKER_SIZE, Color::Gold);
    }
}
```

**Design Reference overlay (tooltip mode):**

```cpp
void RenderDesignReferenceOverlay(const WorldStructure& world, const Camera2D& camera) {
    // On hover: get cell under mouse cursor
    GridCoord hoveredCell = camera.ScreenToWorld(GetMousePosition());
    if (!IsValidCell(hoveredCell, world.dimensions)) return;
    
    const Cell& cell = world.cells[hoveredCell.y * world.dimensions.width + hoveredCell.x];
    
    // Load A1 biome-affinity data (if externalized)
    std::string affinityText = LoadBiomeAffinityText(cell.biome);  // e.g., "Forest → deer (preferred), elk (preferred)"
    
    // Render tooltip near cursor
    DrawTooltip(GetMousePosition(), affinityText);
}
```

### Camera Controls

**Minimal camera for S-1:** Pan (WASD or arrow keys), zoom (mouse wheel), reset (Home key).

**Camera state:** Position (center of view), zoom level (pixels per cell).

**No persistence:** Camera resets on regeneration (future enhancement if designers request it).

## Public Interfaces

### APIs

```cpp
// S1DebugRenderer.h (CoW/S1_WorldGeneration/)
namespace CoW::S1 {

class S1DebugRenderer {
public:
    // Render world with enabled overlays
    void Render(const WorldStructure& world, const Camera2D& camera);
    
    // Toggle overlay visibility
    void SetShowRegions(bool show);
    void SetShowSites(bool show);
    void SetShowDynastyStarts(bool show);
    void SetShowDesignReference(bool show);
    void SetShowGridLines(bool show);
    
    // Query overlay visibility (for Feature 6 checkboxes)
    bool GetShowRegions() const;
    bool GetShowSites() const;
    bool GetShowDynastyStarts() const;
    bool GetShowDesignReference() const;
    bool GetShowGridLines() const;

private:
    void RenderBiomeTiles(const WorldStructure& world, const Camera2D& camera);
    void RenderRegionOverlay(const WorldStructure& world, const Camera2D& camera);
    void RenderSiteOverlay(const WorldStructure& world, const Camera2D& camera);
    void RenderDynastyStartOverlay(const WorldStructure& world, const Camera2D& camera);
    void RenderDesignReferenceOverlay(const WorldStructure& world, const Camera2D& camera);
    
    bool mShowRegions = false;
    bool mShowSites = true;  // Default: Sites visible (most common debug use case)
    bool mShowDynastyStarts = false;
    bool mShowDesignReference = false;
    bool mShowGridLines = false;
};

} // namespace CoW::S1
```

**No events:** Rendering is passive (reads `WorldStructure`, writes to screen); no state changes published.

## Dependencies

### Upstream Dependencies

| System/Feature | Why needed | Status |
|----------------|-----------|---------|
| **Feature 1 (Placeholder Generation Algorithm)** | Produces `WorldStructure` to render | Draft (being specced) |
| **Feature 4 (JSON Export/Import)** | Imported worlds also rendered | Draft (being specced) |
| **Feature 6 (F-1 Debug Host Integration)** | Provides toggle checkboxes (UI writes to `S1DebugRenderer` overlay flags) | Draft (being specced) |
| **Phase 2 data design** | Biome enum (for debug color palette), Region enum (for region overlay colors), ApotheosisType enum (for throne marker shapes) | GDD in-progress (types TBD — placeholder uses arbitrary colors/shapes) |
| **A1 species biome-affinity data** | Optional for Design Reference overlay | GDD §2 says "optional, can cut if not externalized" — non-blocking |

### Dia Engine Dependencies

- **DiaGraphics** (`ICanvas` or DiaBgfx for quad/circle/line rendering)
- **DiaMaths** (`Vector2`, `Camera2D` for screen-to-world transforms, bounds checks)
- **DiaInput** (keyboard for camera pan, mouse wheel for zoom, mouse position for Design Reference tooltip)
- **DiaCore** (containers, file I/O if loading A1 biome-affinity data)

## Acceptance Criteria

### AC1 — Base Layer Rendering
Generate world with known seed (hand-crafted or empirical). Render S-1 stage. **Pass:** Screen shows 128×128 grid of colored tiles, colors correspond to biome types (visual inspection: "mostly green" if Forest-heavy, "mostly gray" if Mountain-heavy per `GenParams` weights).

### AC2 — Sites Overlay Toggle
Generate world, toggle Sites overlay on. **Pass:** Hearth/Nexus/world-throne markers visible on screen (circles/triangles/crowns at site positions). Toggle Sites off. **Pass:** Markers disappear, only biome tiles remain. Proves toggle works.

### AC3 — Regions Overlay Toggle
Generate world with multiple regions (per Feature 1's region assignment). Toggle Regions overlay on. **Pass:** Region boundaries visible (outline or fill mode), distinct colors per region. Toggle off. **Pass:** Boundaries disappear. Proves toggle works.

### AC4 — Dynasty Starts Overlay Toggle
Generate world with player + rival starts. Toggle Dynasty Starts overlay on. **Pass:** Blue circle (player) and red circles (rivals) visible, plus dashed minimum-spacing circles around each. Toggle off. **Pass:** Markers disappear. Proves toggle works.

### AC5 — Design Reference Overlay (Tooltip Mode)
Generate world. Toggle Design Reference overlay on. Hover mouse over cell. **Pass:** Tooltip appears showing biome name + species affinity data (e.g., "Forest → deer (preferred), elk (preferred)"). Move mouse to different cell. **Pass:** Tooltip updates to new cell's data. **If A1 data not externalized:** Skip this AC (GDD §2 says "can cut if not externalized").

### AC6 — Grid Lines Toggle
Toggle Grid Lines on. **Pass:** 1px black lines visible between cells (grid visible). Toggle off. **Pass:** Grid lines disappear (seamless tile rendering). Proves grid toggle works.

### AC7 — Camera Pan
Render world, press WASD or arrow keys. **Pass:** Camera pans (view moves, showing different part of world grid). Pan to edge of world. **Pass:** Camera stops at world bounds (does not pan into void). Proves pan works.

### AC8 — Camera Zoom
Render world, scroll mouse wheel up. **Pass:** Camera zooms in (cells appear larger, fewer cells visible on screen). Scroll down. **Pass:** Camera zooms out (cells appear smaller, more cells visible). Proves zoom works.

### AC9 — Camera Reset
Pan and zoom camera, press Home key. **Pass:** Camera resets to default position/zoom (centered on world, full world visible or reasonable default zoom). Proves reset works.

### AC10 — Rendering Performance
Generate world with 128×128 grid (16k cells), render with all overlays enabled, measure frame time. **Target: <16ms per frame (60 FPS)** for typical world sizes. Proves rendering fast enough for iteration workflow (no frame drops during pan/zoom).

### AC11 — Toggle Persistence
Toggle Sites off, Regions on, Dynasty Starts on. Click Regenerate (Feature 6). **Pass:** After regeneration, Sites still off, Regions still on, Dynasty Starts still on (toggle state persists across regenerations). Proves toggles not reset by new world load.

## Inherited Binding Decisions

### From Platform (Cluiche)

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-002 | ProcessingUnit/Phase/Module architecture | `S1DebugRenderer` runs on MainPU (rendering is main-thread work); reads `WorldStructure` passed from SimPU (via event or shared state) |
| PD-004 | No STL containers in public APIs | `S1DebugRenderer` API uses bool flags (no containers); internally can use `DynamicArrayC` for batched rendering if needed |

### From Application (CoW)

| ID | Decision | How it applies |
|----|----------|----------------|
| AD-002 | Classify cross-cutting needs | `S1DebugRenderer` is **Feature-local** initially (lives in S-1 stage); if S-2/S-3 reuse debug-grade tile rendering, extract base layer to **CoW-Shared** utility. Not Dia (too CoW-specific: uses CoW's biome/site types), not Baseline (no other game needs CoW's debug rendering) |
| AD-003 | GDD owns design intent | GDD defines "debug-grade rendering for fast iteration" (decision 6), layered toggles (decision 8), and <5-second feedback target (§7). This spec defines rendering implementation (color palette, overlay shapes, batching strategy, camera controls) |

### From System (WorldGenerationPipeline)

| ID | Decision | How it applies |
|----|----------|----------------|
| N/A | No system-level binding decisions in parent spec | System spec has Open Design Questions but no SD- binding decisions |

## Open Design Questions

| # | Question | Impact | Resolution |
|---|----------|--------|------------|
| 1 | What are the exact biome debug colors (Forest = green, Mountain = gray, etc.)? | Affects visual clarity (need distinct colors for ~8-10 biomes per Phase 2); does not block implementation (use placeholder colors, update when Phase 2 finalizes) | Use placeholder color palette (HSV-distributed for distinctness); update to thematic colors (Forest = green, Desert = yellow, etc.) when Phase 2 biome names finalized |
| 2 | Should region overlay use outline mode or fill mode (semi-transparent overlay)? | Outline mode is cleaner but harder to see small regions; fill mode is more visible but obscures biome colors | Start with outline mode (1-2px colored lines along region edges); add fill mode as optional toggle if designers request it |
| 3 | Should Design Reference overlay be tooltip (hover) or always-on text (per cell)? | Tooltip is cleaner but requires hovering each cell; always-on text is faster to scan but clutters screen (unreadable at 128×128 = 16k cells) | Tooltip mode (per AC5); always-on text mode infeasible for typical world sizes |
| 4 | Should camera state persist across regenerations (remember pan/zoom between worlds)? | Convenience vs. predictability (persisted camera might be off-screen for smaller worlds); manual reset is safer | No persistence (camera resets on regeneration); add persistence if designers request it during S-1 challenge pass |

## Status

`Approved` — 2026-10-11

---

## Implementation Notes

When implementation begins (after approval), create a plan file `layered-debug-rendering.plan.md` alongside this spec. The plan must include an **Implementation Patterns** section documenting:

- Biome debug color palette (enum → Color mapping, distinctness validation)
- Batched tile rendering (instanced rendering or dynamic vertex buffer, draw call count target)
- Site marker shapes (circle/triangle/crown implementation, size constants)
- Camera transform (screen-to-world and world-to-screen math, zoom clamping, pan bounds)
- Toggle state management (where flags stored, how Feature 6 checkboxes update flags, how rendering reads flags)
- Design Reference data loading (if A1 biome-affinity data externalized: file format, parse logic, cache strategy)

These patterns are validated against actual implementation before tasks are marked Done.

**Dependency note:** If A1 species biome-affinity data not externalized by Feature 7 implementation, cut Design Reference overlay (AC5) per GDD §2's "optional, can cut if not externalized" clause. Re-add later when data available.
