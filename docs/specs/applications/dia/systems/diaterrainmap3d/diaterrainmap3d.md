# System Spec: DiaTerrainMap3D

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** rendering, camera
**Research:** docs/research/tile_renderin_art_pipeline/summary.md (browser prototype: `docs/research/tile_renderin_art_pipeline/lookdev/`)
**First consumer:** CoW Strategic Map (@docs/specs/applications/cow/systems/strategicmap/strategicmap.md), proved by the V-1 testbed.

**Amended 2026-11-20 (user, 3-way split):** DiaTerrainMap3D is **3D rendering only**.
- The cell-field bake (data) moved to **DiaCellFieldBake** (@docs/specs/applications/dia/systems/diacellfieldbake/diacellfieldbake.md).
- Instanced sprites, particles and the post-process stack moved to **DiaBgfx3D** as features (@docs/specs/applications/dia/systems/diabgfx3d/diabgfx3d.md), because any 3D game needs them.
- DiaTerrainMap3D keeps what is specific to an illustrated terrain map.

## Purpose

DiaTerrainMap3D draws a cell grid as an illustrated, tilted **3D** terrain map:
- **Geometry:** a heightfield built from DiaCellFieldBake's fields.
- **Material:** one terrain material carrying the whole style in code.
- **Inputs:** per-cell channels for condition, weather and events.
- **Props:** scattered from class rules.

**What a game supplies:**
- cell data, through the bake;
- a visual table: palettes, per-class parameters and prop rules;
- a small palette-masked sprite atlas.

No per-cell or per-state art is needed.

It exists because a game with no artist still needs a map that is beautiful and readable. Stamped tile atlases need hundreds of matching images and repeat visibly at zoom-out. A field-driven map needs neither, and every simulation channel shows through it (research 2026-11-20).

**Dependency chain:**
`DiaTerrainMap3D → DiaCellFieldBake (fields, owner, margin, river field), DiaBgfx3D (Canvas3D, MeshGpuCache, materials; instanced sprites, particles, post-process stack, render targets), DiaBgfx (BgfxTextureHandle, ShaderProgram), DiaCamera3D, DiaAttribute + DiaValueSchema (style and visuals parameters), DiaObservation, DiaCore`

## Prototype Reference

The approved prototype (`docs/research/tile_renderin_art_pipeline/lookdev/`, user sign-off 2026-11-20) is the visual reference for every feature. Its screenshots are in `docs/research/tile_renderin_art_pipeline/lookdev/shots/`.

| Prototype code | Feature |
|---|---|
| `app.js` `buildWorldGPU` (grid mesh, sea skirt), `GROUND_VS` | 1 Heightfield Terrain |
| `shaders.js` `COMMON` + `GROUND_FS` (palette, ink on fields, hatch, toon, form lines, contours, boil, condition grade, snow, wet, cloud shadow, fire, rivers, grain) | 2 Style Shader Library |
| `props.js` `placeProps`, `app.js` `buildClouds`, `alive.js` `Fauna.instances`, `weather.js` `Precip` spawn rules | 3 Map Props and Emitters |
| `alive.js` `PostFX` settings and the `app.js` UI and state | 4 Module, Presentation and Tooling |

## Responsibilities

**Heightfield terrain.**
- **Mesh:** chunked to fit 16-bit indices, built once per baked world. The vertex shader reads elevation from the baked field texture.
- **Sea skirt:** runs out to the horizon.
- **Uploads:** the baked fields go to GPU textures. The per-cell channel textures (condition, weather, events, water state) are refreshed on change.

**Terrain material (style shader library).** Every rule is driven by parameters:
- palette per class, with primary/secondary/accent allocation by noise, blended across seasons by weight;
- ink on linear-filtered fields at constant pixel width: coast, lake shore and class borders (from the margin field), with weight by zone;
- hatching by zone coverage, with zoom-aware spacing;
- toon hillshade, form lines and optional contours;
- line boil on terrain ink only;
- condition grade (saturation bands plus a bare-ground blend);
- snow by temperature and elevation;
- wet ground, cloud shadows from the weather channel, event overlays (burn front, scar), ground marks and paper grain.

