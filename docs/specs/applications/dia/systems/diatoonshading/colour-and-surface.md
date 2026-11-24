# Feature Spec: DiaToonShading Colour and Surface

## Parent System
@docs/specs/applications/dia/systems/diatoonshading/diatoonshading.md

**Gameplay Domains:** rendering
**Depends on:** Shader Library Foundation (Feature 1)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/shaders.js` (`GROUND_FS` palette, marks, snow, wet, scar, condition grade, season grade, grain)

## Status
`Approved` (2026-11-24, user, after spec review; cross-fade weights used as given). Drafted 2026-11-23. Generic half of the former Style Shader Library goals 1.1, 1.6, 1.7, 1.10–1.14. Which world states use these techniques (snow, wet, burn, condition, season) moved to the DraoiHexWorldRender3D Hex Terrain Material. The GDD numbers (season tints, condition bands) moved to CoW Map Visual Data.

## Summary

The colour treatments of the illustrated look:

- **Palette slots:** split a surface between primary, secondary and accent colours by two noise bands.
- **Marks:** procedural mark patterns (ticks, dots, strokes, specks, veins), chosen by id and density.
- **Masked overlay:** lay a colour, tint or emissive glow over a surface where a mask is above a threshold, with a soft falloff. Consumers use it for snow, wet ground, scars, glows, and the custom overlay slots.
- **Band grade:** change saturation by bands of a scalar, plus a blend toward a "bare" colour. Consumers use it for condition.
- **Weighted grade cross-fade:** N grade presets (blend mode Screen or Multiply, colour, strength) mixed by weights that sum to 1. Consumers use it for seasons.
- **Paper grain:** a fine static grain at a strength.

## Goals

1. Each treatment is one function with its own parameter group.
2. **Masked overlay** has three modes (replace colour, tint, emissive) and a falloff width, and the mask can be biased by two extra inputs (for example the consumer's elevation and temperature for snow).
3. **Band grade** takes up to 4 band thresholds with a saturation change per band, and is continuous across thresholds (no hard step unless the width is 0).
4. **Grade cross-fade** takes up to 4 presets and weights.
5. Defaults are neutral: every treatment at default parameters is the identity.

## Non-Goals

- Naming states or seasons, or choosing their numbers (consumer and game data).
- Texture-based detail art (TOON-004).

## Design

```glsl
// toon/palette.sh
vec3  toonPaletteSlots(vec3 primary, vec3 secondary, vec3 accent, vec2 noiseUv, PaletteParams p);
// toon/marks.sh
float toonMarks(vec2 worldUv, float markId, float density, float pixelScale, MarkParams p);
// toon/overlay.sh
vec3  toonOverlay(vec3 base, float mask, float biasA, float biasB, vec3 colour, OverlayParams p); // mode, threshold, falloff
// toon/grade.sh
vec3  toonBandGrade(vec3 base, float value, vec3 bare, BandGradeParams p);
vec3  toonGradeCrossFade(vec3 base, vec4 weights, GradePresetsParams p);
// toon/grain.sh
vec3  toonGrain(vec3 base, vec2 screenUv, GrainParams p);
```

**Cross-fade weights** are used as given, not normalised: the output is `base` plus the weighted sum of each preset's change, so weights that sum to less than 1 give a partial grade and weights above 1 over-apply. Consumers pass weights that sum to 1 (AC 5 tests that case and the all-zero case).

## Acceptance Criteria

1. **Palette slots.** On a flat patch, slot shares match the two band thresholds within ±3 percentage points. A fixed seed gives a stable pattern.
2. **Marks.** Each mark id renders its pattern. Measured density scales with the density input. A density of 0 draws nothing.
3. **Overlay.** A mask below threshold leaves the base unchanged. Above threshold plus falloff, replace mode gives the colour exactly. Coverage is monotonic in the mask and in each bias input. Emissive mode adds and never darkens.
4. **Band grade.** Saturation at a value inside each band matches the band's change within 1/255. The output is continuous across thresholds. Bare blend at strength 0 is the identity.
5. **Grade cross-fade.** For a known grey, a single preset at weight 1 gives the Screen or Multiply formula at its strength (within 1/255). Half-way between two presets equals the average of the two. All-zero strengths are the identity.
6. **Grain** at strength 0 is the identity and is stable from frame to frame.
7. **Neutral defaults.** Every function at its default parameters is the identity, defined per function (within 1/255): `toonPaletteSlots` returns `primary`; `toonMarks` returns 0 (density 0); `toonOverlay`, `toonBandGrade`, `toonGradeCrossFade` and `toonGrain` return `base`.
8. **Goldens.** One golden per function (Feature 1 harness).

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| TOON-001 | No world vocabulary | "Overlay", "band grade", "grade preset"; snow, condition and season are consumer words |
| TOON-002 | Any surface | Inputs are colours, values and UVs |
| TOON-003 | Includes plus parameter groups | One parameter group per function, sidecars (Feature 1 convention) |
| TOON-004 | No per-state images | All procedural |

## Open Design Questions

1. **How many grade presets and bands.** Four of each fits CoW (4 seasons, 4 condition bands) in a `vec4`. A game needing more would mean a second call or a wider layout. Revisit only if a consumer asks.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Palette slots and marks | AC 1, 2 | Not Started | sonnet | |
| 2 | Masked overlay | AC 3 | Not Started | sonnet | |
| 3 | Band grade and grade cross-fade | AC 4, 5 | Not Started | sonnet | |
| 4 | Grain, neutral-default sweep, goldens | AC 6, 7, 8 | Not Started | sonnet | |
