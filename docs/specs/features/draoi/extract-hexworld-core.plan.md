**Spec:** @docs/specs/features/draoi/extract-hexworld-core.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `Draoi/DraoiHexWorld` static-lib project, vcxproj/filters, `Draoi` sln folder, module doc; CoW project reference + include dir | Empty lib builds; `dia check deps`; `dia check sln-sync` | Pending | sonnet | Layout/placement signed off in spec (2026-11-21) |
| 2 | Move `CellVocabulary`, `StratumTable`, `WorldHexGrid`, `SharedWorldHexGrid`, and world data types (`Cell`, `WorldStructure`, `RegionTable`, `SiteSet`); split generator-only declarations out of `TerrainTypes.h` first; update includes | Scoped filters for Stratum/Grid/Terrain; goldens byte-identical | Pending | sonnet | Mechanical; check data types don't include `TerrainRules`/generator |
| 3 | Move `BiomeRegistry` + `Testing/` helpers; update includes | `TestBiomeRegistry*` | Pending | sonnet | Registry must not include generator |
| 4 | Move tests that need no CoW data to `Tests/GoogleTests/Draoi/`; split mixed files | Filtered runs | Pending | sonnet | Open Q3: per-file decision |
| 5 | Namespace change to `Draoi::HexWorld` | Full suite, goldens | Pending | sonnet | Separate commit; solo verify |
| 6 | Close-out: gap audit, edge tests, one full sharded suite | Full suite (`--shards >= 2`) | Pending | sonnet | No mutation checks |
