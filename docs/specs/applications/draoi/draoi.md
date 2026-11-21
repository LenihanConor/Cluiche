# Application Spec: Draoi

## Parent Platform
@docs/specs/platform/Cluiche.md

## Purpose
**Draoi** (Irish "druid", a mediator) is the layer between the Dia engine and the applications: reusable, content-free gameplay libraries that any game can consume but that are too opinionated to be engine. Like Dia, it is organised as an "application" for spec purposes only; it ships libraries, not an executable.

Dependencies point down only: Applications → Draoi → Dia. See PD-011 to PD-014 in the platform spec for the binding rules.

## Scope
- Scoped by **role, not genre**. A platformer character controller and a hex-world generator are both valid residents.
- Genre kits are separate opt-in packages; a game links only what it uses.
- **Admission test:** a package needs two consumers, or a credible second one. Nothing is extracted on speculation.
- **Mechanism, not content:** Draoi offers hooks, registries and generators. Species tables, parameter data, site guarantees and the like stay in the game.
- **Plain libraries:** no `SimModule`, no processing-unit affinity, no bus, save or debug-domain wiring (PD-012). Test helpers ship in `<Package>/Testing/`.
- **Sims are scheduler-agnostic:** state in, state out, safe to run off-thread, no knowledge of any step chain (PD-013). The application owns execution order and writes the stage adapter.

## Naming
Packages are prefixed `Draoi` and named by what they hang off, e.g. `DraoiHexWorld`, `DraoiHexWorldWeather`. Packages that are not tied to hexes carry no infix (`DraoiGameSpeed`).

## Planned Packages
Nothing is built yet. Extraction order follows the CoW audit of 2026-11-20.

| Package | Source | Notes |
|---------|--------|-------|
| DraoiGameSpeed | CoW `Shared/GameSpeedController.h` | Day base tick, time scale, presets |
| DraoiHexWorld | CoW `Shared/World*`, `Shared/WorldGeneration/*` | Needs `WorldGenSession` to take its guarantee set as injected configuration first; `SiteGuarantees` (Hearth/Nexus/Throne) stays in CoW |
| DraoiHexWorldWeather | A4 Weather spec | Pure sim; stage adapter lives in CoW |
| DraoiHexWorldHydrology | A5 Hydrology spec | Pure sim; stage adapter lives in CoW |
| DraoiHexWorldEcology | A1 Ecology spec | Pure sim; stage adapter lives in CoW |

The step chain (`IWorldStepStage`, `WorldStepChain`, config, listener) is not a Draoi package. It was audited as pure mechanism with no world knowledge, so it moves into Dia's `DiaDeferredStep` under generic names, pending its own feature spec (Dia placement needs sign-off under the escalation rule).

## Open Design Questions
1. ~~**World-sim kit split.**~~ **Resolved 2026-11-20** in the World-Sim Kit's "Draoi layer amendment" (@docs/specs/applications/cow/systems/worldstep/worldsim-kit.md): stage base, restore sequence, commands and names stay in CoW; `WorldCellFields`, `CellSelectorSet` and the codec data half go to DraoiHexWorld; sims expose `Step` plus a thread-neutral `Capture`/`Run`/`Commit`; packages depend one way (HexWorld <- Weather <- Hydrology <- Ecology) with feedback as optional, defaulted input arrays filled at `Capture`; the bus carries notifications only; `IParallelExecutor` moves to DiaThreading. Still owed: A1, A4 and A5 amendments, and a kit plan revision.
2. ~~**CluicheGameBaseline overlap.**~~ **Resolved 2026-11-21: no overlap.** Baseline (~16k lines) is generic *runtime modules* any game needs (boot menu, `Scene2DModule`, camera, render, physics, stage catalogue, debug and inspector modules, `JobSystemModule`). Draoi is generic *libraries* (no modules, PD-012). They are complementary: Baseline is the home for generic modules, Draoi for generic libraries.
3. ~~**Engine-grade plumbing.**~~ **Decided 2026-11-21: defer.** `RandomStreamModule`, `SaveLoadModule` and `SnapshotStoreAdapter` are runtime modules, so they cannot live in Draoi. They stay in CoW until a second game needs them; the destination then is CluicheGameBaseline (AD-001 sign-off), not Dia. Draoi sims take the plain `RandomStream` type as an argument and do not depend on the module wrappers.

