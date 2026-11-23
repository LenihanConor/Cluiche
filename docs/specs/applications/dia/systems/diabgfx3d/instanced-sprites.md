# Feature Spec: DiaBgfx3D Instanced Sprites (Feature B)

## Parent System
@docs/specs/applications/dia/systems/diabgfx3d/diabgfx3d.md (Amendment 2026-11-20)

**Gameplay Domains:** rendering
**Needed by:** DraoiHexWorldRender3D Map Props and Emitters; Particles (C) reuses the quad pipeline
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/shaders.js` (`PROP_VS`, `PROP_FS`), `js/props.js` (atlas build, palette masks), `js/app.js` (`setPropAttribs`, instance buffers)

## Status
`Draft` (2026-11-20). Drafted from the approved prototype; awaiting user review. **Needs AD-001 sign-off before build.**

## Summary

Draw thousands of camera-facing, upright sprites from an atlas in a handful of draw calls, each recoloured by its own palette.

- **Palette-masked atlas.** A sprite's pixels encode *which colour slot* they use, not a colour: red = primary, green = secondary, blue = accent, black = ink. Each instance supplies the three slot colours, so one pine sprite becomes forest, alpine or tundra, in every season, with no extra art.
- **Static and dynamic sets.** A static buffer (scattered props) is uploaded once. A per-frame transient buffer (clouds, fauna) is rebuilt every frame.
- **Two layers.** Cutout (alpha tested, depth written, drawn with the opaque scene) and translucent (blended, no depth write, drawn last).
- **Sway.** Fixed-frequency sway whose amplitude is set per instance and scaled by a gust value, with no frequency changes (TMAP-006).

Today `SpriteRenderer` (DiaBgfx, 2D) submits one draw per sprite and `MeshRenderer` is capped at 4,096 draws with no instancing. This feature adds the instanced path.

## Problem

- A Small world needs about 4,000 props, and Large several times that. One draw per prop is too slow, and `kMaxMeshDraws` is 4,096.
- Per-biome and per-season colour variants of the same art would multiply the asset count. Palette masking removes that.
- Props need to stand upright on a tilted, rolling heightfield and face the camera.

## Goals

1. `SpriteAtlas`: a grid atlas of palette-masked sprites, loaded from a PNG, uploaded with a full mip chain.
2. `SpriteInstance` (compact, fixed layout) and two submission paths: static (`SetStaticInstances`) and per-frame (`SubmitDynamic`).
3. A `SpriteInstanceRenderer` in DiaBgfx3D with cutout and translucent layers and correct depth behaviour against the scene.
4. Palette decode and ink in the fragment shader; fixed-frequency sway in the vertex shader; optional gust texture.
5. Billboards that stay upright with a tilt-dependent lean toward the camera.

## Non-Goals

- Sorting translucent sprites by depth. v1 draws them unsorted with depth test and no depth write; the map's translucent use (clouds, haze puffs) tolerates it.
- Skeletal animation, or animated atlas frames. Motion is sway plus whatever the game does by moving instances.
- Lighting, shadows or normal maps on sprites.
- Culling. The caller supplies only the instances to draw (DraoiHexWorldRender3D culls by chunk).

## Design

```cpp
namespace Dia::Bgfx3D {
    struct SpriteInstance {                 // 36 bytes, no padding
        float    x, y, z;                    // foot position, world
        float    scale;                      // world units tall
        uint16_t atlasIndex;                 // cell in the atlas grid
        uint16_t flags;                      // bit0 rigid (no sway), bit1 flipX
        float    phaseSeed;                  // 0..1: sway phase, aux value
        uint32_t primary, secondary, accent; // RGBA8 palette slots
    };
    enum class SpriteLayer : uint8_t { Cutout, Translucent };

    struct SpriteAtlasDesc { uint16_t cols, rows; uint16_t cellPx; };
    class SpriteAtlas { bool LoadFromPng(const char* assetPath, const SpriteAtlasDesc&); /* uploads with mips */ };

    struct SpriteStyle {
        uint32_t inkColour = 0xFF20242A; float alpha = 1.0f;
        float swayFreq = 1.6f, swayBaseAmp = 0.0f, swayGustAmp = 0.0f;
        float uprightBlend = 0.35f;          // lean toward world-up when the camera tilts
        float cutoff = 0.5f;
    };

