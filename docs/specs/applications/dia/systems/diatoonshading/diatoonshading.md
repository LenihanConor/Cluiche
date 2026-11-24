# System Spec: DiaToonShading

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** rendering
**Research:** docs/research/tile_renderin_art_pipeline/summary.md (browser prototype: `docs/research/tile_renderin_art_pipeline/lookdev/js/shaders.js`)
**Consumers:** DraoiHexWorldRender3D terrain material (@docs/specs/applications/draoi/systems/draoihexworldrender3d/draoihexworldrender3d.md), first. Sprites and, later, meshes (CoW dragons, not yet decided) must be able to use the same techniques (TOON-002).

**Origin (2026-11-23, user):** the layer split of the former DiaTerrainMap3D. Generic techniques go to Dia (this system). Hex-terrain vocabulary, material composition and surface channels go to Draoi (DraoiHexWorldRender3D). The look itself (palettes, GDD numbers) goes to CoW (Strategic Map). This system holds the technique half of the former Style Shader Library feature.

## Purpose

DiaToonShading is a library of **stylised shading techniques**: illustrated-look ink, hatching, toon lighting and colour treatment, written as bgfx shader includes with documented parameter groups. A consumer composes its own material from them.

It exists because an illustrated look with no artist is built from these rules, and they are not terrain-specific. Iso-line ink works on any scalar field, coverage-driven hatching on any surface with a coverage value, and a toon ramp on any normal. Keeping them in Dia, free of world vocabulary, means a map, a sprite and later a mesh can share one art style.

**Dependency chain:**
`DiaToonShading → DiaBgfx (ShaderProgram, shader cook), DiaAttribute (ParamBinding, parameter files), DiaValueSchema (parameter ranges), DiaObservation, DiaCore`
`DiaToonShading/Testing → DiaBgfx3D` (Feature A render-target readback, for the golden harness only; the shipped module does not depend on DiaBgfx3D)

## Responsibilities

- **Ink and lines:** iso-line ink on a linear-filtered scalar at constant pixel width, edge ink from a margin field, form lines at the light terminator, contour lines, line boil.
- **Light and hatch:** toon ramp lighting from a normal or height gradient, slope blend, coverage-driven hatching with zoom-aware spacing and cross-hatch.
- **Colour and surface:** palette-slot allocation by noise bands, procedural mark patterns, masked overlay (tint, colour, emissive) with falloff, saturation grade by scalar bands with a bare blend, weighted cross-fade of grade presets (Screen/Multiply), paper grain.
- **Shared foundation:** noise and colour includes, the parameter-block convention, the shader root, and a golden-capture harness on synthetic inputs (ships in `DiaToonShading/Testing/`).

## Non-Responsibilities

- **Any world vocabulary.** No coast, snow, river, biome, season or terrain names in APIs, uniforms or shader file names. Consumers map their concepts onto techniques: "coast" is iso-line ink at land fraction 0.5, and "snow" is a masked overlay.
- **Materials.** Consumers compose their own fragment shader from the includes. This system ships no terrain, sprite or mesh material.
- **Geometry, textures and upload.** These belong to the consumer, or to DiaBgfx3D.
- **Full-screen post passes.** These are the DiaBgfx3D Post-Process Stack. Grain and grade includes can be called from a post pass, but the pass itself is DiaBgfx3D's.
- **Parameter values.** Defaults are neutral. The look's numbers come from the game.

## Design

### Shader layout

```
Dia/DiaToonShading/Shaders/
  toon/ noise.sh  colour.sh  params.sh        // Feature 1
  toon/ ink.sh  lines.sh  boil.sh              // Feature 2
  toon/ light.sh  hatch.sh                     // Feature 3
  toon/ palette.sh  marks.sh  overlay.sh  grade.sh  grain.sh   // Feature 4
```

Each include exposes functions with explicit inputs (`float field`, `vec2 gradient`, `float coverage`, `float pixelScale` ...) and reads no uniforms except its own parameter group. A consumer passes the values it sampled from its own textures.

### Parameter groups

Each technique has a C++ parameter struct (for example `Dia::ToonShading::InkParams`) with a DiaValueSchema sidecar for ranges and defaults, and a matching uniform layout. Consumers embed these structs in their own style files. **Every field is a live-tweakable DiaAttribute** (user 2026-11-24): an `AttributeSet` backs each struct, so the Inspector, DiaAPI and parameter files can all change it at run time, and the look-dev panel needs no per-technique code.

## Features