**Rivers.** The band is drawn from the baked river field, with:
- inked banks and animated flow;
- width from a per-cell flow state;
- flood and frozen states.

**Map props and emitters.** This is map-specific use of DiaBgfx3D's generic instancing and particles.
- **Deterministic scatter:** props are placed from per-class density rules, scaled by condition, and checked against `OwnerAt` so props never stand on water.
- **Dynamic sprite layers:** clouds and fauna, with instances supplied by the game.
- **Particle density:** per-cell precipitation and smoke channels map to particle density.

**Map presentation.** Configure DiaBgfx3D's post-process stack for the map:
- the focus band;
- haze and sky colours by season;
- silhouette ink.

**Module, tooling and observability.**
- `TerrainMap3DModule` on the render side.
- Per-channel data views and a toggle for every layer.
- Style parameters bound to uniforms.
- Headless capture to a PNG.
- Logs, profile scopes and metrics: upload time, prop count, frame cost.

## Non-Responsibilities

- **Turning cell layers into fields.** This is DiaCellFieldBake.
- **Generic instanced sprites, particles and post-processing.** These are DiaBgfx3D.
- **Simulation of any kind.** Weather, ecology, fire, water and fauna come from the game as channels.
- **What classes mean.** Biomes, palettes and prop sets are game data.
- **Gameplay overlays, icons and selection.**
  - These belong to the game and to DiaCellLayers.
  - DiaTerrainMap3D exposes world-to-cell and screen-to-world helpers only.
- **2D maps.** The DiaBgfx `CellFieldRenderer` stays the 2D debug and overlay path.
- **Art production.**

## Design

### Data flow

```
Game (SimPU)                     Render side
CellLayers / snapshots ──FrameStream──▶ TerrainMap3DModule
                                          ├─ DiaCellFieldBake (on world change / class change)
                                          ├─ upload fields + per-cell channels
                                          ├─ terrain chunks + sea skirt (terrain material)
                                          ├─ props / dynamic sprites ─▶ DiaBgfx3D instancing
                                          ├─ precipitation / smoke density ─▶ DiaBgfx3D particles
                                          └─ map post preset ─▶ DiaBgfx3D post-process stack
```

### Public interfaces (sketch; finalised per feature)

```cpp
namespace Dia::TerrainMap3D {
    enum class Channel : uint8_t { Condition, Weather, Events, WaterState };
    class TerrainMap {
    public:
        bool SetWorld(const CellFieldBake::BakedFields&, const TerrainMapVisuals&);
        void UpdateChannel(Channel, const float* rgbaPerCell);
        void ScatterProps(const PropRules&, const float* conditionPerCell);
        void SetDynamicSprites(const Bgfx3D::SpriteInstance*, uint32_t count, SpriteLayer);
        void Draw(const Camera3D&, const TerrainMapStyle&, float timeSeconds);
        bool Capture(const char* pngPath);
    };
}
```

`TerrainMapVisuals` (class palettes, ground marks, prop rules) and `TerrainMapStyle` (every tunable plus toggles) are DiaAttribute parameter files with DiaValueSchema sidecars.

## Features

| # | Feature | Summary | Spec | Status |
|---|---|---|---|---|
| 1 | Heightfield Terrain | Chunked mesh from baked fields, sea skirt, field/channel textures, terrain material host | TBD | Draft |
| 2 | Style Shader Library | Terrain material functions (palette, ink-on-fields, hatch, toon, form lines, contours, boil, condition grade, snow, wet, cloud shadow, events, marks, rivers, grain); game-side shader root in `pipeline.toml` | TBD | Draft |
| 3 | Map Props and Emitters | Deterministic class-rule scatter scaled by condition, dynamic sprite layers, per-cell density into particles, sway rules (no prop boil) | TBD | Draft |
| 4 | Module, Presentation and Tooling | `TerrainMap3DModule`, map post preset, data views, toggles, style binding, headless capture, observability | TBD | Draft |

