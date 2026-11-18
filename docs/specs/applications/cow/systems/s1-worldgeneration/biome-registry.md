# Feature Spec: S-1 Biome Registry

**Parent:** @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md (Feature 10)
**Status:** `Approved` (2026-11-18, user)
**Research:** @docs/research/worldsim_data_driven/summary.md (§0: A2a, A2b, A11, D5–D7)
**Depends on:** @docs/specs/applications/dia/systems/diaattribute/parameter-files.md (Parameter Files; build it first. The terrain profile and the climate-rule removal edit S-1's parameter sidecar.)

## Summary

What a biome *is* moves from code into one data file. `Global/World/biomes.json` (`cow.biomes`), owned by S-1, holds every biome's:
- identity, name, colour and tags;
- classification rules: zones, forbidden neighbours, climate envelope, pin and plateau role;
- roles in code: `ocean`, `lake`, `fallback`.

Code never names a biome. It asks the registry for roles, tags and rules, and holds a runtime `BiomeIndex`. The `Biome` enum is deleted. Adding a biome is a data change.

The same pass makes the per-terrain-type table (navigation cost and passability, plus the new soil, orographic and runoff factors) S-1 parameter maps.

This **supersedes the 2026-11-13 "one enum, not a StringCRC set" decision** (`a1-ecology.md:81`). There is still one list, one palette and one name table; the list now lives in data.

## Problem

A biome's facts are spread across code and four systems:
- **Identity is spelled four ways:** `Biome.Grassland` (`BiomeId`, used by saves and A1/A4/A5), `Grassland / Steppe` (`BiomeName`), `grasslandSteppe` (the S-1 stage sidecar, which also holds the colours), and `GrasslandSteppe` (the enum).
- **Zone validity and the 7 forbidden pairs are switches** in `BiomeTypes.cpp`.
- **Climate targets exist for only 5 biomes**, as 20 named rules (`kT0_Grass`…).
- **The 5-climate-biome list is hard-coded twice:** `BiomeClassify.cpp:25` and `WorldGenSession.cpp:1677`.
- **Pins, the plateau rule and region repair name biomes directly.**

The enum has about 340 references, about 290 of them in tests. A new biome means an enum change, switch edits, new named rules, code changes in two classifier lists, and rows in three other systems' files.

## Goals

1. One record per biome holds every fact about it, and nothing else does (one fact, one place, in data).
2. Production code refers to biomes only through roles, tags and registry rules.
3. A new biome needs no code change, which a test proves.
4. The migration changes no generated world: goldens stay bit-identical.
5. Every per-biome table in another system is keyed by registry id and checked for completeness by the CLI and by `ParameterFile`.

## Non-Goals

- **A1, A4 and A5's own per-biome tables.** Their content and migration is world-sim spec 6. This feature provides the registry, the `keyRef` target and the completeness rule.
- **Seasonal temperature ownership** (A1 owns it, A4's `baseline_table` becomes a fallback): also spec 6.
- **The `TerrainType` enum.** The classification ladder produces terrain types, so they stay in code. Only their *profile values* become data.
- **Reloading the registry mid-world** (see Open Design Question 1).
- **New biomes or rebalancing.** Rows ship with today's values.

## Design

### Registry file

`Cluiche/Assets/CoW/Global/World/biomes.json`:

```json
{ "format": "cow.biomes", "version": 1,
  "biomes": [
    { "id": "Biome.Ocean", "name": "Ocean", "colour": "#1f4f8f", "description": "Open sea.",
      "tags": ["water", "aquatic"], "generated": true, "zones": ["None"], "roles": ["ocean"],
      "pin": { "rule": "water_ocean", "priority": 4 } },
    { "id": "Biome.Grassland", "name": "Grassland / Steppe", "colour": "#a6c04a", "description": "Dry open plains.",
      "tags": ["land", "open", "fuel"], "generated": true, "zones": ["FirstShores", "WildReaches"],
      "climate": { "t0": 0.60, "m0": 0.35, "wt": 0.25, "wm": 0.20 }, "roles": ["fallback"] },
    { "id": "Biome.Scrub", "name": "Scrub", "colour": "...", "tags": ["land", "fuel"], "generated": false, "zones": [] }
  ],
  "forbidden": [ ["Biome.Volcanic", "Biome.Wetland"], ["Biome.Volcanic", "Biome.Rainforest"] ] }
```

The sidecar `Global/World/biomes.schema.json` (`for: cow.biomes`) defines each field as a record entry (`biomes[].id`, …). In it:
- `id` is `unique` and `required`.
- `tags` is an `enum[]` over a declared vocabulary: `land`, `water`, `aquatic`, `wet`, `cold`, `hot`, `forest`, `open`, `fuel`, `mountain`, `volcanic`, `succession`. The shipped rows set the values; adding a tag is a sidecar edit.
- `zones` is an `enum[]` over the `DepthZone` ids.
- `forbidden` lists unordered pairs, of type `ref` to `cow.biomes`.
- `pin.rule` is an enum: `water_ocean`, `water_lake`, `vent`, `alpine`, `wetland`.
- `plateau` is an enum: `wet` or `dry`.
- `roles` is an `enum[]` over `ocean`, `lake`, `fallback`.

The shipped rows are today's 13, **in today's enum order**, with today's values. The forbidden pairs are today's 7, and the climate envelopes are today's 20 `kT0/kM0/kWT/kWM` values.

**Row order is significant.** It is the runtime index and the classification tie-break ("lowest index"). Saves store ids, so they are unaffected. Reordering rows changes generated worlds; goldens catch it, and the generator version is bumped. Appending a row changes no existing world. The sidecar description says this.

### `BiomeRegistry` (`Cluiche/CoW/Shared/World/BiomeRegistry.h/.cpp`, CoW-Shared)

```cpp
namespace CoW::World {
    using BiomeIndex = uint8_t;  constexpr BiomeIndex kNoBiome = 0xFF;
    enum class BiomeRole : uint8_t { Ocean, Lake, Fallback };
    enum class PinRule   : uint8_t { WaterOcean, WaterLake, Vent, Alpine, Wetland };
    struct ClimateEnvelope { float t0, m0, wt, wm; };

    class BiomeRegistry {
    public:
        bool Load(const Dia::Core::FilePath& path, RegistryError& err);   // JsonDefinitionLoader; strict
        uint32_t   Count() const;
        BiomeIndex Find(Dia::Core::StringCRC id) const;                   // kNoBiome if unknown
        BiomeIndex ResolveOrFallback(Dia::Core::StringCRC id) const;     // logs once per unknown id
        Dia::Core::StringCRC Id(BiomeIndex) const;  const char* IdText(BiomeIndex) const;
        const char* Name(BiomeIndex) const;         uint32_t Colour(BiomeIndex) const;  // 0xRRGGBB
        bool HasTag(BiomeIndex, TagId) const;       uint64_t TagMask(BiomeIndex) const;
        TagId FindTag(Dia::Core::StringCRC tag) const;
        BiomeIndex Role(BiomeRole) const;
        bool IsGenerated(BiomeIndex) const;
        bool IsValidInZone(BiomeIndex, DepthZone) const;                 // O(1), bitmask
        bool AreForbidden(BiomeIndex, BiomeIndex) const;                  // O(1), bit matrix, symmetric
        const ClimateEnvelope* Climate(BiomeIndex) const;                 // null if none
        const BiomeIndex* ClimateBiomes(uint32_t& count) const;           // rows with a climate block, row order
        const BiomeIndex* PinsByPriority(uint32_t& count) const;          // highest priority first
        PinRule  Pin(BiomeIndex) const;
        BiomeIndex Plateau(bool wet) const;                               // kNoBiome if no row has the role
        uint64_t Hash() const;                                            // content hash (provenance; ODQ 3)
    };
}
```

- **Strict load** (D5: fail loudly, no fallback). `Load` fails for each of the following, with `err` naming the row index, id and field:
  - a wrong format or version, or sidecar validation failing;
  - a duplicate id;
  - an unknown tag or zone;
  - a forbidden pair naming an unknown id, a pair listed twice (in either order), or a biome paired with itself;
  - any role held by zero or more than one row;
  - two pins with the same priority, or a pin with an unknown rule;
  - a climate width ≤ 0;
  - two rows holding the same plateau value;
  - more than 254 rows.

  On failure, `WorldGenerationModule` reports health Error and generation refuses to run.
- **Ownership.** `WorldGenerationModule` owns the registry and loads it at module start through its catalogue alias (`json.biomes`). It is immutable after load. Other SimPU modules reach it through `ModuleRef<WorldGenerationModule>`: `GetBiomeRegistry()` returns a `const&`. Off-SimPU readers get ids and indices in snapshots, as they do today.
- **Unknown ids** (import, consumer tables). `ResolveOrFallback` returns `Role(Fallback)` and logs a warning on channel `WGEN` once per unknown id, per registry instance.

### S-1 migration

| Today | After |
|---|---|
| `enum class Biome` (TerrainEnums.h), `kBiomeCount`, `kGeneratedBiomeCount` | Deleted. `Cell::biome` is a `BiomeIndex`. Per-biome arrays are sized by `Count()`. |
| `BiomeId`, `BiomeIdString`, `BiomeName`, `BiomeFromId`, `IsGeneratedBiome` | `Id`, `Name`, `Find`, `IsGenerated` |
| `IsBiomeValidInZone`, `AreBiomesForbidden` (switches, `IsPair`) | `IsValidInZone`, `AreForbidden` |
| Pins hard-coded (water 4, volcanic 3, alpine 2, wetland 1) | `PinsByPriority()`. Each pin's **terrain test** stays in code, keyed by `PinRule`. The biome each rule produces and its priority are data. |
| `kClimateBiomes[]` ×2, `BiomeClimateScore` switch over the 5 named rule sets | `ClimateBiomes()` + `Climate(b)`. The 20 `kT0_*/kM0_*/kWT_*/kWM_*` rows leave S-1's parameters. |
| Plateau `Rainforest : DesertBasin` | `Plateau(wet)`, `Plateau(!wet)` |
| Region repair `Biome::Ocean` / `Biome::Lake` | `Role(Ocean)` / `Role(Lake)` |
| Wetland "last resort" and Alpine fallback consideration | The registry's `wetland` and `alpine` pin rows |
| Biome name table in `WorldJson.cpp` | `IdText`. The file format is unchanged; import uses `ResolveOrFallback`. |
| Biomes layer `Biome::Count` category count | `Count()` |

Tests use a fixture registry (`Cluiche/CoW/Shared/World/Testing/BiomeTestRegistry.h`) that loads the shipped file, plus `B("Biome.Grassland")`-style lookup. A second fixture adds an extra biome for the data-only proof.

### Palette and labels: `valuesFrom` (DiaValueSchema)

An `enum` entry may declare where its values come from, instead of listing them:

```json
{ "key": "biomeIndex", "kind": "field", "type": "enum", "label": "Biome",
  "valuesFrom": { "format": "cow.biomes", "records": "biomes", "id": "id", "label": "name",
                  "colour": "colour", "description": "description" } }
```

- **Runtime.** `ValueSchemaModule` loads the named data file (by format, through the catalogue) and fills the entry's values in record order before publishing. A missing file or field is a module Error naming the entry.
- **Offline.** The CLI resolves it the same way it resolves `ref` (it already indexes data documents by format). T-2's spec is amended to match. Conformance fixtures cover it.
- **S-1's stage sidecar.** Its `biomeIndex` entry uses `valuesFrom`, so the 13 hand-written values with `grasslandSteppe`-style ids are deleted. The legend shows registry names and colours, in index order, which matches `BiomeIndex`.

### Map completeness: `data.incomplete-map`

A `map` entry with a `keyRef` must list **every** key that `keyRef` resolves to in its schema `default`. The CLI reports `data.incomplete-map`, naming the missing keys, and `ParameterFile` fails to load (Parameter Files AC 6 already does this). Packs remain sparse and are not checked for completeness. Conformance fixtures cover both. Any system's per-biome table (`keyRef: "cow.biomes/biomes[].id"`) is therefore guaranteed to cover every registry biome, both in the CLI and at runtime.

### Terrain profile

S-1's parameter sidecar (from Parameter Files; today's `terrain_rules.json` successor) gains maps keyed by `TerrainType` id. The key source is the `TerrainType` enum's id list, through `IMapKeySource`.

| Map | Kind | Default | Notes |
|---|---|---|---|
| `terrain.passable` | bool | today's `terrain_navigation.json` | |
| `terrain.cost` | float, [1, 16] | today's values | |
| `terrain.soil_depth_mult` | float, [0, 4] | 1.0 (Mountain 0.4, Highland 0.7, Hills 0.85, Peak/Cliff 0.2) | **Provisional**, S-2.6 tunes it. A5 consumes it (spec 6). |
| `terrain.orographic_factor` | float, [0, 4] | 1.0 | **Provisional.** A4 and S-1 Stage 13 may consume it (spec 6). |
| `terrain.runoff` | float, [0, 1] | 0.5 (Peak/Cliff 0.9, Lowland 0.4) | **Provisional.** A5 consumes it (spec 6). |

`terrain_navigation.json` is deleted. `TerrainNavigationTable` reads `passable` and `cost` from the bound S-1 parameter struct. Its behaviour is identical.

## Acceptance Criteria

### Registry data
1. **Shipped data.** `Global/World/biomes.json` (`cow.biomes` v1) and `biomes.schema.json` ship today's 13 biomes in today's enum order with today's values (ids, names, colours from the S-1 stage sidecar, zones, 7 forbidden pairs, 5 climate envelopes, 5 pins with priorities, plateau wet/dry, and the roles ocean, lake and fallback = Grassland). `dia validate schema` passes on both.
2. **Strict load.** `BiomeRegistry::Load` fails, naming the row, id and field, for each fault listed in Design → Strict load. `WorldGenerationModule` then reports health Error and refuses to generate.
3. **API.** `Find`, `Id`/`IdText`, `Name`, `Colour`, `HasTag`/`TagMask`, `Role`, `IsGenerated`, `IsValidInZone`, `AreForbidden` (symmetric), `Climate`, `ClimateBiomes`, `PinsByPriority`, `Pin`, `Plateau` and `Hash` return the shipped data. `IsValidInZone` and `AreForbidden` are O(1).
4. **Unknown ids.** `ResolveOrFallback` returns the fallback biome for an unknown id, and logs one warning per distinct unknown id.

### S-1 migration
5. **Enum deleted.** `enum class Biome`, `kBiomeCount`, `kGeneratedBiomeCount`, `BiomeIdString`, `BiomeName`, `BiomeFromId`, `IsBiomeValidInZone` and `AreBiomesForbidden` are deleted. `Cell::biome` is a `BiomeIndex`. No production file under `Cluiche/CoW/` names a specific biome (a grep test fails on `"Biome\.[A-Z]"` literals outside `Assets/` and `Testing/`).
6. **Classification reads the registry.**
   - Pins run in registry priority order.
   - Climate scoring covers every row with a `climate` block.
   - The plateau rule uses `Plateau(wet)`, and region repair uses `Role(Ocean)` and `Role(Lake)`.
   - Both 5-biome lists are gone.
   - The 20 `kT0_/kM0_/kWT_/kWM_` parameters are removed from S-1's parameter sidecar and bindings.
7. **No behaviour change.** Ties break by registry row order. All S-1 layer goldens (4 presets × seeds 1–3, Small, every layer) are **bit-identical**. All S-1 tests pass, using the fixture registry and id lookup.
8. **World files.** The export format is unchanged. Import resolves ids through `ResolveOrFallback`: an unknown id imports as the fallback biome, with one warning per id. Export(Import(world)) round-trips for known ids.
9. **Data-only proof.** With a fixture registry that appends one extra climate biome (envelope placed between two others), classification places that biome on cells whose climate best matches it. No code changes; only the fixture file differs.

### Palette
10. **`valuesFrom`.** An enum entry can declare `valuesFrom`, and the runtime loader and the CLI fill its values from the named data file in record order. A missing file, record path or field is an error naming the entry. S-1's `biomeIndex` layer entry uses it, its 13 hand-written values are deleted, and the Biomes legend shows registry names and colours. T-2's `plugin-shell-files-and-schema.md` and `table-and-form-editors.md` are amended. Shared conformance fixtures cover these cases.

### Coverage
11. **Map completeness.** A `map` entry with a `keyRef` whose schema `default` omits a resolvable key is reported as `data.incomplete-map` by the CLI (naming the missing keys), and fails `ParameterFile` load. A sparse pack over the same map is valid. Conformance fixtures cover both, and C++ and the CLI agree.

### Terrain profile
12. **Terrain profile maps.**
    - S-1's parameter sidecar holds `terrain.passable`, `terrain.cost`, `terrain.soil_depth_mult`, `terrain.orographic_factor` and `terrain.runoff`, keyed by every `TerrainType` id (complete per AC 11). The last three carry the provisional defaults above.
    - `terrain_navigation.json` is deleted and the navigation table reads the bound values.
    - The reachability and navigation tests pass unchanged, and layer goldens are bit-identical.

## Files Touched

| File | Change |
|---|---|
| `Cluiche/CoW/Shared/World/BiomeRegistry.h/.cpp`, `Testing/BiomeTestRegistry.h` | **New** |
| `Cluiche/Assets/CoW/Global/World/biomes.json`, `biomes.schema.json` | **New** |
| `Cluiche/CoW/Shared/WorldGeneration/TerrainEnums.h`, `BiomeTypes.h/.cpp` | Biome enum and functions deleted (`BiomeTypes` removed or reduced to the `BiomeIndex` alias) |
| `BiomeClassify.h/.cpp`, `BiomeClimate.cpp`, `RegionRepair.cpp`, `WorldGenSession.cpp`, `TerrainGenerator.cpp`, `TerrainTypes.h`, `WorldJson.cpp`, `WorldJsonImport.cpp`, `WorldLayerSource.cpp` | Use the registry |
| `TerrainTableNavigationProfile.h/.cpp` | Read the bound terrain profile |
| S-1 parameter sidecar (from Parameter Files) | Climate rules removed; terrain maps added |
| `Cluiche/Assets/CoW/Global/WorldGen/terrain_navigation.json` | Deleted |
| `Cluiche/Assets/CoW/Stages/S1WorldGenerationTestbedStage/worldgen.schema.json` | `biomeIndex` uses `valuesFrom` |
| `WorldGenerationModule.h/.cpp` | Owns and loads the registry; `GetBiomeRegistry()`; health |
| `Dia/DiaValueSchema/ValueInfo.h`, `ValueSchemaLoader.cpp`, `ValueSchemaModule.cpp` | `valuesFrom` |
| `Dia/DiaCLI/dia_cli/commands/validate/{schema_rules,data_rules}.py` + tests; `Dia/DiaValueSchema/Testing/Conformance/` | `valuesFrom`, `data.incomplete-map` |
| `Cluiche/Tests/GoogleTests/CoW/*Biome*`, `*WorldJson*`, `*Region*`, `*NodesRiver*`, `*SiteGuarantees*`, `*Validation*`, `*Navigation*` and the `Testing/*TestWorlds.h` helpers | Id lookup through the fixture registry; new `TestBiomeRegistry.cpp` |
| `assets.catalogue.json`, CoW `pipeline.toml` | `json.biomes`; deploy `Global/World/` |
| `docs/specs/applications/cow/systems/a1-ecology/a1-ecology.md:81` | Superseding note |
| T-2 specs (`plugin-shell-files-and-schema.md`, `table-and-form-editors.md`) | `valuesFrom` and completeness amendments |
| `CoW.vcxproj`, `GoogleTests.vcxproj` (+ filters) | Via `dia docs vcxproj-add` |

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| SD-S1-005 | Terrain first, then biomes; biomes fall out of terrain by rule | Unchanged. The rules read their biome-specific facts from the registry, and the order of stages 12–14 is the same. |
| SD-S1-008 | Retry never changes the shaped world | Unchanged. The registry is fixed for the life of a world; retries reuse it. |
| SD-S1-009 | Provenance and locked imports | The export format is unchanged. Whether provenance also records `BiomeRegistry::Hash()` is Open Design Question 3. |
| SD-S1-002 | Every editor action is a DiaAPI command | No new action. Registry contents show through existing `cow.worldgen` state queries and the layer legend. |
| AD-002 (CoW) | Four-tier dependency model | The registry is CoW-Shared (`Shared/World/`): S-1 owns it, and A1, A3, A4, A5, T-1 and T-2 read it. |
| AD-003 (CoW) | GDD owns design intent | The rows carry today's GDD-derived values unchanged. Biome additions and value changes are folded back to the GDD as before. |
| AD-001 (CoW) | Ask before Dia edits | DiaValueSchema `valuesFrom` and the CLI and conformance `data.incomplete-map` rule: **signed off by the user 2026-11-18.** |
| PD-004 | No STL in public APIs | Registry lists are pointer and count; strings are fixed-size; masks are `uint64_t`. |
| D5, D6 (research) | Fail loudly; records versus parameters | The registry is a record file loaded strictly with no fallback. The terrain profile is parameters (maps over `TerrainType`). |
| Superseded: a1-ecology.md:81 | "One shared 13-value `Biome` enum, not a StringCRC set" | **Superseded** (user, 2026-11-18): one data registry with runtime indices, which keeps the original reasons (one list, one palette, one name table). |

## Test Plan

TDD per `.claude/skills/tdd.md`, in this order:
1. `BiomeRegistry` load and API on fixtures, including every strict-load failure.
2. `valuesFrom` and `data.incomplete-map` in C++ and CLI fixtures.
3. Migrate the S-1 tests to the fixture registry (mechanical; haiku) while the enum still exists, so they pass against both.
4. Switch production to the registry; delete the enum.
5. Goldens bit-identical.
6. The data-only extra-biome proof.
7. Terrain profile maps and navigation.
8. The grep test for biome literals.

Scoped runs use `--filter="*Biome*:*WorldGen*:*WorldJson*:*Region*:*Navigation*:*Reachability*:*ValueSchema*:*TerrainRules*"` with `--shards 2`, plus CLI `pytest` for `validate`. There is one full suite run at close-out. The gap audit and edge pass is the last plan task.

## Open Design Questions

1. **Reloading the registry.** Indices are baked into every cell, layer and per-biome array, so a mid-world reload could shift them. **Recommended:** the registry loads at module start only. A dev edit to `biomes.json` takes effect on the next module start (or a future `cow.worldgen.regenerate` that reloads first). `FileWatcher` does not hot-swap it.
2. **`JsonDefinitionLoader` as the loader.** This feature is its first production consumer (D6). Its support for arrays of records with optional nested objects (`climate`, `pin`) through `DIA_SERIALIZE` is unproven. **Recommended:** plan Task 0 spikes it on `biomes.json`. If it can't express the shape, use a small hand parser validated by the same sidecar, and log the gap against DiaData rather than bending the file shape.
3. **Registry hash in world provenance.** A world exported under one registry and imported under another (rows added, removed or reordered) imports silently, with unknown ids falling back. **Recommended:** export provenance (SD-S1-009) records `BiomeRegistry::Hash()`, and import warns, without failing, on a mismatch. This is an additive field in the world format. Decide before A1/A4/A5 start saving per-biome state.

## Resolved Design Questions (2026-11-18, user)

All three are decided as recommended:
1. **Reload.** The registry loads at `WorldGenerationModule` start only. There is no live hot-swap; `FileWatcher` does not reload it.
2. **Loader.** Plan Task 0 spikes `JsonDefinitionLoader` on `biomes.json`. If it can't express the shape, use a small hand parser validated by the same sidecar, and log the gap against DiaData.
3. **Provenance.** World export provenance records `BiomeRegistry::Hash()` (an additive field). Import warns on a mismatch but does not fail. **This adds to AC 8:** export writes the hash, and importing a world with a different hash logs one warning.

## Status

**Status:** `Approved` (2026-11-18, user). No plan yet; write one at `/implement`, after DiaAttribute Parameter Files. Interview done with the user on 2026-11-18 (decisions: delete the enum; `valuesFrom` palette; terrain profile included; row order is significant; AD-001 sign-off for `valuesFrom` and `data.incomplete-map`).