    class SpriteInstanceRenderer {
    public:
        bool SetStaticInstances(SpriteLayer, const SpriteInstance*, uint32_t count);   // replaces; count <= kMaxInstances
        void SubmitDynamic(SpriteLayer, const SpriteInstance*, uint32_t count);        // this frame only
        void SetGustTexture(unsigned short textureIdx, const float worldToUV[4]);      // optional; r channel = gust
        void Draw(const Camera3D&, const SpriteAtlas&, const SpriteStyle&, float timeSeconds);
    };
    static constexpr uint32_t kMaxInstances = 65536;
}
```

- **Billboard.** A unit quad (width 1, height 1, foot at bottom centre) expanded as `foot + right × x·scale + up × y·scale`. `right` is the camera right vector; `up` is `normalize(mix(cameraUp, worldUp, uprightBlend))`, so props stand up on slopes yet face the viewer.
- **Palette decode.** `m = texel.rgb` (the mask); `ink = clamp(1 − (m.r + m.g + m.b), 0, 1)`; `colour = primary·m.r + secondary·m.g + accent·m.b`; `final = mix(colour, ink, ink)`; alpha from the texel alpha.
- **Atlas mips.** The atlas is uploaded with a generated mip chain (bimg), because the mask must filter correctly when sprites are small. Premultiplied alpha is *not* used (the mask channels are data, not colour).
- **Sway.** `x offset = sin(time × swayFreq + phaseSeed × 40) × (swayBaseAmp + gust × swayGustAmp) × y²`, skipped when the rigid flag is set. The frequency is a constant, so a changing gust only changes amplitude.
- **Depth.** Cutout draws with depth test and depth write, after the mesh pass in the scene target. Translucent draws with test and no write, after cutout and before the post pass.
- **Instancing.** Static sets live in a `bgfx` vertex buffer used as instance data; dynamic sets use `allocInstanceDataBuffer` (transient). D3D11 hardware instancing.

## Acceptance Criteria

1. **Layout.** `sizeof(SpriteInstance) == 36`, offsets as specified (static assert).
2. **Limits.** `SetStaticInstances` with `count > kMaxInstances` returns false, logs a warning and leaves the previous set; a null pointer with a nonzero count returns false.
3. **Draw count.** Drawing N static instances and M dynamic instances in one layer submits at most 2 draws for that layer, however large N and M are (counter exposed for tests).
4. **Atlas indexing.** `atlasIndex` maps to cell `(i % cols, i / cols)`. A test atlas of solid-colour cells renders the matching cell for several indices; an index ≥ cols × rows draws nothing and logs once.
5. **Palette decode.** A test sprite with pure red, green, blue and black regions and three palette colours renders those colours in those regions (pixel check within 2/255), and black regions render as the ink colour.
6. **Same sprite, different palettes.** Two instances of one sprite with different palettes render different colours with the same silhouette (identical alpha coverage).
7. **Upright and facing.** With a 55° tilted camera, a sprite's base stays at its foot position and its top is screen-up within 20° of the camera up; at `uprightBlend = 1` it is world-up exactly.
8. **Cutout and depth.** A cutout sprite behind a mesh is hidden by it; in front it hides it; texels below `cutoff` write no depth.
9. **Translucent.** A translucent instance blends with what is behind it (alpha 0.5 gives the mean) and does not write depth, so an opaque draw after it is not occluded by its quad.
10. **Sway.** Rigid instances do not move; flexible instances move at a fixed frequency, and changing the gust amplitude at runtime changes displacement only, with no phase jump (the displacement at time t is continuous when gust changes between frames, checked numerically).
11. **Dynamic reuse.** Submitting dynamic instances every frame for 1,000 frames leaks nothing and stays within the transient buffer limits (a submit that cannot allocate logs a warning and skips the draw, never asserts).
12. **Atlas mips.** The atlas has a full mip chain and a minified sprite (drawn at 1/8 size) still shows its colour regions without shimmering (mask channels average, not alias).
13. **Observability.** Metrics `bgfx.sprites_drawn` and `bgfx.sprite_draws`; a profile scope per layer; log channel `BGFX`.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| TMAP-006 | No boil or frequency-varying sway on props | Fixed `swayFreq`; AC 10 |
| RB-006 | No backend types in DiaGraphics public surface | Renderer, atlas and instance types live in DiaBgfx3D |
| AD-001 | Engine API changes need sign-off | **Sign-off required** |
| PD-004 | No STL in public APIs | Raw pointers and counts |
| RB-001 | bgfx abstraction | Instanced draws via bgfx |

## Open Design Questions

1. **Atlas source.** The shipped atlas is built from SVG and rasterised by a DiaCLI step (the asset forge, `.texture.png` today has no mip or atlas handling). Does the atlas builder live here or in CoW's asset pipeline? Proposed: the `SpriteAtlas` loader is generic and lives here; the *builder* (SVG → atlas PNG) is a DiaCLI handler specced separately.
2. **Per-instance depth bias.** On a rolling heightfield, a sprite's lower texels can sink into the terrain. The prototype lifted nothing and looked fine at the tested tilts. If low tilts show clipping, add a small view-space depth bias.
3. **Sorting for translucent layers** may be needed if overlapping clouds look wrong.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `SpriteInstance`, layouts, limits, static buffer | AC 1, 2 | Not Started | sonnet | |
| 2 | `SpriteAtlas` load + mip chain | AC 4, 12 | Not Started | sonnet | |
| 3 | Shaders: billboard, palette decode, sway; cook entries | AC 5, 6, 7, 10 | Not Started | sonnet | |
| 4 | Renderer: cutout and translucent layers, depth state | AC 8, 9 | Not Started | sonnet | |
| 5 | Dynamic instances (transient buffer) | AC 3, 11 | Not Started | sonnet | |
| 6 | Gust texture binding | AC 10 | Not Started | sonnet | |
| 7 | Metrics, profile, docs | AC 13 | Not Started | haiku | |
