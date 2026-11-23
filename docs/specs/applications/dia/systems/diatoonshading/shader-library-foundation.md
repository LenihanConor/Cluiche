# Feature Spec: DiaToonShading Shader Library Foundation

## Parent System
@docs/specs/applications/dia/systems/diatoonshading/diatoonshading.md

**Gameplay Domains:** rendering
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/shaders.js` (`COMMON`)

## Status
`Draft` (2026-11-23). Split out of the former DiaTerrainMap3D Style Shader Library (its tasks 1, 2 and 9, harness part). **AD-001 signed off 2026-11-23 (user)** for the new module and the `pipeline.toml` shader root.

## Summary

The groundwork every technique sits on: a cooked shader root, the shared noise and colour includes, the parameter-block convention, and a headless harness that renders one technique on a synthetic input and compares it with a golden.

## Problem

- bgfx shaders cook from `.sc` files under `pipeline.toml [bgfx_shaders] source_roots`. Today only `Dia/DiaBgfx/Shaders` and `Dia/DiaBgfx3D/Shaders` are scanned.
- Techniques must be testable one at a time, without a terrain, a game or a world.
- Every technique needs the same noise and colour maths. Duplicating it per include drifts.

## Goals

1. **Shader root.** `Dia/DiaToonShading/Shaders` added to the cook roots. Includes live under `toon/`, so consumers write `#include <toon/ink.sh>`. Consumer shader roots (Draoi, games) can include them.
2. **Common includes.** `noise.sh` (value noise, fbm, ridged, hash; deterministic across backends), `colour.sh` (sRGB/linear, HSL saturation and lightness, Screen and Multiply blend), `params.sh` (uniform packing helpers).
3. **Parameter convention.** One C++ struct per technique (`Dia::ToonShading::<Technique>Params`), a DiaValueSchema sidecar with range and default per field, a `Pack` to a fixed uniform layout, clamping with a warning on out-of-range values.
4. **Golden harness** in `DiaToonShading/Testing/`: render a test fragment shader (one technique on a synthetic input) to an offscreen target at a fixed size, read it back, compare with a stored golden (1/255 on a 3×3 mean), with an update switch like the existing golden stores.

## Non-Goals

- Any technique (Features 2–4).
- Consumer shader roots. Draoi's and CoW's roots are their own features.
- A runtime shader graph or permutation system (TOON-003).

## Acceptance Criteria

1. **Cooks.** `dia pipeline` cooks the root for dx11, dx12 and vulkan with no warnings. A consumer shader outside the root that includes `<toon/noise.sh>` cooks.
2. **Noise determinism.** The noise functions give the same values (within 1e−4) on the CPU reference and on dx11 for a fixed set of points. `ridged` stays in [0, 1].
3. **Colour maths.** Screen and Multiply on a known grey match the formula within 1/255. A saturation change of 0 is the identity.
4. **Parameters.** Every field of a sample parameter struct has a sidecar range and default. An out-of-range value is clamped with one warning. `Pack` round-trips through a read-back test shader.
5. **Harness.** A deliberately changed test shader fails its golden. The update switch rewrites it. The harness runs headless under `dia run googletest`.
6. **Observability.** Shader cook or load failure logs the file and error (channel `TOON`).

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| TOON-001 | No world vocabulary | Common includes are maths only |
| TOON-003 | Includes plus parameter groups, no runtime generation | Offline cook; fixed uniform layouts |
| AD-001 | Engine and pipeline changes need sign-off | Module and shader root signed off 2026-11-23 |

## Open Design Questions

1. **Headless GPU in GoogleTests.** The golden harness needs a bgfx device. If GoogleTests cannot create one reliably, the goldens run as a CluicheTest stage or E2E capture instead. Decide in task 1 from what DiaBgfx3D Feature A (render targets, readback) provides.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Module skeleton `Dia/DiaToonShading/`, module doc, deps, shader root in `pipeline.toml` | AC 1; `dia check deps` | Not Started | sonnet | AD-001 signed off 2026-11-23 |
| 2 | `noise.sh`, `colour.sh` with CPU references | AC 2, 3 | Not Started | sonnet | |
| 3 | Parameter convention, sidecar, `Pack` | AC 4 | Not Started | sonnet | |
| 4 | Golden harness in `Testing/` | AC 5 | Not Started | sonnet | needs DiaBgfx3D Feature A readback |
| 5 | Observability | AC 6 | Not Started | haiku | |
