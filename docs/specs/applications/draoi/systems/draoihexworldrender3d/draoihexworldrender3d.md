# System Spec: DraoiHexWorldRender3D

## Parent Application
@docs/specs/applications/draoi/draoi.md

**Gameplay Domains:** rendering
**Research:** docs/research/tile_renderin_art_pipeline/summary.md (browser prototype: `docs/research/tile_renderin_art_pipeline/lookdev/`)
**First consumer:** CoW Strategic Map (@docs/specs/applications/cow/systems/strategicmap/strategicmap.md), proved by the V-1 testbed. **Credible second:** any hex strategy game that wants an illustrated world map without writing shaders.

**Layer split (2026-11-23, user).** This system was the Dia system DiaTerrainMap3D (Approved 2026-11-20). It is now split by layer:
- **Dia, generic techniques:** DiaToonShading (@docs/specs/applications/dia/systems/diatoonshading/diatoonshading.md) for ink, hatch, toon light and colour treatments; DiaBgfx3D Feature E (@docs/specs/applications/dia/systems/diabgfx3d/heightfield-mesh.md) for the chunked heightfield; DiaCellFieldBake for cell-to-field baking; DiaBgfx3D A–D for targets, sprites, particles and post.
- **Draoi, hex-world vocabulary (this system):** what a hex map can show (coast, lake, rivers, strata, snow, water, weather, events, condition, props by class), the material composed from Dia techniques, the standard and custom channels, and a neutral default style.
- **CoW, the look:** palettes, GDD numbers, season names, the sim-to-channel adapter, CoW-only effects through custom slots, and all module, stream, command, camera and capture wiring (Strategic Map).

## Purpose

DraoiHexWorldRender3D draws a hex world as an illustrated, tilted 3D map from data. A game supplies:
- the hex world and its baked fields (DraoiHexWorld, DiaCellFieldBake, DraoiHexWorldRelief);
- per-cell channel values;
- a visual table (per-class palettes, marks, prop rules) and a style file (every number).

No per-cell or per-state art is needed. A game with no artist still gets a beautiful, readable map, and every simulation channel shows through it (research 2026-11-20).

It is a **plain library** (Draoi PD-012): no module, no streams, no commands, no PU affinity. The game's render-side module owns a `HexWorldMap` and calls it.

**Dependency chain:**
`DraoiHexWorldRender3D → DraoiHexWorld (grid, BiomeIndex, StratumIndex), DraoiHexWorldRelief (relief, river field), DiaCellFieldBake (fields, owner, margin, OwnerAt), DiaToonShading (techniques), DiaBgfx3D (A render targets and dynamic textures, B instanced sprites, C particles, D post stack, E heightfield mesh), DiaBgfx (BgfxTextureHandle, ShaderProgram), DiaCamera3D, DiaAttribute + DiaValueSchema, DiaObservation, DiaCore`

It does **not** depend on DraoiHexWorldWeather, Hydrology or Ecology. Channels arrive as plain per-cell arrays, so a game can feed them from any source.

## Prototype Reference

The approved prototype (`docs/research/tile_renderin_art_pipeline/lookdev/`, user sign-off 2026-11-20) is the visual reference. Screenshots are in `lookdev/shots/`.

| Prototype code | Feature |
|---|---|
| `app.js` `buildWorldGPU` (textures), `commonUniforms` | 1 Hex Terrain Surface |
| `shaders.js` `GROUND_FS` (composition: coast, borders, strata hatch, snow, water, rivers, weather, events, season, condition) | 2 Hex Terrain Material |
| `props.js` `placeProps`, `app.js` `buildClouds`, `alive.js` `Fauna.instances`, `weather.js` `Precip` | 3 Map Props and Emitters |
| `app.js` frame loop and draw order | 4 Map Assembly |

## Channels (hybrid model, user 2026-11-23)

**Standard channels** have fixed meaning and a fixed GPU layout. Each mirrors what a Draoi sim produces, so the set is not guesswork:

| Channel | Values per cell | Typical source |
|---|---|---|
| Condition | health, vegetation | DraoiHexWorldEcology |
| Weather | cloud, precipitation, snow fraction, gust | DraoiHexWorldWeather |
| Snow | snow pack, temperature | DraoiHexWorldWeather |
| Water | lake level, river flow state, flood, ice | DraoiHexWorldHydrology |
| Events | burn front, scar | DraoiHexWorldEcology (fire) |

**Custom overlay slots:** 4 per-cell values with no fixed meaning. The game's style file binds each slot to a DiaToonShading technique (tint, masked overlay, emissive, hatch) with its colour, threshold and falloff. This is how a game shows states Draoi does not name (for CoW, for example, magic snow as an emissive overlay) without Draoi learning the concept.

