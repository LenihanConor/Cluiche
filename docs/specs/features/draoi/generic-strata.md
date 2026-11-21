# Feature Spec: Strata (data-defined gameplay areas; depth is one set, the count is not 3)

**Parent:** @docs/specs/applications/draoi/draoi.md
**Status:** `Draft` (2026-11-21; interview with the user the same day: name, data-defined sets, injectable assigner, maximum data flexibility for tweaking)
**Origin:** The user's objection that "the 3 zones seem very CoW oriented", then: "other games could use this construct for other things... this is my stratum and this is yours... a gameplay area". Reopens Resolved Question 2 of @docs/specs/features/draoi/hexworld-core-prerequisites.md (names only, count deferred); that spec noted the generalisation stays non-breaking because `DepthZone` is already an index.
**Depends on:** hexworld-core-prerequisites (Done). Best done **before** the terrain extraction, so Draoi never ships a 3-zone generator.

## Summary
A **stratum** is a named class of cells that gameplay cares about ("First Shores", "the north kingdom", "difficulty tier 2"). Strata are **gameplay**, regions are **geographic** (regions stay as they are: always generated, a game's gameplay may ignore them). A **stratum set** is one named classification of the world's cells into strata; CoW's current "depth zones" become the set called `depth`: three ordered strata assigned by travel cost from the coast.

Today the generator hard-codes three slots (`kRegionZoneCount = 3`, per-zone region counts, a first-share and a last-share band algorithm, an "innermost zone" rule, barrier k between zone k and k+1) and `zones.json` lists only names. This spec makes the **stratum sets, their strata and the assigner choice data-defined**, with an injectable assigner seam for anything data cannot express. CoW keeps three strata with byte-identical output.

## Goals
- A game declares its stratum sets and each set's strata (1..`kMaxStrata`, compile-time cap, suggested 8) in data; 3 is CoW's choice, not the generator's.
- A set names its **assigner** and parameters in data. The built-in assigner is **coast-cost banding** (today's algorithm, generalised to N bands). A game can register its own assigner in code (`IStrataAssigner`); data selects it by name.
- **Flexibility is the point (user, 2026-11-21: "a lot of flexibility (Data) so we can tweak").** Multiple sets, per-stratum attributes and the banding metric are all data, and every number is a sidecar-owned, live-tweakable parameter.
- A set is **ordered** or not (data flag). Only ordered sets get banding by shares and "barrier k separates stratum k from k+1".
- Everything CoW-specific about strata is data: names, shares, per-stratum region counts, which stratum carries the "innermost" rules (main landmass only, minimum depth).
- Behaviour-preserving: the three-stratum CoW world is byte-identical (goldens, JSON, region layouts).

## Non-goals
- Regions: unchanged. They stay always-on and geographic; gameplay may ignore them (user decision 2026-11-21).
- **Dynamic ownership** ("this territory is captured by faction X during play") is simulation state and belongs in a sim's cell layer, not in stratum sets, which are generation-time and static.
- Assigners that depend on placed sites (sites are placed after strata).
- Built-in assigners beyond `coast_cost_bands` (its `metric` is data-selectable, see Design); further assigner types arrive through the injection seam when a game needs them.

## Design
**Vocabulary (renames, hard cutover, no aliases).**
| Today | Becomes |
|---|---|
| `DepthZone` (uint8_t index), `kNoZone` | `StratumIndex`, `kNoStratum` (0 = none/ocean) |
| `Cell::depthZone` | `Cell::stratum` (the built-in `depth` set; see Storage) |
| `World::ZoneTable`, `zones.json`, `zones.schema.json`, catalogue aliases `json.zones*` | `World::StratumTable`, `strata.json`, `strata.schema.json`, aliases `json.strata*` |
| `TerrainDepthZones` | `TerrainStrata` (coast-cost assigner) |
| `to_zone` in `guarantees.json` | `to_stratum` (names resolve against the set) |
| JSON `depth_zone` in the world file | `stratum` (world JSON version bump; the importer refuses an unknown stratum name as it refuses an unknown zone name today) |

**`strata.json`** (new shape; per-stratum numbers that are tunable live in the sidecar like the other sidecar-owned params):
```
{ "format": "cow.strata", "version": 1,
  "sets": [ { "id": "depth", "ordered": true,
              "assigner": { "type": "coast_cost_bands", "metric": "coast_cost" },
              "strata": [ { "name": "FirstShores",  "share": 0.45, "regions_min": 4, "regions_max": 6 },
                          { "name": "WildReaches",  "share": 0.40, "regions_min": 4, "regions_max": 6 },
                          { "name": "AncientInteriors", "share": 0.15, "regions_min": 2, "regions_max": 4,
                            "main_landmass_only": true, "min_coast_depth": 3 } ] } ] }
```
(The CoW values above reproduce today's 0.45 / remainder / 0.15, 4-6 / 4-6 / 2-4 and `kAncientMinDepth`; exact values are copied from the code in task 1.) The last stratum's share is implied or checked to complete 1. Each stratum may carry an `attributes` block (a danger level, a label, any tunable) that gameplay reads, backed by `DiaAttribute` (schema and accessor bridge); task 1 checks that the schema fits and extends it rather than inventing a second key-value format. Sidecar-owned parameters (shares, minimum depths, cost weights, attribute defaults and ranges) use the existing Parameter Files / `ParamBinding` pattern, so they work with live sliders and parameter files, not only file edits.

**Assigner seam.** `IStrataAssigner { Assign(const WorldStructure&, const StratumSetSpec&, std::span<StratumIndex> out) }` with a registry by type name (like site placers). Contract every assigner must meet: output in `0..N` with 0 only for ocean/unassigned, deterministic from the world and params, no dependence on sites. The built-in `coast_cost_bands` banding takes a data-selected **`metric`**: `coast_cost` (today's travel cost from the coast), `elevation`, `latitude`, or the name of any registered cell field (`WorldCellFields`); the cutting into N bands by cumulative shares is the same for every metric. `coast_cost` is today's algorithm: multi-source Dijkstra over land with ridge, barrier and river-corridor costs, N-1 thresholds from cumulative shares with the existing "next distinct cost" fix-up, per-stratum `main_landmass_only` and `min_coast_depth` (a cell failing its stratum's rule falls to the previous stratum, as Ancient falls to Wild today).

**What reads strata, generalised.**
- `RegionPartition` / `RegionRepair`: arrays sized `kMaxStrata`; one RNG draw per **declared** stratum in order (CoW's draw order unchanged, so goldens hold); counts from `regions_min/max`.
- `NodesCorridor`: barrier k between stratum k and k+1 for ordered sets, loop to N-1.
- `variant_cover` / `reach_any` guarantees: `to_stratum` by name, "innermost" = the last stratum.
- `WorldJson` / `WorldJsonImport`, `WorldLayerSource`, viewer layers: names through the table.

**Storage.** The `depth` set stays a field on `Cell` (hot path, used everywhere). Additional sets declared in data are stored as cell layers (the `DiaCellLayers` pattern used by `WorldCellFields`) and read through the same table API; they are saved in the world JSON and shown in the viewer's layer list. A game with only `depth` pays nothing for them.

## Binding Decisions
No binding constraints beyond PD-011..014: the set model, the table, the loader and the built-in assigner are mechanism (Draoi); CoW's three strata, names and numbers are content in `strata.json`. This is a CoW-internal refactor that lands before the terrain extraction.

## Acceptance Criteria
1. With the shipped 3-stratum CoW file, `worldgen_layers.golden.json`, all JSON goldens and region layouts are byte-identical (world JSON `stratum` rename and version bump aside, which the goldens are regenerated for exactly once and reviewed as a pure key rename).
2. A 2-stratum file and a 5-stratum file load and generate: every land cell has a stratum in `1..N`, ocean is `kNoStratum`, and region counts respect each stratum's `regions_min/max`.
3. A 1-stratum file works (no thresholds, no barriers, all land in stratum 1).
4. A second set (e.g. `homeland`, unordered, injected test assigner) loads, is assigned, appears in the cell layers, the world JSON and the viewer layer list, round-trips through save and load, and does not disturb the `depth` set.
4a. The `metric` choice works: the same 3-stratum file with `metric: elevation` bands by height, and a metric naming an unknown cell field fails load naming it. Stratum attributes declared in data read back through `DiaAttribute`, and a sidecar change (a share, a minimum depth) re-bands the world without a rebuild.
5. Load faults (no sets, duplicate set id, no strata, more than `kMaxStrata`, shares not completing 1, `regions_min > regions_max`, duplicate or reserved names, unknown assigner type, unknown guarantee `to_stratum`) fail loud naming the set and stratum, leaving the previous table in place.
6. A registered custom assigner is selected by name from data; one that violates the contract (value above N, non-zero for ocean) is rejected loudly with the assigner and cell.
7. No `kRegionZoneCount`, `kFirstZoneShare`, `kLastZoneShare`, `kZoneCountMin/Max`, `DepthZone`, `ZoneTable` or "three zones" remain in the generator, viewer, guarantees or docs of the touched packages.
8. Existing CoW suite passes unchanged apart from renamed fixtures.

## Tasks
| # | Task | Test | Notes |
|---|------|------|-------|
| 1 | Rename + data model: `StratumIndex`/`kNoStratum`, `StratumTable` with sets, `ordered`, assigner id and params, per-stratum `share`, `regions_min/max`, flags, `kMaxStrata`; `strata.json` + schema + sidecar, catalogue aliases; world JSON `stratum` key and version bump; guarantee `to_stratum`; hard cutover of `zones.json` | RED-GREEN: loader and fault tests (AC 5); round trip and unknown-name refusal | Wide rename (about 70 files); do it before generalising so the diff stays mechanical. CoW values copied from the code |
| 2 | `IStrataAssigner` seam + registry; `coast_cost_bands` ported to the seam for N bands (cumulative shares, per-stratum rules, collapsed-threshold fix-up) | RED-GREEN: **3-stratum parity first** on the existing fixtures, then 1, 2, 5 strata (AC 1-3, 6) | Parity assertions written before the generalisation |
| 3 | `RegionPartition`/`RegionRepair`/`RegionTable::limitedBy` to N strata, counts from data; `NodesCorridor` to N-1 barriers | RED-GREEN: 2- and 5-stratum region counts (AC 2); goldens (AC 1) | RNG draw order preserved |
| 4 | Additional sets stored as cell layers; world JSON and viewer layer list include them; save and load | RED-GREEN: a second unordered set with a test assigner (AC 4) | In scope (data flexibility) |
| 4b | `metric` choice (`coast_cost`, `elevation`, `latitude`, named cell field); stratum `attributes` via `DiaAttribute`; sidecar live-tweak wiring | RED-GREEN: AC 4a | Checks `DiaAttribute` schema fit first and extends it |
| 5 | Remove leftover 3-zone constants, comments and docs; update the specs and plans that name zones (A1/A3/A4/A5, guarantees, S-1) | grep proof (AC 7); scoped suite | |
| 6 | Gap audit, edge tests (collapsed thresholds, empty strata, tiny islands with N strata, assigner contract violations), full sharded suite once | Full suite | No mutation checks |

## Open Design Questions
1. **Order relative to the terrain extraction.** Recommended: do this first (CoW-internal refactor protected by goldens), then extract, so Draoi receives the generic version.
2. **Scope check on tasks 4 and 4b.** They are what make the system as flexible as asked; together with the rename they make this a large feature (about 8 tasks). Confirm they ship in this spec and not as a follow-up.
3. **Stored extra sets and the world file.** Extra sets add keys to the world JSON; confirm the one version bump for `stratum` also covers them.
