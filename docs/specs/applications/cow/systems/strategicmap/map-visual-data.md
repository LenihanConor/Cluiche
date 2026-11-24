# Feature Spec: Strategic Map Visual Data

> **Strata (2026-11-21, not yet built):** Depth zones become data-defined strata (@docs/specs/features/draoi/generic-strata.md, Approved, not yet built). The `zones` table in `map_style.json` stays keyed by CoW's stratum names (FirstShores, WildReaches, AncientInteriors) from `strata.json`; it follows the rename when the feature lands.

## Parent System
@docs/specs/applications/cow/systems/strategicmap/strategicmap.md (Feature 1)

**Gameplay Domains:** rendering
**Depends on:** S-1 Biome Registry (@docs/specs/applications/cow/systems/s1-worldgeneration/biome-registry.md; the ids and tags), DiaValueSchema (sidecars, `valuesFrom`, `keyEnum`, `data.incomplete-map`), DiaAttribute Parameter Files
**Consumed by:** DraoiHexWorldRender3D (`HexWorldVisuals`, `HexWorldStyle`), DraoiHexWorldRelief (relief weights), Strategic Map Features 2–5
**Design source:** GDD `03_VisualBible/ColourPaletteAndLighting.md` (Parts 1, 2, 3, 7), `AestheticStyleCore.md` Parts 4–5, `ShapeLanguageAndAnimation.md` Part 9
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/app.js` (`BASE_PAL`, `seasonalPalette`, state defaults), `js/shaders.js` (`gradeOne`, zone weights, hatch coverage)

## Status
`Approved` (2026-11-24, user, after spec review). Drafted 2026-11-20 from the approved prototype. The three GDD naming conflicts (Open Design Questions 1–3) were **decided by the user 2026-11-24** as proposed; the GDD session applies the matching text from `docs/research/tile_renderin_art_pipeline/gdd-foldback.md`.

### Amendment 2026-11-23: layer split (user)

The renderer is now DraoiHexWorldRender3D (Draoi), built on DiaToonShading (Dia). `TerrainMapStyle` and `TerrainMapVisuals` are now `HexWorldStyle` and `HexWorldVisuals`; `ToTerrainMapVisuals` becomes `ToHexWorldVisuals`. Draoi ships neutral defaults only (TMAP-012), so **every GDD number lives here**, and the rendered checks that used to sit in the old Style Shader Library move here as well:

- **AC 13, rendered strata coverage.** With CoW's files loaded, measured hatch ink coverage on a synthetic world is within the GDD bands: First Shores 10–20%, Wild Reaches 30–50%, Ancient Interiors 60–80%.
- **AC 14, rendered ink weight.** Coast and border ink widths by stratum are in the ratio 1 : 1.2 : 1.5 (±10%).
- **AC 15, rendered season grade.** For a known grey input, Bloom, Dry and Freeze produce the GDD tints (Screen and Multiply as specified, strengths 8%, 6%, 12%); Hunt leaves it unchanged; half-way between two seasons equals the average of the two.
- **AC 16, rendered condition bands.** Saturation at health 0.15 / 0.5 / 0.9 falls inside the GDD bands (Collapsed −40–60%, Stressed −15–25%, Thriving up to +15%); a barren cell shows the bare blend.
- **AC 17, relief weights.** The relief weights (`Mountain` 1.0, `Peak` 1.45, `Volcano` 1.3, `Hills` 0.22, `Highland` 0.18, `Cliff` 0.15, from the prototype) live in `map_visuals.json` and reach DraoiHexWorldRelief unchanged.
- **Custom slots.** `map_style.json` gains the 4 custom slot bindings (`custom.slot[k] = { technique, colour, threshold, falloff, enabled }`). Which CoW states use them (for example magic snow) is decided with the GDD session; until then all slots are disabled.
- **Added 2026-11-24 (spec review against the Draoi approvals).** (a) `BiomeVisual` gains a `wet` flag (wetland, rainforest and the like), which Draoi's prop rule `needsWet` reads. (b) `map_style.json` gains a `props` group holding the numbers Draoi defaults to neutral: the condition-to-density curve (the prototype's three segments), `deadBelow` 0.22, `deadChance` 0.6, `scatterRadius`, `densityScale` (the prototype's values; Draoi's own defaults are flat and off). (c) **Season palettes are blended here, on the CPU:** `ToHexWorldVisuals(out, seasonWeights)` produces one blended palette table (about 13 classes × 3 colours), re-pushed when the weights change, and the same blend fills the sprite palette table (DiaBgfx3D `SetPaletteTable`); Draoi does not blend palettes. AC 4 stands. (d) Every `map_style.json` number is a live-tweakable attribute with a sidecar (TOON-006, TMAP-013), including the new `bake.*` and `relief.*` groups (Feature 4).

Additional task: 8 rendered GDD checks, AC 13–17 (sonnet; needs DraoiHexWorldRender3D Feature 2).

## Summary

Every CoW-specific visual fact in one set of schema'd data files under `Global/Map/`, so the renderer never names a biome, a season or a colour in code:

1. **`map_visuals.json`** (`cow.map_visuals`): per biome, its palette (primary/secondary/accent), seasonal palette overrides, ground-mark kind, prop set and relief class weights.
2. **`map_style.json`** (`cow.map_style`): the default `HexWorldStyle` (ink, hatch, toon, haze, tilt-shift, grade strengths…) and the depth-zone and ecology-band tables.
3. **`map_presets/*.json`** (`cow.map_preset`): named style overrides (Feature 5).

All three have DiaValueSchema sidecars with ranges, defaults and descriptions. Completeness against the Biome Registry is checked by the CLI.

## Problem

- The prototype holds the GDD palette, season callouts, zone weights and grade tokens as JavaScript literals. They belong in data the Strategic Map owns and a designer can edit.
- The Biome Registry deletes the `Biome` enum, so a palette table keyed by enum or index would break. It must key by registry id.
- The GDD's numbers (ink coverage by zone, outline weights, season tints, ecology saturation bands) must live in exactly one place and be checkable against the GDD.

## Goals

1. **One palette row per registry biome**, with seasonal overrides only where the GDD defines them (Grassland, Wetland, Tundra, Rainforest, Volcanic), and inherit otherwise.
2. **Depth-zone table**: ink weight multiplier, hatch coverage, ambient saturation and lightness modifier for First Shores, Wild Reaches, Ancient Interiors, from the GDD numbers.
3. **Season table**: names, tint colour, strength and blend mode (Screen/Multiply/None), cross-fade window, per-season ambient.
4. **Ecology band table**: health thresholds and saturation multipliers for Collapsed, Stressed, Stable, Thriving, plus bare-blend start.
5. **Relief weights** per terrain class (the prototype's `MOUNTAIN_WEIGHT`).
6. **Style defaults** reproducing the approved prototype.
7. **Completeness rule**: every non-water registry biome has a `map_visuals` row, or the CLI fails the build; unknown ids in the file are errors.
8. **Hot reload** on file change in dev builds (catalogue + `PathStore` + `FileWatcher`, as the world-sim kit does).

## Non-Goals

- Authoring prop art or sprite atlases (Feature 3).
- Per-cell overrides or per-world palettes.
- Colour space beyond sRGB hex; no tone mapping.
- Changing the GDD's palette values. They are transcribed; changes go through the GDD session.

## Design

### `map_visuals.json`

```json
{ "format": "cow.map_visuals", "version": 1,
  "biomes": {
    "Biome.Grassland": {
      "palette": { "primary": "#D4B84A", "secondary": "#8DB558", "accent": "#7AB8C8" },
      "accentNearRiver": true,
      "seasons": { "Bloom": { "primary": "#A8C858" }, "Dry": { "primary": "#B07830" }, "Freeze": { "primary": "#A89040" } },
      "marks": "grass_ticks", "propSet": "grassland" },
    "Biome.TemperateForest": { "palette": { "primary": "#4A7A3C", "secondary": "#2E5228", "accent": "#8A6E3A" },
      "marks": "tree_dots", "propSet": "forest" }
  },
  "water": { "shallow": "#4AB8C8", "deep": "#2A7888",
             "lakeBySeason": { "Bloom": "#38B4C8", "Hunt": "#4A9EA8", "Dry": "#8A7850", "Freeze": "#6A8878" } },
  "relief": { "Mountain": 1.0, "Peak": 1.45, "Volcano": 1.3, "Hills": 0.22, "Highland": 0.18, "Cliff": 0.15 }
}
```

- Keys are Biome Registry ids (`valuesFrom` biomes); terrain class keys are the TerrainType names (kept in code, SD-S1).
- `seasons` entries override any of `primary`/`secondary`/`accent` for that season; missing means inherit.
- `marks` is an enum (`grass_ticks`, `tree_dots`, `reed_strokes`, `rock_specks`, `lava_veins`, `none`). `propSet` keys Feature 3's prop rule table.

### `map_style.json`

```json
{ "format": "cow.map_style", "version": 1,
  "zones": { "FirstShores":      { "inkWeight": 1.0, "hatchCoverage": 0.15, "satMod": 0.0,   "lightMod": 0.0   },
             "WildReaches":      { "inkWeight": 1.2, "hatchCoverage": 0.40, "satMod": -0.05, "lightMod": 0.0   },
             "AncientInteriors": { "inkWeight": 1.5, "hatchCoverage": 0.70, "satMod": -0.10, "lightMod": -0.08 } },
  "seasons": [ { "name": "Bloom",  "tint": "#F0D870", "strength": 0.08, "blend": "Screen",   "satMod": 0.10  },
               { "name": "Hunt",   "tint": null,      "strength": 0.0,  "blend": "None",     "satMod": 0.0   },
               { "name": "Dry",    "tint": "#E0A840", "strength": 0.06, "blend": "Screen",   "satMod": -0.08 },
               { "name": "Freeze", "tint": "#A8C4D8", "strength": 0.12, "blend": "Multiply", "satMod": -0.12 } ],
  "seasonBlendWindow": 0.30,
  "ecologyBands": { "stressedBelow": 0.66, "collapsedBelow": 0.33,
                    "saturation": { "Collapsed": [0.40, 0.60], "Stressed": [0.75, 0.85], "Stable": 1.0, "Thriving": 1.15 },
                    "bareBlendStart": 0.42 },
  "style": { /* HexWorldStyle defaults: ink, hatch, toon, form lines, haze, tilt-shift, boil, camera curves */ } }
