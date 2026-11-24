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

**Presentation packages carry `Render` plus their dimension** (decided 2026-11-23): `DraoiHexWorldRender3D`, and later perhaps `DraoiHexWorldRender2D`. Dia shows a domain through its folder layout. Draoi is flat, so the name has to say it. Generic techniques never live in Draoi: they go to Dia under a technique name (`DiaToonShading`).

## Planned Packages
Nothing is built yet. Extraction order follows the CoW audit of 2026-11-20.

| Package | Source | Notes |
|---------|--------|-------|
| DraoiGameSpeed | CoW `Shared/GameSpeedController.h` | Day base tick, time scale, presets |
| DraoiHexWorld | CoW `Shared/World*`, `Shared/WorldGeneration/*` | Needs `WorldGenSession` to take its guarantee set as injected configuration first; `SiteGuarantees` (Hearth/Nexus/Throne) stays in CoW |
| DraoiHexWorldWeather | A4 Weather spec | Pure sim; stage adapter lives in CoW |
| DraoiHexWorldHydrology | A5 Hydrology spec | Pure sim; stage adapter lives in CoW |
| DraoiHexWorldEcology | A1 Ecology spec | Pure sim; stage adapter lives in CoW |
| DraoiHexWorldRelief | Former DiaCellFieldBake Feature 2 (2026-11-23) | Relief by terrain class, lake/sea elevation, river distance field over the Dia bake's hooks. System spec: @docs/specs/applications/draoi/systems/draoihexworldrelief/draoihexworldrelief.md (Draft) |
| DraoiHexWorldRender3D | Former Dia system DiaTerrainMap3D (2026-11-23) | Hex-world 3D map: textures, material composed from DiaToonShading, standard channels + 4 custom slots, props, `HexWorldMap` facade; neutral defaults; CoW owns the look and all wiring. System spec: @docs/specs/applications/draoi/systems/draoihexworldrender3d/draoihexworldrender3d.md (Draft) |

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

## Layer split of the 3D map (2026-11-23, user)
The 3D map was specced as the Dia system DiaTerrainMap3D. Under the layer rule it splits three ways:
- **Dia, generic techniques:** DiaToonShading (new), DiaBgfx3D Feature E Heightfield Mesh (new), DiaCellFieldBake (Features 1 and 3, plus the `ITilePostPass` and `WarpedPoint` hooks).
- **Draoi, hex-world vocabulary:** DraoiHexWorldRender3D and DraoiHexWorldRelief. These are the first Draoi system specs; they live under `docs/specs/applications/draoi/systems/`.
- **CoW, the look and wiring:** Strategic Map (module, streams, bake jobs, camera, commands, presets, capture, every GDD number, custom-slot content such as magic snow).

Channel model (hybrid, TMAP-010): standard channels mirror Draoi sim outputs; 4 custom overlay slots carry game-only states, bound to DiaToonShading techniques in the game's style file.

## Extraction specs
Feature specs for extraction order step (e) (order: Strata -> core -> terrain -> GameSpeed):
- @docs/specs/features/draoi/extract-hexworld-core.md
- @docs/specs/features/draoi/extract-hexworld-terrain.md (Approved 2026-11-21)
- @docs/specs/features/draoi/extract-draoigamespeed.md (Approved 2026-11-21)

## Status
Extractions Done: core (2026-11-21), terrain (2026-11-22), GameSpeed (2026-11-23), plus the kit's Draoi half (2026-11-23) and the Draoi tooling (2026-11-23). **The extraction backlog row was closed 2026-11-23.** What remains lives elsewhere: the A4, A1, A3 and A5 sims build in Draoi packages under their own backlog rows (each system spec's "Draoi layer amendment (2026-11-21)"), and the terrain test helper follow-up is a Task row in BACKLOG.md. The step-by-step log is in "Backlog status log" below.

## Backlog status log (moved 2026-11-22)

Moved verbatim from the BACKLOG.md row "Draoi layer: spec, folder pass, extraction" so the backlog row holds only open actions. The Status section above is older than this log.

