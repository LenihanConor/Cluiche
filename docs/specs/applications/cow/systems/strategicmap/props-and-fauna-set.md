# Feature Spec: Strategic Map Props and Fauna Set

## Parent System
@docs/specs/applications/cow/systems/strategicmap/strategicmap.md (Feature 3)

**Gameplay Domains:** rendering
**Depends on:** Strategic Map Feature 1 (prop sets keyed by biome), DiaBgfx3D Instanced Sprites (atlas format), DraoiHexWorldRender3D Map Props and Emitters (rule format)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/props.js` (`PROP_SVG`, `PROP_PAL`, `BIOME_PROPS`, `propColours`), the prop sheet in `lookdev/index.html`

## Status
`Draft` (2026-11-20). Drafted from the approved prototype; awaiting user review.

## Summary

The art and rules for what stands on the map: the **sprite set** (trees, shrubs, grass, reeds, rocks, dead trees, jungle, hills, clouds, herds, birds), its **palette-masked atlas**, and the **per-biome prop rules**. The set is authored without an artist: Claude writes the SVGs, a DiaCLI step rasterises them into the atlas the renderer loads, and Gemini is an optional upgrade for individual sprites.

This feature owns CoW's *content* (what and where). DiaBgfx3D owns how sprites are drawn; DraoiHexWorldRender3D owns scatter mechanics.

## Problem

- The prototype's 14 sprites are Claude-written SVG embedded in JavaScript. They need a home as real assets, a build step to an atlas, and tests that they meet the palette-mask contract.
- Prop rules (which sprite where, how many) are prototype constants. They must be data per biome.
- The prototype's peak, volcano and hill sprites were judged weak; mountains are terrain now (TMAP-004). The default set must not contain them.

## Goals

1. **Source assets**: SVG files under `Cluiche/Assets/CoW/Global/Map/props/` for each sprite, following the mask contract (red = primary, green = secondary, blue = accent, black = ink, alpha = coverage), each 100 × 100 with the foot at bottom centre.
2. **Atlas build**: a DiaCLI asset handler rasterises the SVGs into one atlas PNG (grid layout) plus an `atlas.json` (index → name, cell, rigid flag, default size), with validation of the mask contract.
3. **Prop rules** per `propSet` in `map_props.json` (`cow.map_props`): rules as in DraoiHexWorldRender3D (type, per-cell density, size, jitter, slope range, wet-only, dead variant).
4. **Palettes for props**: by biome and season (from Feature 1), so a forest sprite recolours automatically.
5. **Fauna and sky sprites**: cloud puffs (translucent), herd beasts, birds, with their layer assignments.
6. **Replaceable per sprite**: a Gemini-made PNG in the same mask contract can replace any SVG without code changes (the asset forge's job; this feature defines the contract and the override order).

## Non-Goals

- The asset forge itself (prompt files, background removal, palette lock). It is a separate DiaCLI feature raised by research (C4); this feature defines what it must output.
- Prop animation beyond sway.
- 3D prop meshes.
- Buildings or any human structure (GDD: none on the map).

## Design

### Sprite contract (validated by the atlas build)

- Canvas 100 × 100 viewBox, foot point at (50, ~97).
- Colours limited to `#ff0000`, `#00ff00`, `#0000ff`, `#000000` (anti-aliased edges produce blends; the validator checks the *authored* colours only), no gradients, no text.
- Outlines are black strokes of at least 2.4 units so the ink survives minification at the smallest on-screen size (about 12 px).
- A sprite has a `rigid` flag (no sway) in `atlas.json`.

### Shipped v1 set

| Sprite | Layer | Rigid | Used by |
|---|---|---|---|
| conifer, broadleaf, shrub, jungle | cutout | no | forest, alpine, tundra, rainforest, grassland |
| grass, reeds | cutout | no | grassland, wetland |
| rock | cutout | yes | desert, alpine, volcanic, tundra |
| deadtree | cutout | no | degraded forest, volcanic |
| cloud | translucent | yes | weather fronts |
| beast | cutout | yes | herds (ambient stand-in) |
| bird | translucent | yes | ambient flocks |

**Dropped from the prototype set:** `peak`, `volcano`, `hill`. Mountains are terrain. They stay in the SVG folder behind a disabled flag in case a later look needs them.

### `map_props.json`

