**Spec:** @docs/specs/features/draoi/extract-hexworld-core.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `Draoi/DraoiHexWorld` static-lib project, vcxproj/filters, `Draoi` sln folder, module doc; CoW project reference + include dir | Empty lib builds; `dia check deps`; `dia check sln-sync` | Done | sonnet | Layout/placement signed off in spec (2026-11-21). deps clean; BiomeRegistry* 47 pass (filter is BiomeRegistry*, not TestBiomeRegistry*) |
| 2 | Move `CellVocabulary`, `StratumTable`, `WorldHexGrid`, `SharedWorldHexGrid`, and world data types (`Cell`, `WorldStructure`, `RegionTable`, `SiteSet`); split generator-only declarations out of `TerrainTypes.h` first; update includes | Scoped filters for Stratum/Grid/Terrain; goldens byte-identical | Done | sonnet | Merged with 3 (one build). New WorldStructure.h/.cpp split from TerrainTypes; WorldShapeParams+BarrierForm+WorldProvenance went to core with it (Cell->WorldStructure->Provenance needs them). TerrainEnums.h deleted. GoogleTests.vcxproj compiled CoW sources directly: entries removed. Scoped 980 pass, goldens untouched |
| 3 | Move `BiomeRegistry` + `Testing/` helpers; update includes | `TestBiomeRegistry*` | Done | sonnet | Done in task 2 commit |
| 4 | Move tests that need no CoW data to `Tests/GoogleTests/Draoi/`; split mixed files | Filtered runs | Pending | sonnet | Open Q3: per-file decision |
| 5 | Namespace change to `Draoi::HexWorld` | Full suite, goldens | Pending | sonnet | Separate commit; solo verify |
| 6 | Close-out: gap audit, edge tests, one full sharded suite | Full suite (`--shards >= 2`) | Pending | sonnet | No mutation checks |