- Original "Draoi layer: spec, folder pass, extraction" row (agreed 2026-11-20), moved verbatim from BACKLOG.md on 2026-11-22:

  Spec cell: [draoi.md](specs/applications/draoi/draoi.md), PD-011 to PD-014 in [Cluiche.md](specs/platform/Cluiche.md)

  Notes cell: New layer between Dia and applications (plain libraries, scheduler-agnostic sims, no modules; Dia stays world-agnostic). **Done:** platform spec, Draoi stub spec, CLAUDE.md boundary rule. **Status 2026-11-21 (pushed `f7e2f7d17`):** step 1 folder pass **Done** (`Modules/` is in 7 subfolders; scoped CoW tests green); step 3 spec **Approved** (step-chain.md, AD-001 given); step 5 amendments of A1/A3/A4/A5 **drafted**; core prerequisites **Done**. **Next, in order:** (a) ~~core prerequisites~~ **Done 2026-11-21** ([hexworld-core-prerequisites.md](specs/features/draoi/hexworld-core-prerequisites.md): `CellVocabulary.h` split, open pin-rule ids with a generator rule table, `zones.json` + `ZoneTable`, `DepthZone` is an index; full suite 4320 pass, goldens unchanged); (b) step 2: inject the guarantee set into `WorldGenSession` (~lines 1968-2218), `WorldOverlayGeometry` and `WorldGenViewHost` instead of hard-coding Hearth/Nexus/Throne; (c) ~~step-chain~~ **Done 2026-11-21** (Dia `StepChain` in DiaDeferredStep, CoW cut over, full suite 4322 pass); (b) ~~generic special sites~~ **Done 2026-11-21** ([guarantee-set-injection.md](specs/features/draoi/guarantee-set-injection.md)); (b-strata) ~~Strata~~ **Done 2026-11-21** ([generic-strata.md](specs/features/draoi/generic-strata.md), 7 tasks, full sharded suite green): CoW depth zones become data-defined strata (`DepthZone`->`StratumIndex`, `ZoneTable`->`StratumTable` in core, `zones.json`->`strata.json`, `TerrainDepthZones`->`TerrainStrata` + injectable `IStrataAssigner`; count, shares and banding metric are data); **landed before the extraction (e)**, which now moves `StratumTable`/`TerrainStrata` instead of the zone names; (d) revise the World-Sim Kit spec and plan into Draoi and CoW halves; (e) extract DraoiHexWorld core, then terrain, then DraoiGameSpeed; (e-core) ~~DraoiHexWorld core~~ **Done 2026-11-21** ([extract-hexworld-core.md](specs/features/draoi/extract-hexworld-core.md), 6 tasks, full sharded suite green, goldens unchanged): new `Draoi/DraoiHexWorld` project, namespace `Draoi::HexWorld`; also holds `WorldShapeParams`/`BarrierForm`/`WorldProvenance` (provenance lives in `WorldStructure`); 4 tests moved to `Tests/GoogleTests/Draoi/`; **terrain extraction (next) must delete the bridge `CoW/Shared/WorldGeneration/HexWorldAccess.h`** (using-directive for unqualified generator access), move the 3 mixed tests (`TestBiomeRegistryEdge`, `TestStratumTable`, `TestRegionsAndSitesTypes`), and decide the `CoW.Node.*` site ids and `"CoW"` log category still inside DraoiHexWorld; module doc uses layer `draoi` under `draoi.root` (tooling follow-up: docs/refactors/draoi-tooling/plan.md); (f) build the sims in Draoi. **Known noise:** 4 pre-existing iostream precommit errors in GoogleTests (`TestWorldGenDebugDomain`, `TestWorldGenDebugDomainPanel`, `TestWorldJsonExhaustive`, `TestCellLayersIntegrationStreams`); the folder-pass refactor has no commit of its own (swept into `270f4c067` and `f7e2f7d17`). **Decided 2026-11-21:** World-Sim Kit (#3) waits for the Draoi split; Baseline does not overlap (it is the generic-modules home); RandomStream/SaveLoad/SnapshotStore modules deferred (destination Baseline, not Dia); Biome Registry mechanism goes to DraoiHexWorld core with `biomes.json` staying in the app, after two prerequisites: data-driven pin rules (no `Vent`/`Alpine` enum values) and `BiomeIndex`/`DepthZone` moved into core; S-1 generator becomes its own package. **Core prerequisites spec Approved 2026-11-21** ([hexworld-core-prerequisites.md](specs/features/draoi/hexworld-core-prerequisites.md): open pin-rule ids, data-named zones (count stays 3), `TerrainEnums.h` split; 5 tasks, ready to plan). **Step 3 spec drafted 2026-11-21** ([step-chain.md](specs/applications/dia/systems/diadeferredstep/step-chain.md): DiaDeferredStep Feature 6, generic `StepChain`/`IStepStage` names, `applyAfterSteps`, configurable capacity, hard cutover, `WorldStepModule` stays in CoW; needs approval and the AD-001 Dia sign-off). **A1/A3/A4/A5 Draoi amendments drafted 2026-11-21** (each touched spec has a 'Draoi layer amendment' section, plans marked re-plan required; A4/A5 share `HydrologyInputs`; open: A4 influence book and cycles at the boundary, pending-record ownership, stub weather input in Ecology, whether the succession resolver is its own package; F7-F9 and presets carry a pointer line, detail owed at planning; the World-Sim Kit spec/plan revision is still owed). **WorldGeneration audit Done 2026-11-21** (see draoi.md): the split is mechanical; `TerrainEnums.h` splits (core vs `BarrierForm` to terrain), `DepthZone` becomes data-named, and step 2 must also decouple `WorldOverlayGeometry` and `WorldGenViewHost` from `SiteGuarantees`. Step 1 folder grouping: `WorldGen/`, `WorldStep/`, `GameSpeed/`, `F2Harness/`, `BusTools/`, `Plumbing/`, `FrontEnd/`. Terrain and GameSpeed extraction specs Approved 2026-11-21, order Strata -> core -> terrain -> GameSpeed. **Open:** rename Application layer to Cluiche deferred
