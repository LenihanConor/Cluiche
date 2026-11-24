# Feature Spec: DraoiHexWorldRender3D Hex Terrain Material

## Parent System
@docs/specs/applications/draoi/systems/draoihexworldrender3d/draoihexworldrender3d.md

**Gameplay Domains:** rendering
**Depends on:** Hex Terrain Surface (Feature 1: textures); DiaToonShading Features 1–4 (@docs/specs/applications/dia/systems/diatoonshading/diatoonshading.md); DiaBgfx3D Feature E (displacement include)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/shaders.js` (`GROUND_FS`), screenshots in `lookdev/shots/`

## Status
`Approved` (2026-11-24, user, after spec review: live-tweakable style, boil clock `boilHz`, river banks above width 0, custom-slot branch). Drafted 2026-11-23. **Was DiaTerrainMap3D Feature 2 Style Shader Library (Draft 2026-11-20).** Split by layer: the techniques went to DiaToonShading, and the GDD numbers (strata coverage 15 / 40 / 70%, ink weights 1 / 1.2 / 1.5, season tints, condition bands) went to CoW Map Visual Data (@docs/specs/applications/cow/systems/strategicmap/map-visual-data.md). This feature keeps the hex-world composition. The old game-side shader override root is dropped: CoW-only looks go through the custom slots (TMAP-010).

## Summary

The hex terrain material: one fragment shader composed from DiaToonShading includes, reading Feature 1's textures. It knows the hex-world vocabulary and turns it into techniques:

| Hex-world concept | Inputs | DiaToonShading technique |
|---|---|---|
| Coast | land fraction at 0.5 | iso-line ink, weight by stratum |
| Lake shore | lake fraction at the lake level | iso-line ink |
| Class borders | margin field | edge ink, weight by stratum |
| Strata look | stratum per cell | hatch coverage and ink weight per stratum, ambient grade per stratum |
| Hillshade, slopes | elevation gradient | toon ramp, slope blend to rock with ink ticks, form lines, optional contours |
| Biome colour | primary/secondary class, season weights | palette slots by noise, per-season palette blend |
| Ground marks | class | mark pattern per class |
| Condition | Condition channel | band grade + bare blend with cracks |
| Snow | Snow channel (pack, temperature) + elevation | masked overlay biased by elevation and temperature |
| Ocean, lake, ice | elevation below sea, lake fraction, Water channel | depth tint, wave marks, coast foam; lake surface; ice overlay |
| Rivers | river field + Water channel flow state | water band, animated flow streaks, inked banks, width by flow state, flood band, frozen |
| Cloud shadow, wet ground | Weather channel | masked overlay (darken) on drifting noise; wet darkening, zero for snow |
| Burn, scar | Events channel | emissive overlay along the front; charcoal overlay that fades |
| Season | season weights (uniform) | grade cross-fade over up to 4 presets |
| Custom slots 0–3 | custom slot texture | the technique each slot is bound to in the style file |
| Boil, grain | frame index, screen | line boil on terrain ink only (TMAP-006); paper grain |

It also defines `HexWorldStyle` (the parameter file that holds every number, embedding the DiaToonShading parameter groups) and ships **neutral defaults** (TMAP-012).

## Problem

- The approved prototype is one large fragment shader that mixes techniques, hex vocabulary and CoW's numbers. Each now has its own layer.
- Draoi must not learn CoW concepts, yet CoW needs CoW-only looks (custom slots).
- Every rule must be tunable from a data file, with no shader edit.

## Goals

1. **Composition.** `fs_hexterrain.sc` composes DiaToonShading includes per the table above. No technique maths lives here, only the mapping and the order of layers.
2. **Strata-driven look.** Ink weight, hatch coverage and ambient grade come from a per-stratum table (up to 4 strata, `StratumIndex` from DraoiHexWorld), filled by the game.
3. **Rivers by flow state.** Dry, Low, Normal, Flood and Frozen change width and colour from the style's per-state table.
4. **Custom slots.** Each of 4 slots binds to `{ technique: tint | overlay | emissive | hatch, colour, threshold, falloff, enabled }` in the style file. An unbound slot costs nothing visible.
5. **`HexWorldStyle`** parameter file: every field documented and range-checked (DiaAttribute + DiaValueSchema sidecar); embeds DiaToonShading parameter groups. **Everything is live-tweakable (user 2026-11-24, TOON-006):** the style is an `AttributeSet`, so every field, including the per-stratum arrays, per-flow-state arrays and custom-slot bindings, can be changed at run time through the Inspector, DiaAPI or a parameter file, taking effect next frame with no rebuild or shader reload. The style is touched on the render thread only; edits from other PUs are marshalled over a stream by the game's module (Map Assembly goal 5b). **No look constant lives in the shaders:** every number that affects appearance (noise scales, drift and flow speeds, wave and foam size, crack density, cloud-shadow scale, relief tilt, the boil rate and so on) is a style field with a range and a neutral default. Only structural constants stay in code (array capacities, texture layouts).
6. **Neutral defaults.** The defaults draw a plain, readable hex map with no game's identity (grey-green palette, one stratum, no season tint). CoW's look comes only from CoW's style file.
7. **Shader root.** `Draoi/DraoiHexWorldRender3D/Shaders`, cooked by the standard pipeline, including DiaToonShading's root. This relies on the DiaCLI cook change that adds every `source_roots` entry as a `shaderc` include path (DiaToonShading Feature 1 task 1; sign-off given 2026-11-24).
8. **Data views** for class, elevation, condition, stratum, temperature, margin, each standard channel and each custom slot.

## Non-Goals

- Technique maths (DiaToonShading).
- Any GDD number or CoW name (CoW Map Visual Data).
- A game-side shader override root (dropped 2026-11-23; ODQ 2).
- Props, particles, post pass, sky (Features 3, 4 and DiaBgfx3D).
- Dynamic lights and shadows.

## Design

### Parameter surface

`HexWorldStyle`:

| Group | Parameters |
|---|---|
| Geometry | `heightScale`, light (DiaToonShading `LightParams`) |
| Ink | DiaToonShading `InkParams`, `BoilParams`; `boilHz` (the material derives `boilFrame = floor(timeSeconds * boilHz)`; boil itself stays off by default, TOON-005); `inkColour`; per-stratum `inkWeight[4]` |
| Hatch | DiaToonShading `HatchParams`; per-stratum `hatchCoverage[4]` |
| Lines | DiaToonShading `LineParams`; `formLinesOn`, `contoursOn`, `contourInterval` |
| Palette | DiaToonShading `PaletteParams` (band thresholds, noise scale) |
| Season | `seasonWeights[4]` (uniform, set per frame by the game; used as given, not normalised), DiaToonShading `GradePresetsParams` (up to 4 presets), per-stratum `ambient[4]` |
| Condition | DiaToonShading `BandGradeParams` (thresholds, saturation per band), `bareColour`, `crackStrength`, `crackScale` |
| Water | `seaShallow`, `seaDeep`, `seaDepthRange`, `waveScale`, `waveSpeed`, `foamOn`, `foamWidth`, `lakeLevel`, `iceOn`, per flow state `riverWidth[5]`, `riverColour[5]`, `riverFlowSpeed[5]`, `streakScale`, `bankInkWeight`, `floodOn` |
| Snow | DiaToonShading `OverlayParams` (colour, threshold, falloff), `snowLineBias`, `snowTempBias` |
| Weather | `cloudShadowOn`, `cloudShadowStrength`, `cloudShadowScale`, `cloudDriftSpeed`, `cloudThreshold`, `wetStrength` |
| Events | `burnColour`, `scarColour`, `scarFade`, `burnGlowWidth` |
| Custom | `slot[4] = { technique, colour, threshold, falloff, enabled }` |
| Marks/grain | `marksOn`, DiaToonShading `MarkParams`, `GrainParams` |
| Debug | `dataView` |

Other `HexWorldStyle` groups belong to the other features and sit in the same `AttributeSet`: **Props and Particles** (Feature 3: scatter, sway, density gains; DiaBgfx3D `SpriteStyle` and `ParticleKindParams`) and **Layers and Post** (Feature 4: `layers.*` toggles, DiaBgfx3D `CompositeParams` including `wobbleHz`). DiaBgfx3D's param structs are plain; `HexWorldMap` snapshots them from the style with `ParamBinding` each frame.

The groups list the fields the prototype needs; the sidecar is the authoritative list, and any further look number found while building goes in as a field (goal 5), never as a shader literal.

`HexWorldVisuals` (per class, from the game): `palette[class] = { primary, secondary, accent }`, per-season palette overrides, mark id and density, relief weights (passed to DraoiHexWorldRelief). Uploaded as a small uniform array or texture.

### Shader structure

```
Draoi/DraoiHexWorldRender3D/Shaders/
  varying.def.sc
  hexterrain/ coast.sh  strata.sh  water.sh  rivers.sh  surface.sh  custom.sh  views.sh
  vs_hexterrain.sc   fs_hexterrain.sc      // composes DiaToonShading + hexterrain includes
