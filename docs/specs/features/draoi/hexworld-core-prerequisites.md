# Feature Spec: DraoiHexWorld Core Prerequisites (data-driven pin rules, generic zones, enum split)

**Parent:** @docs/specs/applications/draoi/draoi.md
**Status:** `Approved` (2026-11-21)
**Origin:** Draoi layer audit, 2026-11-21 (Biome Registry and WorldGeneration coupling audit in the parent spec).

## Summary
Three changes that must land, in CoW, **before** `BiomeRegistry` and the world types can move into a Draoi core package. Each is behaviour-preserving: world generation goldens (`worldgen_layers.golden.json`) stay bit-identical.

1. **Data-driven pin rules.** `PinRule { WaterOcean, WaterLake, Vent, Alpine, Wetland }` is a closed enum naming CoW terrain concepts. Replace it with an open rule id declared in `biomes.json`.
2. **Generic depth zones.** `DepthZone { None, FirstShores, WildReaches, AncientInteriors }` hard-codes CoW's three-zone world structure. Replace it with a zone index whose names and count come from data.
3. **`TerrainEnums.h` split.** Break the include cycle `BiomeRegistry` <-> `TerrainEnums.h` by moving the cell vocabulary into a core header and the generator-only options out.

## Goals
- A game can declare a biome set with no volcanic biome (or any other CoW pin) and still load and generate.
- A game can declare its own names for the three depth zones (a different zone count is deferred).
- `BiomeRegistry`, `BiomeIndex`, the zone table and the cell enums have no include dependency on the generator.

## Non-goals
- Moving any file to Draoi (separate extraction tasks, after these land).
- Changing generation output. Any golden diff is a bug in this feature.
- Generalising `TerrainType` or `TerrainFlag` (a never-produced enumerator such as `Volcano` is not content leakage; revisit only if a second game objects).

## Design

### 1. Pin rules as open ids
Today the rule *logic* is already in the generator (`BiomeClassify.cpp`: `PinTestHits`, `PinLayerValue`, named slots `oceanBiome`, `lakeBiome`, `ventBiome`, `alpineBiome`, `wetlandBiome`). The registry only stores which enum value a biome carries.

- `biomes.json` keeps `"pin": {"rule": "<name>", "priority": n}`. The registry stores the rule as a `StringCRC` (plus the name text for diagnostics) and no longer validates it against an enum.
- The **generator owns a rule table**: `rule id -> { predicate, pin layer value }`. The five existing rules (`water_ocean`, `water_lake`, `vent`, `alpine`, `wetland`) register as built-ins of the terrain package. The application may register more.
- `BiomeClassify` resolves `rule id -> biome` through the registry instead of five named slots, in priority order, exactly as `PinsByPriority` does today.
- A pin rule id with no registered predicate fails the load loudly (all-or-nothing, as the registry does now), naming the biome row.
- A rule with no biome declaring it is simply unused (a game with no volcanoes omits `vent`).

**Rejected:** declarative conditions over cell fields (DiaCondition over `WorldCellFields`) for the predicates. More general, but it moves hot-path classification onto an interpreter and risks golden drift. Revisit when a second game needs a pin the built-ins cannot express.

### 2. Generic depth zones
- `DepthZone` becomes `uint8_t` (0 = none/ocean, 1..N = zone) owned by core, with no named enumerators.
- A **zone table** in data (new `Global/World/zones.json`, or a `zones` block in `biomes.json`; see Open Question 1) lists zones in order, with display names. CoW's file declares `FirstShores`, `WildReaches`, `AncientInteriors`.
- `biomes.json` `"zones": [...]` already references zones by name; it now resolves against the table instead of `kZoneNames[]` in `BiomeRegistry.cpp`.
- Generator code that indexes zones by position generalises from "3 slots" to N: `RegionPartition` zone slots, `NodesCorridor` barrier pairs (barrier k separates zone k from k+1), `TerrainDepthZones` thresholds. `TerrainRules::kFirstShoresShare` becomes a per-boundary share list; the sidecar default keeps CoW's value.
- Code that means "the innermost zone" (`SiteGuarantees`' AncientInteriors cells) asks the table for the last zone. That file is CoW content and stays in CoW anyway.
- Save/JSON: `WorldJson` already writes zone *names*; the loader resolves names against the table. A world saved with a zone name the table lacks is refused (existing refusal path).

Scope: 29 files reference `DepthZone` (about 80 occurrences, 8 test headers). Mechanical but wide.

### 3. `TerrainEnums.h` split
- To **core** (`World/`): `BiomeIndex`, `kNoBiome`, `DepthZone`, `WaterSource`, `TerrainType`, `RegionId`, `kNoRegion`, `TerrainFlag`. Sims read these off a cell.
- To the **terrain** side (stays in `WorldGeneration/`): `BarrierForm { Plateau, VolcanicChain }`, a generator option.
- `BiomeRegistry.h` includes the core header only. The 10 generator files that include the registry keep working; the cycle is gone because the registry no longer reaches into `WorldGeneration/`.

## Binding Decisions
No binding constraints apply beyond the parent spec's PD-011..014 (mechanism in Draoi, content in the application).

## Tasks (order matters)
| # | Task | Test | Notes |
|---|------|------|-------|
| 1 | Split `TerrainEnums.h` (core header + `BarrierForm` stays); fix includes | Build; goldens unchanged | Pure move |
| 2 | Pin rule ids: registry stores `StringCRC`, generator rule table with 5 built-ins, `BiomeClassify` resolves via table | Registry load tests (unknown rule fails loud); a biomes file with no `vent` biome loads and generates; goldens unchanged | The "game without volcanoes" proof |
| 3 | `zones.json` loader (sidecar-owned shares) + `DepthZone` as index type; replace `kZoneNames[]` and the `DepthZone` enumerator names in code | Zone table tests; `WorldJson` round trip; goldens unchanged | Zone count stays 3 |
| 4 | Replace named-enumerator uses (`FirstShores`/`WildReaches`/`AncientInteriors`) with index/table lookups in the generator, sites and tests | CoW goldens unchanged; a zones file with renamed zones loads and generates | Wide but mechanical (about 29 files); no count generalisation |
| 5 | Gap audit and edge tests | Full CoW suite once | Per project rule |

## Resolved Design Questions (2026-11-21, all recommendations accepted)
1. **Zone table lives in a separate `zones.json`** (own sidecar and params). Zones carry generator shares, which are sidecar-owned parameters, not biome facts.
2. **Names only for now.** The generator keeps three zone slots; zone names and share defaults come from data. A different zone *count* is deferred until a second game needs it (Draoi admission test). Consequence: the "N zones" generalisation in Design section 2 and the 2-zone/4-zone fixture in task 4 are **out of scope**; `DepthZone` is still an index type so the later generalisation stays non-breaking.
3. **No application hook for pin rules yet.** Only the five built-ins register, in the terrain package. The registry stores the rule as an open id, so adding a hook later is not a breaking change.