A game may also add to a standard channel (CoW can add magic snow into Snow if it should look like ordinary snow). The packing into textures is Feature 1's.

## Responsibilities

- **Surface (Feature 1):** upload baked fields, relief and river field, per-cell class data, standard channels and custom slots to GPU textures; host the DiaBgfx3D heightfield mesh and skirt.
- **Material (Feature 2):** compose DiaToonShading techniques into the hex terrain material: coast, lake shore and class-border ink weighted by stratum; hatching by stratum coverage; toon light and slope rock; snow; ocean, lake and ice; rivers by flow state; cloud shadow and wet ground; burn and scar; condition grade; season cross-fade; ground marks per class; custom slots; data views. Ships a neutral default style.
- **Props and emitters (Feature 3):** deterministic class-rule scatter scaled by condition, dynamic sprite layers, channel-to-particle density, fixed-frequency sway.
- **Assembly (Feature 4):** `HexWorldMap`, the library facade: owns the pieces, versioned channel uploads, draw order, data-view switch, picking helpers, bake-state query, logs, profile scopes and metrics.

## Non-Responsibilities

- **Generic techniques, meshes, sprites, particles, post.** These are Dia (DiaToonShading, DiaBgfx3D).
- **The look's numbers.** Palettes, season tints and names, strata coverage and ink weights, condition bands: these are game data (CoW Map Visual Data). Draoi ships neutral defaults only.
- **Modules, streams, commands, presets, capture commands, camera, input, selection routing.** These belong to the game (CoW Strategic Map Feature 4 and 5). Draoi packages are plain libraries.
- **Simulation.** Channels arrive as data.
- **Gameplay overlays, icons, markers and regions.** Game and DiaCellLayers.
- **2D maps.** The DiaBgfx `CellFieldRenderer` stays the 2D path. A `DraoiHexWorldRender2D` would be a separate package.

## Design

### Data flow

```
Game render-side module (CoW StrategicMapModule)
  ├─ world change ─▶ DiaCellFieldBake + DraoiHexWorldRelief ─▶ HexWorldMap::SetWorld
  ├─ channel snapshot ─▶ HexWorldMap::UpdateChannel / UpdateCustomSlots (versioned)
  ├─ style + visual table (parameter files) ─▶ HexWorldMap::SetStyle / SetVisuals
  └─ per frame ─▶ HexWorldMap::Draw(camera, time)
                    ├─ DiaBgfx3D E heightfield + skirt, hex terrain material (DiaToonShading)
                    ├─ props / dynamic sprites ─▶ DiaBgfx3D B
                    ├─ precipitation / smoke ─▶ DiaBgfx3D C
                    └─ post settings ─▶ DiaBgfx3D D
```

### Public interface (sketch; finalised in Feature 4)

```cpp
namespace Draoi::HexWorldRender3D {
    enum class Channel : uint8_t { Condition, Weather, Snow, Water, Events };
    class HexWorldMap {
    public:
        bool SetWorld(const Dia::CellFieldBake::BakedWorld&, const HexWorldRelief::ReliefFields&, const HexWorldVisuals&);
        void UpdateChannel(Channel, const float* valuesPerCell, uint32_t version);
        void UpdateCustomSlots(const float* rgbaPerCell, uint32_t version);
        void SetStyle(const HexWorldStyle&);
        void Draw(const Dia::Camera3D&, float timeSeconds);
    };
}
```

`HexWorldVisuals` (per-class palettes, marks, prop rules) and `HexWorldStyle` (every tunable, embedding DiaToonShading parameter groups, plus custom slot bindings) are DiaAttribute parameter files with DiaValueSchema sidecars. CoW supplies both.

## Features

| # | Feature | Summary | Spec | Status |
|---|---|---|---|---|
| 1 | Hex Terrain Surface | Field, class, coast, relief and river textures from the bake; per-cell class data; standard channel and custom slot textures; hosts DiaBgfx3D E | [hex-terrain-surface.md](hex-terrain-surface.md) | Approved 2026-11-23 |
| 2 | Hex Terrain Material | Composition of DiaToonShading techniques with hex vocabulary; custom slots; data views; Draoi shader root; neutral default style | [hex-terrain-material.md](hex-terrain-material.md) | Approved 2026-11-24 |
| 3 | Map Props and Emitters | Deterministic class-rule scatter scaled by condition, dynamic sprite layers, per-cell density into particles, fixed-frequency sway (no prop boil) | [map-props-and-emitters.md](map-props-and-emitters.md) | Approved 2026-11-24 |
| 4 | Map Assembly | `HexWorldMap` facade, versioned uploads, draw order, data-view switch, `ScreenToWorld`/`WorldToCell`, bake-state query, observability | [map-assembly.md](map-assembly.md) | Approved 2026-11-24 |

