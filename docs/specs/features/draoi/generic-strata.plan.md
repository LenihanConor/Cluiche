**Spec:** @docs/specs/features/draoi/generic-strata.md
**Status:** In Progress

Order: starts after generic special sites (guarantee-set-injection) task 4 lands (both touch WorldGenSession, viewer and guarantees). Task 1 is a wide mechanical rename, done alone before generalising.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Rename + data model: `StratumIndex`/`kNoStratum`, `StratumTable` with sets (`ordered`, assigner id/params, per-stratum `share`, `regions_min/max`, flags), `kMaxStrata`; `strata.json` + schema + sidecar, catalogue aliases `json.strata*`; world JSON `stratum` key + version bump; guarantee `to_stratum`; hard cutover of `zones.json`; goldens regenerated once and reviewed as a key rename | RED-GREEN: loader and fault tests (AC 5); round trip and unknown-name refusal | Done | sonnet | ~70 files. Single set `depth`, three strata, CoW values copied from code. Checks DiaAttribute fit notes only |
| 2 | `IStrataAssigner` seam + registry; `coast_cost_bands` ported for N bands (cumulative shares, per-stratum rules, collapsed-threshold fix-up) | RED-GREEN: 3-stratum parity first, then 1, 2, 5 strata (AC 1-3, 6) | Done | sonnet | Parity assertions before generalising |
| 3 | `RegionPartition`/`RegionRepair`/`RegionTable::limitedBy` to N strata with counts from data; `NodesCorridor` to N-1 barriers | RED-GREEN: 2- and 5-stratum region counts (AC 2); goldens (AC 1) | Done | sonnet | RNG draw order preserved |
| 4 | Additional sets as cell layers; world JSON, save/load and viewer layer list | RED-GREEN: second unordered set with a test assigner (AC 4) | Done | sonnet | |
| 5 | `metric` choice (coast_cost, elevation, latitude, named cell field); stratum `attributes` via DiaAttribute; sidecar live-tweak wiring | RED-GREEN: AC 4a | Done | sonnet | Check DiaAttribute schema fit first; extend it, no second KV format |
| 6 | Remove leftover 3-zone constants/comments; update spec and plan text naming zones (A1/A3/A4/A5, guarantees, S-1, draoi.md) | grep proof (AC 7); scoped suite | Done | haiku | Code, schema and tests clean (grep proof); only bin copy of biomes.schema.json had stale text (generated). Older S-1 specs keep their 2026-11-21 strata banners as history |
| 7 | Gap audit, edge tests, full sharded suite once | Full suite | Pending | sonnet | No mutation checks |