```

Numbers are the GDD's (hatch 10–20 / 30–50 / 60–80% midpoints, outline 1× / 1.2× / 1.5×, zone ambient −5% / −10% saturation and −8% lightness, season tints 8% / 6% / 12%, ecology desaturation bands).

### Interface

```cpp
namespace CoW::Map {
    struct BiomeVisual { uint32_t palette[3]; uint32_t seasonOverride[4][3]; uint8_t seasonMask[4]; MarkKind marks; PropSetId propSet; bool accentNearRiver; };
    class MapVisuals {
    public:
        bool Load(const BiomeRegistry&, const char* path, std::string* err);   // validates completeness; atomic swap
        const BiomeVisual& Get(BiomeIndex) const;
        void ToHexWorldVisuals(Draoi::HexWorldRender3D::HexWorldVisuals& out, const float seasonWeights[4]) const;   // blends seasons
        uint64_t Hash() const;
    };
    struct MapStyleFile { /* zones, seasons, bands, HexWorldStyle */ bool Load(const char*, std::string*); bool Save(const char*, std::string*) const; uint64_t Hash() const; };
}
```

Pure CoW-Shared code (no GPU), unit-testable.

## Acceptance Criteria

1. **Complete.** Loading against a registry where a non-water biome lacks a row fails with the biome id named; a row for an unknown id fails with that id; a valid file loads.
2. **Palette values.** The shipped file's eight GDD biome palettes equal the GDD hex tokens exactly (a test table transcribed from `ColourPaletteAndLighting.md` Part 1), plus the coastal water base `#4AB8C8` and deep `#2A7888`.
3. **Season overrides.** Grassland Bloom primary is `#A8C858`, Dry `#B07830`, Freeze `#A89040`; Wetland Bloom secondary `#38B4C8`, Dry `#8A7850`, Freeze `#6A8878`; Tundra Bloom `#8AB890`, Freeze `#E4EEF2`; Volcanic Freeze secondary `#8A1A08`, primary `#1A1418`. A biome with no override inherits.
4. **Season blend.** `ToHexWorldVisuals` with weights `{0.5, 0.5, 0, 0}` gives the channel-wise mean of the Bloom and Hunt palettes; with `{0,1,0,0}` gives the base palette.
5. **Zone table.** Zone rows hold the GDD numbers (AC table test); a value outside the sidecar range fails validation.
6. **Ecology bands.** Band thresholds are ordered (`collapsedBelow < stressedBelow`), saturation ranges lie inside the GDD bands, and the lookup `SaturationFor(health)` is continuous and monotonic non-decreasing across the bands.
7. **Style defaults parity.** `MapStyleFile` defaults, applied through `cow.map.style.set`, reproduce the approved prototype (side-by-side capture signed off by the user, shared with DraoiHexWorldRender3D Hex Terrain Material AC 13).
8. **Round-trip.** `Save` then `Load` gives an identical `Hash()`; a file with an unknown key is a warning and the key is preserved (hand-edited files survive).
9. **CLI.** `dia validate schema` passes on the shipped files; `data.incomplete-map` fires when a biome is missing (a negative fixture).
10. **Hot reload.** Editing the file in a dev build swaps visuals atomically between frames, and a bad edit keeps the old data and logs an error.
11. **Succession biomes.** `Scrub`, `BurnedAshland` and `DesertifiedPlain` have rows (derived palettes, ODQ 3) and load like any other.
12. **Observability.** Log channel `MAP`; info on load with row counts and hash; error on validation failure; metric `map.visuals_hash`; health Failing when the last load failed and no data is loaded.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| SM-006 | Biome visuals key off registry ids and tags, never the enum | Rows keyed by registry id; `valuesFrom` the registry |
| SM-002 | GDD look rules stand unchanged | All numbers transcribed; AC 2, 3, 5, 6 test them |
| SD-S1 / world-sim D1–D7 | Sidecar owns ranges and defaults; `ParameterFile` strict load | Files are parameter files with sidecars; completeness by `keyEnum` |
| AD-003 | GDD owns design intent | Values come from the Visual Bible; changes via the GDD session |
| PD-001 | StringCRC ids | Biome ids resolved through the registry; season names are enum values |

