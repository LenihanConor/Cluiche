# Feature Spec: Generic Depth-Zone Count (N zones from data, 3 is CoW's choice)

**Parent:** @docs/specs/applications/draoi/draoi.md
**Status:** `Draft` (2026-11-21; user: "the 3 zones seem very CoW oriented")
**Origin:** Reopens Resolved Question 2 of @docs/specs/features/draoi/hexworld-core-prerequisites.md (names only, count deferred). The user now wants the count generic. That spec said the later generalisation stays non-breaking because `DepthZone` is already an index.
**Depends on:** hexworld-core-prerequisites (Done). Best done **before** the terrain extraction, so Draoi never ships a 3-zone generator.

## Summary
`zones.json` lists zones in order, but the generator still has exactly three slots: `kRegionZoneCount = 3` with per-zone region counts (`RegionPartition`), a three-band depth algorithm with a "first share" and a "last share" (`TerrainDepthZones`), a special "innermost zone" rule (main landmass only, minimum depth), barrier k separating zone k from k+1 (`NodesCorridor`), and a loader that refuses more than three zones. Make the zone count N come from data, bounded by a compile-time upper limit. CoW stays at 3 with identical output.

## Goals
- A game declares 1..N zones in `zones.json` (upper bound `kMaxZones`, compile-time, e.g. 8; 3 is no longer special).
- Behaviour-preserving for CoW: the three-zone world is byte-identical (goldens, JSON, region layouts).
- Everything CoW-specific about zones becomes data: names, band shares, per-zone region counts, and which zone carries the special "innermost" rules.

## Non-goals
- Making regions optional (decided 2026-11-21: regions stay as they are; gameplay may ignore them).
- Changing the depth-cost model (ridge, barrier, river corridor costs stay).
- A zone count above `kMaxZones`.

## Design
**Zone table gains per-zone data** (`zones.json` + its sidecar, same sidecar-owned-params pattern):
| Today (hard-coded) | Becomes |
|---|---|
| `kFirstZoneShare`, `kLastZoneShare` (two shares; the middle zone is "the rest") | `share` per zone: the fraction of main-landmass cells in each band, outermost first; the **last zone's share is implied** (remainder) or explicit and checked to sum to 1. CoW values reproduce 0.45 / 0.40 / 0.15 |
| `kZoneCountMin[3]`, `kZoneCountMax[3]` (4-6, 4-6, 2-4 regions) | `regions_min` / `regions_max` per zone |
| Innermost zone rule: main landmass only, `kAncientMinDepth` coast distance; failing it demotes to the previous zone | per-zone flags `main_landmass_only` and `min_coast_depth` (CoW: only the last zone sets them) |
| Thresholds `t1`, `t2` from two shares | N-1 thresholds from cumulative shares, same "next distinct cost" fix-up when a threshold collapses, with `hasInnermost`-style flag generalised to "zone is empty" |
| `NodesCorridor` barrier pairs (barrier k between zone k and k+1) | already positional; loops to N-1 |
| `RegionPartition` arrays sized 3, RNG draws one per zone in zone order | arrays sized `kMaxZones`, draws one per **declared** zone in order (CoW's draw order is unchanged, goldens hold) |
| `ZoneTable` loader refuses more than 3 zones | refuses more than `kMaxZones` |
| `SiteGuarantees`/`variant_cover` `to_zone` | already resolves a name through the table; no change |

**Compatibility.** The shipped CoW `zones.json` gains the per-zone fields with CoW's current values. The sidecar keys `kFirstZoneShare`/`kLastZoneShare` are replaced by the per-zone `share` (hard cutover, no aliases, as with earlier cutovers).

## Binding Decisions
No binding constraints beyond PD-011..014 (the zone model is generator mechanism; CoW's three zones are content in `zones.json`).

## Acceptance Criteria
1. With the shipped 3-zone file, `worldgen_layers.golden.json`, all JSON goldens and region layouts are byte-identical.
2. A 2-zone file and a 5-zone file load and generate; every land cell has a zone in `1..N`, ocean is `kNoZone`, and region counts respect each zone's `regions_min/max`.
3. A 1-zone file works (no thresholds, no barriers, all land in zone 1).
4. Load faults (zero zones, more than `kMaxZones`, shares not summing to 1, `regions_min > regions_max`, duplicate or reserved names) fail loud, naming the zone.
5. No `kRegionZoneCount`, `kFirstZoneShare`, `kLastZoneShare`, `kZoneCountMin/Max` or "three zones" remain in the generator.
6. Existing CoW suite passes unchanged apart from fixtures.

## Tasks
| # | Task | Test | Notes |
|---|------|------|-------|
| 1 | `ZoneTable` per-zone fields (share, regions min/max, special-zone flags), `kMaxZones`, loader and schema/sidecar, shipped CoW values | RED-GREEN: loader tests, faults (AC 4) | Hard cutover of the two share keys |
| 2 | `TerrainDepthZones` to N bands (cumulative shares, per-zone rules, collapsed-threshold fix-up) | RED-GREEN: 1, 2, 3, 5 zone grids; 3-zone parity against current outputs (AC 1-3) | Parity first: assert identical results on the existing fixtures before generalising |
| 3 | `RegionPartition`/`RegionRepair`/`RegionTable::limitedBy` to N zones, counts from data | RED-GREEN: 2- and 5-zone region counts (AC 2); goldens (AC 1) | RNG draw order preserved |
| 4 | `NodesCorridor`, validation and viewer sites that index zones; remove leftover 3-zone constants and comments | grep proof (AC 5); scoped suite | |
| 5 | Gap audit, edge tests (collapsed thresholds, empty zones, tiny islands with N zones), full suite once | Full sharded suite | No mutation checks |

## Open Design Questions
1. **Shares model.** Per-zone explicit shares summing to 1 (clear, but easy to mis-sum) or N-1 cumulative cut points (always valid)? Recommended: per-zone shares, last implied as the remainder.
2. **`kMaxZones`.** 8 keeps arrays small and covers any plausible game; confirm.
3. **Order relative to the terrain extraction.** Recommended: do this first (it is a CoW-internal refactor with golden protection), then extract.
