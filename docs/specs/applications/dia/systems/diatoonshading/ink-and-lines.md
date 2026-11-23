# Feature Spec: DiaToonShading Ink and Lines

## Parent System
@docs/specs/applications/dia/systems/diatoonshading/diatoonshading.md

**Gameplay Domains:** rendering
**Depends on:** Shader Library Foundation (Feature 1)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/shaders.js` (`GROUND_FS` ink, form lines, contours, boil)

## Status
`Draft` (2026-11-23). Generic half of the former Style Shader Library goals 1.2, 1.5 and 1.14 (ink); the coast, lake and border composition moved to the DraoiHexWorldRender3D Hex Terrain Material.

## Summary

Bold, hand-drawn-looking lines at a constant on-screen width, drawn from fields instead of geometry:

- **Iso-line ink:** a line where a linear-filtered scalar crosses a level (the consumer's coast is land fraction at 0.5).
- **Edge ink:** a line where a margin field approaches zero (the consumer's class borders).
- **Form lines:** ink where the light terminator crosses the surface.
- **Contours:** iso-lines at a fixed interval of a height value.
- **Line boil:** a frame-stepped wobble on line positions, for a hand-drawn feel.

## Goals

1. **Constant pixel width.** Width is set in pixels and held across zoom and tilt, using the screen-space derivative of the field.
2. **Weight and variation.** A per-call weight multiplier (the consumer's per-stratum weights) and a noise-driven width variation.
3. **No false lines.** A flat field (constant value) never produces ink at any zoom or tilt.
4. **Form lines and contours** from a height value and gradient.
5. **Boil** as a parameter: a frame index (the consumer steps it, for example at 12 Hz) offsets the line noise. With boil off, lines never change.

## Non-Goals

- Choosing which fields get ink, or their weights (consumer).
- Silhouette ink over the whole screen (DiaBgfx3D Post-Process Stack, depth silhouettes).
- Sprite and mesh outlines (system ODQ 1).

## Design

```glsl
// toon/ink.sh
float toonIsoInk(float field, float level, float widthPx, float weight, vec2 noiseUv, InkParams p);
float toonEdgeInk(float margin, float widthPx, float weight, vec2 noiseUv, InkParams p);
// toon/lines.sh
float toonFormLine(vec3 normal, vec3 lightDir, float widthPx, LineParams p);
float toonContour(float height, float interval, float widthPx, LineParams p);
// toon/boil.sh
vec2  toonBoilOffset(vec2 uv, float boilFrame, BoilParams p);   // zero when p.on == 0
```

Each returns ink coverage in [0, 1]. The consumer mixes ink colour by it.

## Acceptance Criteria

1. **Width.** On a synthetic ramp, iso-line ink is the same pixel width at three zoom levels and two tilts (within 1 px).
2. **Weight.** Weights 1, 1.2 and 1.5 give widths in that ratio (±10%).
3. **Edge ink.** On a synthetic margin field, ink appears only where margin is below its threshold, at constant width.
4. **No false lines.** A constant field, a constant margin of 1 and a flat height give zero ink at every zoom and tilt in the captures.
5. **Form lines and contours.** A synthetic hemisphere shows one form line at the terminator for a known light direction. Contours on a linear ramp are evenly spaced at the interval.
6. **Boil.** Line positions change between boil frames, are stable within a frame, and never change with boil off.
7. **Goldens.** One golden per function on its synthetic input (Feature 1 harness).

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| TOON-001 | No world vocabulary | Functions take fields and levels, not coasts or borders |
| TOON-002 | Any surface | Inputs are values and derivatives; no texture reads |
| TOON-005 | Boil is a parameter, off by default | `BoilParams.on` defaults to 0 |

## Open Design Questions

1. **Derivatives on sprites.** Screen-space derivatives work on terrain. On sprites (alpha-tested quads) they are noisy at the cutout edge. That matters only for the sprite-ink feature (system ODQ 1), not for v1.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `toonIsoInk`, `toonEdgeInk`, weight and noise width | AC 1, 2, 3, 4 | Not Started | sonnet | |
| 2 | Form lines and contours | AC 5 | Not Started | sonnet | |
| 3 | Boil | AC 6 | Not Started | sonnet | |
| 4 | Goldens | AC 7 | Not Started | sonnet | |