## Open Design Questions (GDD conflicts; 1–3 decided 2026-11-24)

1. **DECIDED 2026-11-24 (user): biome set.** The palette doc defines 8 biomes. `TileRenderingPipeline`/V-1 name 12 Phase-2 fantasy biomes. S-1's registry has 10 generated plus 3 succession biomes. Proposed: the registry is the truth. The 8 palette biomes map onto the generated land biomes (Grassland, Temperate Forest, Wetland, Desert, Alpine, Tundra, Rainforest, Volcanic), Ocean and Lake use the water table, and the 12 fantasy names are dropped. Needs GDD fold-back.
2. **DECIDED 2026-11-24 (user): season names.** Bloom/Hunt/Dry/Freeze (palette doc) against Spring/Summer/Autumn/Winter (pipeline and V-1 docs). Proposed: Bloom/Hunt/Dry/Freeze (the palette doc is the locked colour source). The data table uses those names, and the code refers to seasons by index 0..3.
3. **DECIDED 2026-11-24 (user): palettes for succession biomes.** `Scrub`, `BurnedAshland`, `DesertifiedPlain` have no GDD palette. Proposed (flag as provisional): Scrub = Grassland primary desaturated 25% with Desert accent; BurnedAshland = Volcanic primary with Alpine secondary; DesertifiedPlain = Desert palette. Confirmed by the user 2026-11-24; the derived palettes stand until the GDD supplies its own.
4. **Ecology state names.** Four (Collapsed / Stressed / Stable / Thriving, sort-layers doc) against five (+ Recovering / Depleted / Barren, V-1 doc). The map uses a continuous health value with four bands, so this feature needs only the band thresholds; the label set is an A1/GDD matter.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Schemas and sidecars for the three file types | AC 9 | Not Started | sonnet | needs Biome Registry |
| 2 | `MapVisuals` load/validate/blend | AC 1, 4, 8 | Not Started | sonnet | |
| 3 | Transcribe GDD tables into the shipped files; table tests | AC 2, 3, 5 | Not Started | sonnet | |
| 4 | Ecology bands and zone lookups | AC 6 | Not Started | sonnet | |
| 5 | `MapStyleFile` load/save/hash and parity capture | AC 7, 8 | Not Started | sonnet | user signs off parity |
| 6 | Succession biome rows | AC 11 | Not Started | haiku | ODQ 3 answered 2026-11-24 |
| 7 | Hot reload and observability | AC 10, 12 | Not Started | sonnet | |