Build order: DiaCellFieldBake 1 → DraoiHexWorldRelief → DiaBgfx3D A, E → DiaToonShading 1–4 → 1 → 2 → 4 (first visible map, hosted by CoW Strategic Map Feature 4). Then 3, once DiaBgfx3D B and C exist.

## Decisions

| ID | Decision | Status | Binding |
|---|---|---|---|
| TMAP-001 | Superseded by TMAP-009 (was: new Dia system for the 3D terrain map) | Superseded | — |
| TMAP-002 | The map is a 3D heightfield drawn from cell fields, not stamped tiles or per-state tile images (bake-off 2026-11-20) | Accepted | Yes |
| TMAP-003 | The style lives in shader code and parameter files; no feature may require per-cell or per-state images (also TOON-004) | Accepted | Yes |
| TMAP-004 | Mountains are terrain relief, not sprites; peak sprites optional and off by default | Accepted | Yes |
| TMAP-005 | Weather, condition and events arrive as per-cell channels; no global weather state | Accepted | Yes |
| TMAP-006 | No line boil or frequency-varying sway on props (read as vibration, user 2026-11-20) | Accepted | Yes |
| TMAP-007 | Superseded by CFB-002 (moved to DiaCellFieldBake) | Superseded | — |
| TMAP-008 | Superseded by TMAP-009 (was: 3-way split with the map as a Dia system) | Superseded | — |
| TMAP-009 | Layer split: generic techniques → Dia (DiaToonShading, DiaBgfx3D E), hex-world vocabulary and material composition → this Draoi package, the look and all wiring → CoW (user 2026-11-23) | Accepted | Yes |
| TMAP-010 | Hybrid channels: fixed standard channels that mirror Draoi sim outputs, plus 4 custom overlay slots bound to DiaToonShading techniques in the game's style file; no runtime shader generation (user 2026-11-23) | Accepted | Yes |
| TMAP-011 | Plain library: no module, streams, commands or PU affinity; the game's render-side module owns a `HexWorldMap` (Draoi PD-012) | Accepted | Yes |
| TMAP-012 | Draoi ships neutral defaults only; every GDD number lives in game data (user 2026-11-23) | Accepted | Yes |
| TMAP-013 | `HexWorldStyle` and the scalar fields of `HexWorldVisuals` are live-tweakable attributes (`AttributeSet`, TOON-006); the library is touched on the render thread only, and the game's module marshals edits from other PUs over a stream (user 2026-11-24) | Accepted | Yes |

## Inherited Binding Decisions

| ID | Decision | How it constrains DraoiHexWorldRender3D |
|---|---|---|
| PD-011 to PD-014 | Draoi layer rules | Plain library, depends down only, admission test (CoW first, a second hex game credible) |
| PD-001 | StringCRC for IDs | Class, channel, prop and parameter ids |
| PD-004 / AD-002 | No STL in public APIs | Pointer + count / `DynamicArrayC` |
| PD-005 | x64 / D3D11 | bgfx D3D11 path |
| AD-003 | Namespaces | `Draoi::HexWorldRender3D::` |
| CFB-001..005 | DiaCellFieldBake | Fields come only from the bake; no nearest-cell class lookups in the renderer |
| TOON-001..006 | DiaToonShading | The material is composed from its includes; no Dia include learns hex vocabulary; the style is a live-tweakable `AttributeSet` |

## Test Strategy

- **GoogleTests:** channel upload round-trips and versioning; scatter determinism, condition scaling and no props on water; custom slot binding; `ScreenToWorld`/`WorldToCell` round-trip.
- **Visual:** one headless capture per feature at a fixed world, camera and style, judged by the user (taste check, not a pixel gate). Captures are driven through the CoW module's capture command.
- **Performance (Debug, ballpark):** frame time at Large with everything on. The target is 30 fps (GDD V-1 Test 7).

## Open Design Questions

1. **Hex readability.** Is cell highlighting an outline drawn from the owner texture (here), or a DiaCellLayers overlay?
2. **Channel update cadence.** Should the weather channel update every frame (smoothed blend computed on the CPU) or upload two snapshots and blend on the GPU by day?
3. **Is four custom slots right?** One RGBA16F texture is the cheap layout. A game needing more than four CoW-only states would need a second texture. Revisit when CoW's list is known.

## Status

`Approved` (2026-11-23, user). Created by moving DiaTerrainMap3D (Approved 2026-11-20) to Draoi in the layer split. **All four features Approved (Feature 1 2026-11-23; 2, 3 and 4 2026-11-24, after spec review).** No AD-001 sign-off needed for the package itself (Draoi); the Dia pieces it depends on carry their own.