| # | Feature | Summary | Spec | Status |
|---|---|---|---|---|
| 1 | Shader Library Foundation | Shader root and cook config, noise and colour includes, parameter-block convention with schema sidecars, synthetic-input golden-capture harness | [shader-library-foundation.md](shader-library-foundation.md) | Approved |
| 2 | Ink and Lines | Constant-width iso-line ink, margin-edge ink, weight and noise width, form lines, contours, line boil | [ink-and-lines.md](ink-and-lines.md) | Approved |
| 3 | Light and Hatch | Toon ramp lighting, slope blend, coverage-driven hatching with zoom-aware spacing and cross-hatch | [light-and-hatch.md](light-and-hatch.md) | Approved |
| 4 | Colour and Surface | Palette slots by noise bands, procedural marks, masked overlay, band saturation grade, weighted grade cross-fade, paper grain | [colour-and-surface.md](colour-and-surface.md) | Approved |
| — | Sprite and mesh ink | Alpha-edge ink for sprites; outline ink for meshes | not specced | Spec once CoW decides how dragons are drawn (ODQ 1) |

Build order: 1, then 2, 3 and 4 in any order. The DraoiHexWorldRender3D Hex Terrain Material needs all four.

**Test gate (review 2026-11-24).** The shader code has almost no dependencies, but every golden capture (F1 AC 5, F2 AC 7, F3 AC 7, F4 AC 8) needs render-target readback from DiaBgfx3D Feature A, which is Draft and owes its AD-001 sign-off and a depth/RGBA16F spike. Until then the techniques can be written and checked through their CPU references and cook tests, but not goldened. **DiaCLI change owed (AD-001):** the shader cook passes only the bgfx include directories to `shaderc`, so `<toon/...>` includes from another root (Draoi, games) need the cook to add each `source_roots` entry as an include path (see Feature 1 task 1).

## Decisions

| ID | Decision | Status | Binding |
|---|---|---|---|
| TOON-001 | Stylised shading techniques are a Dia system with no world vocabulary; terrain, snow, coast and the like are consumer concepts (user 2026-11-23, layer split) | Accepted | Yes |
| TOON-002 | Techniques work on any surface (terrain, sprites, later meshes): inputs are generic values the consumer samples, never the consumer's textures (user 2026-11-23, so dragons can share the map's style) | Accepted | Yes |
| TOON-003 | Shipped as shader includes plus parameter groups; consumers compose materials; no runtime shader generation (bgfx shaders cook offline) | Accepted | Yes |
| TOON-004 | Style lives in shader code and parameter files; no technique may require per-state images (was TMAP-003) | Accepted | Yes |
| TOON-005 | Boil is a per-call parameter, off by default; the consumer decides which layers boil (TMAP-006 stays a consumer rule) | Accepted | Yes |
| TOON-006 | Every technique parameter is a live-tweakable attribute (`AttributeSet` + schema sidecar), no hard-coded look values in shaders (user 2026-11-24) | Accepted | Yes |

## Inherited Binding Decisions

| ID | Decision | How it constrains DiaToonShading |
|---|---|---|
| PD-005 | x64 / D3D11 | Every include compiles for dx11; dx12 and vulkan are cooked too |
| AD-001 | Module docs; engine changes signed off | New module doc; module and `pipeline.toml` shader root **signed off 2026-11-23** |
| AD-003 | `Dia::<Module>::` | `Dia::ToonShading::` |
| Platform: DiaAttribute / DiaValueSchema | Parameters are data | Every parameter struct has a sidecar, bound through `ParamBinding` (not a new binder) |
| AD-001 (DiaCLI) | CLI changes signed off | Cook include paths (see Test gate): **sign-off owed** |

## Test Strategy

- **Golden captures** per technique on synthetic inputs (a ramp, a step, a disc, a slope), rendered headless through the Feature 1 harness, 1/255 tolerance on a 3×3 mean.
- **Measured properties:** constant ink pixel width across zoom, hatch coverage against its target, grade formulas on a known grey, monotonic grades.
- **Cook test:** every include cooks for dx11, dx12 and vulkan with no warnings.

## Open Design Questions

1. **Sprite and mesh ink.** Dragons on the strategic map are undecided (ideally 3D, probably 2D sprites for now; user 2026-11-23). Sprite alpha-edge ink is cheap and fits TOON-002. Mesh outlines (inverted hull or a depth/normal edge pass) are left out of v1. Spec the feature when the dragon rendering is decided; nothing in Features 1–4 may assume a heightfield.
2. **Include granularity versus cost.** Many small functions each doing their own noise lookups may cost more than the prototype's single fused shader. Share noise samples through explicit inputs where measurement shows it matters (measured in the Draoi material, the first heavy consumer).
3. **Grain and grade: material or post?** Both work per surface (so sprites and terrain can differ) and as a post pass (so everything shares them). v1 offers both call sites. The consumer picks, and the Strategic Map's presets decide which one CoW uses.

## Status

`Approved` (2026-11-23, user). Created by the layer split. **AD-001 signed off 2026-11-23 (user):** new module `Dia/DiaToonShading` and the shader root `Dia/DiaToonShading/Shaders` in `pipeline.toml`. **Features 1–4 Approved 2026-11-24 (user, after spec review).** Still owed before build: AD-001 sign-off for the DiaCLI cook include-path change and the `Testing/Shaders` root (F1 task 1); golden harness (F1 task 4) is blocked on DiaBgfx3D Feature A.