```json
{ "format": "cow.map_props", "version": 1,
  "propSets": {
    "forest":    [ { "sprite": "broadleaf", "perCell": 4.2, "size": 0.62, "jitter": 0.2, "deadVariant": "deadtree" },
                   { "sprite": "conifer",   "perCell": 1.6, "size": 0.62, "jitter": 0.2, "deadVariant": "deadtree" },
                   { "sprite": "shrub",     "perCell": 0.8, "size": 0.40, "jitter": 0.2 } ],
    "grassland": [ { "sprite": "grass", "perCell": 3.0, "size": 0.42 }, { "sprite": "shrub", "perCell": 0.7, "size": 0.42 }, { "sprite": "broadleaf", "perCell": 0.35, "size": 0.62 } ]
  } }
```

Sprites reference `atlas.json` names; prop sets are referenced from Feature 1's `propSet`. Completeness: every `propSet` named in `map_visuals` exists; every sprite named exists in the atlas.

### Asset build

`dia pipeline` build-assets runs a new `map_props` handler: SVG files → rasterise at 128 × 128 per cell with a free CLI rasteriser (resvg) → pack the atlas (4 columns) → `atlas.png` + `atlas.json` + a contact sheet PNG for the taste check. If the rasteriser is not installed, `dia env verify` reports it and the build step fails with the install command (not silently skipped).

## Acceptance Criteria

1. **Contract.** The validator accepts the shipped SVGs and rejects an SVG with a gradient, a text element, an off-contract colour, or a missing foot region; the rejection names the file and rule.
2. **Atlas build.** Building produces an atlas whose cell count equals the sprite count (rounded up to the grid), an `atlas.json` with every sprite, and a contact sheet; a rebuild with unchanged inputs gives byte-identical outputs.
3. **Mask decode.** Every cell, decoded with a known palette, has pixels only in the primary/secondary/accent/ink families (no stray colours beyond anti-aliasing blends), measured by a histogram test.
4. **Minification.** Each sprite rendered at 12 px height retains at least 15% ink coverage on its outline (an ink check on the downsampled cell), so it stays legible at strategic zoom.
5. **Rules complete.** Every `propSet` referenced by Feature 1 resolves; every sprite named in a rule exists; an unknown name fails the load with the rule and name.
6. **No mountain sprites by default.** The shipped rule sets contain no `peak`, `volcano` or `hill` sprites.
7. **Dead variants.** Every rule with `deadVariant` names an existing sprite.
8. **Recolour.** For each biome and season, the three palette slots applied to a sprite give colours inside the GDD palette family for that biome (hue within 25° of the biome's primary/secondary/accent, or a documented neutral such as rock grey).
9. **Replace by override.** Placing a PNG named `<sprite>.png` in a `props_override/` folder replaces that cell in the atlas and passes the same mask-contract check; a failing override is reported and the SVG is used.
10. **Contact sheet.** A contact-sheet PNG of every sprite on the biome palettes is produced for the user's taste check (one image per feature).
11. **Observability.** Build logs sprite count, atlas size and warnings; runtime logs the atlas and rules hash on load (channel `MAP`).

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| TMAP-003 | No per-cell or per-state images | Sprites are shared and recoloured; the set has about 14 images in total |
| TMAP-004 | Mountains are terrain | Peak/volcano/hill are not in the default rules (AC 6) |
| AESTHETIC (GDD) | No human structures, no text on the map | No such sprites; validator forbids text |
| AD-002 | Four-tier | The sprite contract and loader are Dia (DiaBgfx3D); sprites and rules are CoW |
| Feedback: use existing packages first | Extend the asset pipeline handlers | A new handler beside `mesh3d.py`/`texture.py`; no new package |
| AD-001 | DiaCLI changes need sign-off | **The `map_props` handler is a DiaCLI change: sign-off required** |

## Open Design Questions

1. **SVG authoring quality.** Claude-written SVG can be crude (the prototype's hills read as hay mounds). The set is judged at the contact sheet. If trees look weak, the asset forge's Gemini route for specific sprites is the planned upgrade, using the same mask contract.
2. **Rasteriser choice.** resvg (a single Rust binary) is easy to install and deterministic. Inkscape's CLI is heavier. Confirm resvg with `dia env`.
3. **Atlas size and extension.** 4 × 4 cells of 128 px is 512 × 512 (small). Adding sprites is cheap; the grid grows by rows.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Move prototype SVGs into `props/`, contract validator | AC 1, 3 | Not Started | sonnet | |
| 2 | `map_props` DiaCLI handler + atlas + contact sheet | AC 2, 4, 10 | Not Started | sonnet | AD-001 |
| 3 | `map_props.json` schema, rules, completeness | AC 5, 6, 7 | Not Started | sonnet | needs Feature 1 |
| 4 | Recolour palette family check | AC 8 | Not Started | sonnet | |
| 5 | Override folder | AC 9 | Not Started | sonnet | |
| 6 | Observability | AC 11 | Not Started | haiku | |