```

## Acceptance Criteria

1. **Cooks.** `dia pipeline` cooks the root for dx11, dx12 and vulkan with no warnings.
2. **Composition captures.** On a synthetic hex world (a coast, a lake, a class border, a slope, one patch per stratum, a river in each flow state, a burn front with scar, a cloud-shadow field, a snow gradient), a headless capture per concept matches a stored golden (1/255 on a 3×3 mean).
3. **Strata table.** Changing `hatchCoverage[s]` and `inkWeight[s]` changes only stratum `s` texels. Measured hatch coverage per stratum follows the table within DiaToonShading's tolerance (±5 points) for any table, not only CoW's.
4. **Coast and lake ink.** Coast ink sits at land fraction 0.5 and lake ink at the lake level (within 1 px). No ink crosses a flat region of one class at any zoom or tilt.
5. **Snow rule.** Snow cover is monotonic in pack, increasing with elevation and decreasing with temperature, and zero when pack is 0.
6. **Rivers.** Each flow state uses its width and colour; flow streaks move in the flow direction at that state's `riverFlowSpeed`; banks are inked wherever the river width is above 0 (so Dry draws no river unless the style gives it a width).
7. **Weather and events.** Cloud shadow darkens only where the cloud value is above its threshold. Wet darkening scales with precipitation and is zero for snow. A burn of 1.0 is emissive; a scar of 1.0 is the scar colour; a scar fading to 0 returns the ground colour.
8. **Season.** Season weights blend the grade presets (half-way equals the average of the two). Weights with all strengths at 0 are the identity. Weights are used as given, not normalised (DiaToonShading Feature 4).
9. **Custom slots.** A slot bound to each technique draws it where its value passes the threshold. An unbound or disabled slot leaves the output byte-identical to no slot texture. Values in slot `k` never affect slot `j`. Technique choice is a uniform-driven branch (all four paths compiled in, no permutations, TOON-003).
10. **Boil only on terrain ink** (TMAP-006). With boil on, ink lines change between boil frames, stepped at `boilHz`; fills never change.
11. **Parameters.** Every `HexWorldStyle` field has a sidecar range and default; out-of-range values are clamped with a warning.
12. **Neutral defaults.** The defaults render a readable map with no CoW data loaded (user taste check on a capture).
13. **Prototype parity, through CoW.** With CoW's style and visuals files loaded (CoW Map Visual Data), a side-by-side capture against the approved prototype is signed off by the user. The GDD-number checks live in that CoW feature.
14. **Data views.** Each view outputs its raw value in a documented colour mapping.
15. **Performance (Debug, ballpark).** Fragment cost at 1080p is recorded (including the custom-slot branch). If Large with everything on misses 30 fps (GDD V-1 Test 7), a perf task is queued (cheaper noise, early water exit, a cached static layer), not silently accepted.
16. **Observability.** Shader load failures log file and error (channel `HWR3D`); profile scope `HexTerrain.Material`; the style file hash is logged on `SetStyle`.
17. **Live tweak, whole style.** Setting any `HexWorldStyle` field on the backing `AttributeSet` changes the next frame's output with no rebuild, shader reload or `SetStyle` call. A sweep test sets each field of the per-stratum tables, the per-flow-state tables and the custom-slot bindings in turn, and only that stratum, state or slot changes in the capture (no cross-talk). A scan test checks that the `hexterrain/` shaders contain no look literals outside a short allowlist of structural constants.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| TMAP-003 / TOON-004 | Style in code and parameter files | Procedural composition plus `HexWorldStyle` |
| TMAP-005 | Per-cell channels | Every state comes from a channel texture |
| TMAP-006 | No boil on props | Boil applies to terrain ink only |
| TMAP-010 | Hybrid channels | Standard channels mapped here; 4 custom slots bound in the style |
| TMAP-012 | Neutral defaults only | AC 12; GDD numbers in CoW |
| TOON-001..006 | DiaToonShading | Techniques come from its includes; this feature adds no technique maths; `HexWorldStyle` is a live-tweakable `AttributeSet` with no look literals in shaders (goal 5, AC 17) |

## Open Design Questions

1. **Cost of one big fragment shader.** The prototype does many noise lookups per pixel. If it is too slow, split into a cached static layer (marks, hatch, palette at the current season) rendered to a texture when inputs change, then lit and composited per frame. Decide from AC 15.
2. **Game shader overrides.** The old spec let CoW override library shaders by name from its own root. Dropped: custom slots cover CoW-only looks without forking the material. If CoW ever needs a look the slots cannot express, the fix is a new DiaToonShading technique and a slot binding, not an override.
3. **Seasons as a concept.** Draoi treats seasons as up to 4 weighted grade presets with no names. CoW names them (Bloom, Hunt, Dry, Freeze, pending the GDD answer). Is "4" enough for a second game? It fits a `vec4`, so leave it until someone needs more.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Shader root, cook config, varying defs, `HexWorldStyle` as a live-tweakable `AttributeSet` with sidecar | AC 1, 11, 17 | Not Started | sonnet | needs the DiaToonShading cook include-path change |
| 2 | Coast, lake and border ink; strata table (ink, hatch, ambient) | AC 3, 4, 10 | Not Started | sonnet | |
| 3 | Hillshade, slope rock, form lines, contours; palette slots, season, marks, grain | AC 8 | Not Started | sonnet | |
| 4 | Snow, ocean, lake, ice, rivers by flow state | AC 5, 6 | Not Started | sonnet | |
| 5 | Weather, events, condition grade | AC 7 | Not Started | sonnet | |
| 6 | Custom slots | AC 9 | Not Started | sonnet | |
| 7 | Data views; neutral defaults capture | AC 12, 14 | Not Started | sonnet | user taste check |
| 8 | Composition goldens; prototype parity with CoW data | AC 2, 13 | Not Started | sonnet | needs CoW Map Visual Data; user signs off; goldens gated on DiaBgfx3D Feature A readback |
| 9 | Cost measurement and observability | AC 15, 16 | Not Started | haiku | |
| 10 | Live-tweak sweep and literal scan | AC 17 | Not Started | sonnet | after tasks 2 to 6 |