Build order: DiaCellFieldBake 1 → 1 → 2 → 4 (first visible map). Then 3, once DiaBgfx3D instancing and particles exist. The post preset in 4 needs the DiaBgfx3D post-process stack.

## Dependencies

- **DiaCellFieldBake (all features):** fields, owner, margin, river field, `OwnerAt`.
- **DiaBgfx3D:**
  - Canvas3D, MeshGpuCache and MaterialRegistry (built);
  - planned features: render targets, instanced sprites, particles, post-process stack, dynamic texture update.
- **DiaBgfx:** `BgfxTextureHandle` (gains `Update`), `ShaderProgram`, the shader cook.
- **DiaCamera3D:** Camera3D plus Orbit, SmoothDamp and BoundsClamp.
- **DiaAttribute / DiaValueSchema:** the parameter files.
- **DiaObservation.**

## Decisions

| ID | Decision | Status | Binding |
|---|---|---|---|
| TMAP-001 | New Dia system `DiaTerrainMap3D` for the illustrated 3D terrain map; CoW supplies data only (user 2026-11-20) | Accepted | Yes |
| TMAP-002 | The map is a 3D heightfield drawn from cell fields, not stamped tiles or per-state tile images (bake-off 2026-11-20) | Accepted | Yes |
| TMAP-003 | The style lives in shader code and parameter files; no feature may require per-cell or per-state images | Accepted | Yes |
| TMAP-004 | Mountains are terrain relief, not sprites; peak sprites optional and off by default | Accepted | Yes |
| TMAP-005 | Weather, condition and events arrive as per-cell channels; no global weather state | Accepted | Yes |
| TMAP-006 | No line boil or frequency-varying sway on props (read as vibration, user 2026-11-20) | Accepted | Yes |
| TMAP-007 | Superseded by CFB-002 (moved to DiaCellFieldBake) | Superseded | — |
| TMAP-008 | 3-way split: bake → DiaCellFieldBake (data), generic sprites/particles/post → DiaBgfx3D, DiaTerrainMap3D = 3D terrain-map rendering only (user 2026-11-20) | Accepted | Yes |

## Inherited Binding Decisions

| ID | Decision | How it constrains DiaTerrainMap3D |
|---|---|---|
| PD-001 | StringCRC for IDs | Class, channel, prop and parameter ids |
| PD-002 / AD-004 | ProcessingUnit/Phase/Module | `TerrainMap3DModule` on the render side; sim state only via FrameStream snapshots |
| PD-004 / AD-002 | No STL in public APIs | Pointer + count / `DynamicArrayC` |
| PD-005 | x64 / D3D11 | bgfx D3D11 path |
| AD-001 | Module docs; engine changes signed off | New module doc; DiaBgfx and DiaBgfx3D additions are signed off at their feature specs |
| AD-003 | `Dia::<Module>::` | `Dia::TerrainMap3D::` |
| CFB-001..003 | DiaCellFieldBake | Fields come only from the bake; no nearest-cell class lookups in the renderer |

## Test Strategy

- **GoogleTests:**
  - chunking is within index limits and seamless;
  - scatter is deterministic per seed;
  - prop density follows condition bands;
  - no prop lands on water (`OwnerAt`);
  - channel upload round-trips.
- **Visual:** one headless capture per feature at a fixed world, camera and style, judged by the user. This is a taste check, not a pixel gate.
- **Performance (Debug, ballpark):** frame time at Large with everything on. The target is 30 fps, from GDD V-1 Test 7.

## Open Design Questions

1. **Hex readability.** Is cell highlighting a DiaTerrainMap3D outline drawn from the owner texture, or a DiaCellLayers overlay?
2. **Channel update cadence.** Should the weather channel update every frame (smoothed A4 blend computed CPU-side) or upload two snapshots and blend on the GPU by day?

## Status

`Approved` (2026-11-20, user), amended the same day for the 3-way split (TMAP-008). Built at CoW build order #18. No AD-001 sign-offs yet.