## Biome Registry (decided 2026-11-21)
The registry mechanism moves down to `DraoiHexWorld` core; the biome list (`biomes.json`) stays in the application, so a game with no volcanic areas simply does not declare them. Two prerequisites before extraction (today `BiomeRegistry.h` is not content-free):
1. **Pin rules become data-driven.** `PinRule { WaterOcean, WaterLake, Vent, Alpine, Wetland }` and `BiomeRole` hard-code CoW terrain concepts. Replace with named rules declared in `biomes.json` that select generic behaviours, so `Vent` and `Alpine` are data, not enum values.
2. **`BiomeIndex` and `DepthZone` move into the core package.** The registry currently includes `WorldGeneration/TerrainEnums.h` for them; the core package owns them so the registry no longer depends on the generator.

Package split (agreed 2026-11-21): `DraoiHexWorld` core (grid and world types, `BiomeIndex`/`DepthZone`, `BiomeRegistry`, `WorldCellFields`, `CellSelectorSet`, codec data half); the S-1 generator becomes a separate terrain package that depends on core; Weather, Hydrology and Ecology depend on core only, not on the generator. 

### WorldGeneration coupling audit (2026-11-21)
`Shared/` never includes `Modules/`, and `WorldGeneration` depends only on Dia and `World/BiomeRegistry`, so the split is mechanical. Findings and decisions:
- **One include cycle:** `BiomeRegistry` includes `WorldGeneration/TerrainEnums.h`, and 10 generator files include the registry back. Fix by splitting `TerrainEnums.h`: `BiomeIndex`, `DepthZone`, `WaterSource`, `TerrainType`, `RegionId` go to core (every sim reads them off a cell); `BarrierForm { Plateau, VolcanicChain }` goes to the terrain package (generator option, same volcanic issue as `PinRule`).
- **`DepthZone` becomes generic** (decided): zones are indices with names from data, not the hard-coded `FirstShores`/`WildReaches`/`AncientInteriors`. **Superseded 2026-11-21 by strata** (@docs/specs/features/draoi/generic-strata.md, Approved, 7 tasks, lands before the extraction): `DepthZone` becomes `StratumIndex`, `ZoneTable` becomes `StratumTable` and `strata.json` (both core / CoW content respectively), `TerrainDepthZones` becomes `TerrainStrata` with an injectable `IStrataAssigner` in the terrain package. Count and per-stratum shares are data, not 'names only'.
- **CoW content is concentrated in `SiteGuarantees` (46 hits) and `WorldGenSession` ~1968-2218** (hard-coded guarantee set). `WorldOverlayGeometry.cpp` and `Modules/WorldGenViewHost.cpp` also include `SiteGuarantees.h`, so Draoi step 2 (inject the guarantee set) must cover all three, not only `WorldGenSession`.
- **Placement of the remaining files:** `WorldHexGrid`/`SharedWorldHexGrid` -> core. `WorldJson`, `WorldJsonImport`, `WorldFiles`, `WorldShapePreset`, `WorldLayerSource` -> terrain package (they depend on `SiteRegistry`, `TerrainRules`, `WorldGenSession`). `WorldCameraController` (pure `DiaMaths`, one consumer) and `WorldOverlayGeometry` (depends on `SiteGuarantees`) stay in CoW as view code; the camera controller fails the two-consumer admission test today.

## Extraction specs
Feature specs for extraction order step (e) (order: Strata -> core -> terrain -> GameSpeed):
- @docs/specs/features/draoi/extract-hexworld-core.md
- @docs/specs/features/draoi/extract-hexworld-terrain.md (Approved 2026-11-21)
- @docs/specs/features/draoi/extract-draoigamespeed.md (Approved 2026-11-21)

## Status
Draft. No packages exist.
