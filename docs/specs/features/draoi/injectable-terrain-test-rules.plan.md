**Spec:** @docs/specs/features/draoi/injectable-terrain-test-rules.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add neutral `TerrainTestHost.h` (config, `TestRules`, `TestSidecar*`, `TestSession`); reduce `CoWTerrainRulesTestHelpers.h` to forwarders; register CoW config in `CoWTestAssetPaths.cpp` | RED-GREEN: unset-config abort message; `--filter=TerrainRules*:WorldGenSession*` | Done | sonnet | Scoped run: 93 tests pass (TerrainTestHost, TerrainRules*, WorldGenSession*). RED not captured (agent interrupted) |
| 2 | Move `BiomeTestGrids.h` and `TerrainSliderDriver.h` to `Draoi/DraoiHexWorldTerrain/Testing/`; update includes in dependent tests | `--filter=Biome*:Terrain*` | Done | sonnet | 430 tests pass over 2 shards; 18 dependents updated |
| 3 | Split `WorldJsonTestWorlds.h`; move the neutral half (node kinds injected) | `--filter=WorldJson*:WorldGenSessionImport*` | Done | sonnet | 253 tests pass over 2 shards; kind ids from config nodeKinds, goldens unchanged |
| 4 | Move `TestBiomeRegistryEdge` and `TestStratumTable` to `Tests/GoogleTests/Draoi/` | their own suites | Pending | sonnet | CoW-name assertions to a CoW file |
| 5 | Split `TestRegionsAndSitesTypes` (CoW node-kind names stay) and move the neutral half | its suite | Pending | sonnet | |
| 6 | Close-out: grep check, `dia check deps/arch/sln-sync`, `dia docs precommit`, one full sharded suite, gap audit | Full suite | Pending | sonnet | No mutation checks |
