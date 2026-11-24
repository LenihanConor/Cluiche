**Spec:** @docs/specs/features/draoi/injectable-terrain-test-rules.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add neutral `TerrainTestHost.h` (config, `TestRules`, `TestSidecar*`, `TestSession`); reduce `CoWTerrainRulesTestHelpers.h` to forwarders; register CoW config in `CoWTestAssetPaths.cpp` | RED-GREEN: unset-config abort message; `--filter=TerrainRules*:WorldGenSession*` | Done | sonnet | Scoped run: 93 tests pass (TerrainTestHost, TerrainRules*, WorldGenSession*). RED not captured (agent interrupted) |
| 2 | Move `BiomeTestGrids.h` and `TerrainSliderDriver.h` to `Draoi/DraoiHexWorldTerrain/Testing/`; update includes in dependent tests | `--filter=Biome*:Terrain*` | Done | sonnet | 430 tests pass over 2 shards; 18 dependents updated |
| 3 | Split `WorldJsonTestWorlds.h`; move the neutral half (node kinds injected) | `--filter=WorldJson*:WorldGenSessionImport*` | Done | sonnet | 253 tests pass over 2 shards; kind ids from config nodeKinds, goldens unchanged |
| 4 | Move `TestBiomeRegistryEdge` and `TestStratumTable` to `Tests/GoogleTests/Draoi/` | their own suites | Done | sonnet | 24 tests pass (unchanged count). Residual `cow.biomes`/`cow.strata`/`cow.zones` format ids and one spec-path comment accepted: backlog already dropped these as saved-data ids |
| 5 | Split `TestRegionsAndSitesTypes` (CoW node-kind names stay) and move the neutral half | its suite | Done | sonnet | Draoi file 20 tests (2 CoW-name tests replaced by neutral versions), CoW file keeps the 2 originals verbatim; 23 pass |
| 6 | Close-out: grep check, `dia check deps/arch/sln-sync`, `dia docs precommit`, one full sharded suite, gap audit | Full suite | Done | sonnet | Full sharded suite 17,750 pass (4 shards); deps, arch, precommit pass; residual grep hits accepted (see task 4 and one pre-existing spec-path comment) |
