# Feature Spec: DiaToonShading Light and Hatch

## Parent System
@docs/specs/applications/dia/systems/diatoonshading/diatoonshading.md

**Gameplay Domains:** rendering, lighting
**Depends on:** Shader Library Foundation (Feature 1)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/shaders.js` (`GROUND_FS` toon shade, hatch, slope rock)

## Status
`Approved` (2026-11-24, user, after spec review; hatch formula option A chosen). Drafted 2026-11-23. Generic half of the former Style Shader Library goals 1.3 and 1.4. The per-stratum coverage numbers (GDD 15 / 40 / 70%) moved to CoW Map Visual Data.

## Summary

Flat, banded lighting and pen hatching:

- **Toon ramp:** diffuse light quantised into bands, with a banding amount (0 = smooth, 1 = hard bands).
- **Slope blend:** a factor from surface steepness, so a consumer can blend to a rock colour and add ink ticks on steep ground.
- **Hatching:** pen strokes whose ink coverage follows a target coverage value, driven by shading and noise, with spacing that doubles as the view zooms out and cross-hatch above a coverage threshold.

## Goals

1. **Toon ramp** from a normal (or a height gradient converted to a normal) and a light direction, with band count and banding amount.
2. **Slope factor** in [0, 1] from the normal and a slope range.
3. **Coverage hatching.** Input: target coverage in [0, 1], the shading value, a pixel scale. Measured ink coverage on a flat patch tracks the target.
4. **Zoom-aware spacing.** Stroke spacing stays within a pixel range by doubling or halving at LOD steps, blended so no pop is visible.
5. **Cross-hatch** above a coverage threshold (parameter).

## Non-Goals

- Dynamic lights or shadows. The light is a direction parameter.
- Which surfaces get which coverage (consumer; for CoW, strata coverage is GDD data).

## Design

```glsl
// toon/light.sh
float toonRamp(vec3 normal, vec3 lightDir, LightParams p);          // 0..1 banded
vec3  toonNormalFromGradient(vec2 gradient, float heightScale);
float toonSlope(vec3 normal, LightParams p);                        // 0 flat .. 1 at p.slopeMax
// toon/hatch.sh
float toonHatch(vec2 worldUv, float coverage, float shade, float pixelScale, HatchParams p);
```

**How coverage and shade combine.** `coverage` is the target ink fraction for the patch. `shade` (0 dark .. 1 lit, typically `toonRamp`) shifts the local target: `target = clamp(coverage + p.shadeGain * (0.5 - shade), 0, 1)`, with `shadeGain` defaulting to 0 (neutral: shade has no effect). Noise (`p.noiseAmount`, default 0) jitters stroke thresholds only, so mean coverage is unchanged. On a flat patch at `shade = 0.5` the output is exactly `coverage`, which is what AC 4 measures.

## Acceptance Criteria

1. **Ramp.** With band count N and banding 1, a synthetic hemisphere shows exactly N flat bands. With banding 0, the shade equals Lambert within 1/255.
2. **Gradient normal.** A planar ramp of known gradient gives the analytic normal (within 1e−3).
3. **Slope.** `toonSlope` is 0 on flat ground, 1 at `slopeMax` and monotonic in between.
4. **Coverage.** On a flat patch, measured ink coverage (ink pixels over patch pixels) is within ±5 percentage points of the target, for targets 0.10, 0.15, 0.40, 0.70 and 0.80 (at shade 0.5, noise off). **Risk:** 0.70 and 0.80 rely on cross-hatch; if the first prototype cannot hold ±5 there, relax those two targets to ±8 and record it here.
5. **Spacing.** Across a continuous zoom sweep, stroke spacing in pixels stays within its range and coverage stays within AC 4's tolerance (no pop beyond it).
6. **Cross-hatch** appears only above its threshold.
7. **Goldens.** One golden per function (Feature 1 harness).

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| TOON-001 | No world vocabulary | Coverage is a number; strata and zones are consumer concepts |
| TOON-002 | Any surface | Inputs are normals, values and a pixel scale |
| TOON-003 | Includes plus parameter groups | `LightParams`, `HatchParams` with sidecars (Feature 1 convention) |
| TOON-004 | No per-state images | Strokes are procedural |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Toon ramp, gradient normal, slope | AC 1, 2, 3 | Not Started | sonnet | |
| 2 | Coverage hatching and cross-hatch | AC 4, 6 | Not Started | sonnet | |
| 3 | Zoom-aware spacing | AC 5 | Not Started | sonnet | |
| 4 | Goldens | AC 7 | Not Started | sonnet | |
