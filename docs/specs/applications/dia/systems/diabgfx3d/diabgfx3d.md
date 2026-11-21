# System Spec: DiaBgfx3D

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** rendering, lighting

## Amendment 2026-11-20: generic 3D features for DiaTerrainMap3D (user, 3-way split)

DiaTerrainMap3D (@docs/specs/applications/dia/systems/diaterrainmap3d/diaterrainmap3d.md) needs these capabilities. Any 3D game needs them too, so they belong here as DiaBgfx3D features, not in DiaTerrainMap3D. All are proven in the research prototype (`docs/research/tile_renderin_art_pipeline/lookdev/`). Each needs AD-001 sign-off at its feature spec, before build. The module doc's "post-processing out of scope" line is revised by the post-process feature.

Prototype references (`docs/research/tile_renderin_art_pipeline/lookdev/js/`):
- **A:** `alive.js` `PostFX.ensure` (colour + depth target).
- **B:** `PROP_VS`/`PROP_FS` in `shaders.js` and `setPropAttribs` in `app.js`.
- **C:** `weather.js` `Precip`.
- **D:** `alive.js` `POST_FS`.

| # | Feature | Summary | Spec | Status |
|---|---|---|---|---|
| A | Render Targets and Dynamic Textures | Offscreen colour + depth targets, readback for headless capture; `BgfxTextureHandle::Update` for per-frame CPU-side texture refresh (RGBA8, RGBA16F) | [render-targets-and-textures.md](render-targets-and-textures.md) | Draft |
| B | Instanced Sprites | Camera-facing sprite instancing from an atlas: per-instance position, scale, atlas index and palette (palette-masked R/G/B + ink channels), static and per-frame instance buffers, cutout depth and a translucent layer | [instanced-sprites.md](instanced-sprites.md) | Draft |
| C | Particles | CPU-simulated world-space particles with a budget: line streaks, point flakes, soft puffs; emitters fed by a density callback or per-cell texture; viewport culling | [particles.md](particles.md) | Draft |
| D | Post-Process Stack | Ordered full-screen passes over colour + depth: tilt-shift focus band, distance haze, sky fill above the horizon, depth silhouettes (second difference of 1/z, flat-surface safe), vignette | [post-process-stack.md](post-process-stack.md) | Draft |

These are planned features under the existing (stub) system spec; they do not change built code until their feature specs are approved.
