# Feature Spec: Extract DraoiHexWorldTerrain (S-1 generator)

**Parent:** @docs/specs/applications/draoi/draoi.md (extraction order step (e), part 2)
**Status:** `Approved` (2026-11-21; decisions recorded from the user)
**Origin:** Draoi layer audit, 2026-11-21 (WorldGeneration coupling audit and placement list).
**Prerequisites:** @docs/specs/features/draoi/guarantee-set-injection.md must have landed (confirm in task 1).
**Depends on / ordering:** Strata -> core -> terrain -> GameSpeed. The Strata feature (@docs/specs/features/draoi/generic-strata.md, Approved 2026-11-21, not yet built) lands first (`TerrainDepthZones` becomes `TerrainStrata` with the `IStrataAssigner` seam, stays in this package); then @docs/specs/features/draoi/extract-hexworld-core.md must be Done; this spec is third; @docs/specs/features/draoi/extract-draoigamespeed.md is last.

## Summary
Move the S-1 world generator (terrain, biome classification, regions, sites, session, JSON/files) into a Draoi package `DraoiHexWorldTerrain` that depends only on `DraoiHexWorld` core and Dia. CoW keeps content and view code. Behaviour-preserving; goldens byte-identical.

**Admission test (decided 2026-11-21, see Binding Decisions):** one real consumer today (CoW, with its editor/viewer); the user is building a second game and the generator is already being made generic, so it is extracted now. Weather/Hydrology/Ecology do **not** depend on it (core only), so they do not count as consumers. Extract what exists; no speculative API.

## Goals
- `DraoiHexWorldTerrain` builds with includes of Dia and core only; no `CoW`, no `Modules/`.
- CoW's world-gen module, viewer host and tests link it with unchanged behaviour.
- Everything CoW-specific is data (`guarantees.json`, `biomes.json`, `strata.json`, terrain rules sidecar) or injected code (`ISitePlacer`s registered by CoW).

## Non-goals
- Changing generation output, JSON, or the registry plug-in path.
- Weather/Hydrology/Ecology packages (separate).
- Moving CoW module wiring (world-gen module, view host, debug domains, commands).

## Design

### Moves to `DraoiHexWorldTerrain/` (from `Cluiche/CoW/Shared/WorldGeneration/`)
- Terrain: `TerrainGenerator`, `TerrainLandmass`, `TerrainMountains`, `TerrainRelief`, `TerrainValleys`, `TerrainRivers`, `TerrainDrainage`, `TerrainClassify`, `TerrainStrata` (was `TerrainDepthZones`; Strata feature, with `IStrataAssigner`), `TerrainTypes`, `TerrainRules`, `TerrainTableNavigationProfile`, and the generator-only remainder of `TerrainEnums.h` (`BarrierForm`; core owns the cell enums).
- Biomes: `BiomeClassify`, `BiomeClimate` (including the pin-rule table with its five built-ins `water_ocean`, `water_lake`, `vent`, `alpine`, `wetland`).
- Regions and sites: `RegionPartition`, `RegionRepair`, `Regions.h`, `RegionsAndSites`, `Sites`, `SiteRegistry`, `NodesCorridor`, `NodesFill`, `NodesRiver`, `PlaceResult.h`.
- Validation: `ValidationTypes`, `ReachabilityValidator`, `LayoutCheck`, `NavigationQuery`, `WorldLayoutGuarantee`, `GuaranteeSpec` and `SiteRules` (the generic data-declared guarantee rules and site-kind rules; site kinds are data-declared, not compiled in). `SiteGuarantees.*` no longer exists (deleted); guarantees are data via `GuaranteeSpec` + `SiteRules`.
- Overlay: `WorldOverlayGeometry` (moves, decision 2): its `SiteGuarantees` dependency is gone and the marker overlay is spec-driven via `BuildMarkers`.
- Run and session: `WorldGenSession`, `GenerationRun`, `GenerationGuard`, `GenerationReport`, `DeriveSubSeed`.
- I/O and presets (audit: they depend on `SiteRegistry`, `TerrainRules`, `WorldGenSession`): `WorldJson`, `WorldJsonImport`, `WorldFiles`, `WorldShapePreset`, `WorldLayerSource`.
- `Testing/` helpers move to `DraoiHexWorldTerrain/Testing/`, except any that name CoW content: `TestGuarantees.h`, `TestPlacers.h` and `GuaranteeSpecTestHelpers.h` need a per-file check (split, leaving the CoW fixture half in CoW).
- Tests: the matching `Test*` files (Terrain*, generator Biome*, Region*, Validation*, WorldGen session/layer goldens, Navigation, Reachability, GuaranteeSpec) move to the Draoi test location chosen in the core spec; golden fixtures follow their tests.

### Stays in CoW
- View code: `WorldCameraController` (single consumer, fails admission) and `Modules/WorldGen*` including `WorldGenViewHost` (module, view host, debug domain, commands).
- Content: CoW `guarantees.json` + schema, `biomes.json`, `strata.json`, terrain parameter files, and the site-kind declarations in data.
- Dragon-layer site placers (`ISitePlacer` implementations registered by CoW in code).

### Project and naming
Static library `DraoiHexWorldTerrain`, include root `#include <DraoiHexWorldTerrain/TerrainGenerator.h>`. Depends on `DraoiHexWorld` and Dia. Module doc, sln folder, registry and `dia check deps` as in the core spec. Namespace follows the core spec's decision.

## Binding Decisions
- PD-011/PD-012: no modules, never includes CoW.
- Parent spec split: terrain -> core is a one-way dependency; Weather, Hydrology, Ecology must not depend on terrain.
- **Decision 1 (2026-11-21): extract the terrain package now.** Admission: one real consumer today, the user is building a second game, and the generator is already being made generic. Ordering: after the Strata feature and after the core extraction. Extract what exists; no speculative API.
- **Decision 2 (2026-11-21): `WorldOverlayGeometry` moves to the terrain package.** The audit kept it in CoW only because it depended on `SiteGuarantees`; that dependency is gone (marker overlay is spec-driven via `BuildMarkers`). The extraction must confirm it has no CoW content or includes left. `WorldCameraController` and `WorldGenViewHost` stay in CoW (module / single consumer).
- Mechanism in the library, content in the application: no `Hearth`, `Nexus`, `Throne`, `Dynasty` or `CoW.*` identifier in the package (grep check, as in guarantee-set-injection AC 6).

## Acceptance Criteria
1. The package includes only Dia and `DraoiHexWorld`; grep for `CoW`, `Hearth`, `Nexus`, `Throne`, `Dynasty` in the package returns nothing.
2. All CoW GoogleTests pass; `worldgen_layers.golden.json` and JSON export goldens are byte-identical.
3. CoW's `guarantees.json` loads through the package unchanged; an empty spec still generates.
4. The viewer host, module and the world-gen stage launch and behave as before (E2E smoke).
5. `dia check deps`, `dia check sln-sync`, `dia docs precommit` pass.

## Tasks
| # | Task | Test | Notes |
|---|------|------|-------|
| 1 | Confirm guarantee-set-injection Done and core extracted; grep the files-to-move for CoW identifiers | Grep clean | Gate; block if not |
| 2 | Create project, module doc, sln folder; reference from CoW | Empty lib builds | User sign-off on the new package |
| 3 | Move terrain + biome classification files and the `TerrainEnums` remainder | Terrain/Biome filtered suites, goldens | Mechanical |
| 4 | Move regions, sites, nodes, validation | Region/Validation/Reachability suites | |
| 5 | Move session, run, guard, report, JSON, files, presets, layer source | WorldGen* suites, goldens | |
| 6 | Move `WorldOverlayGeometry` (+ its tests); confirm no CoW content or includes remain (grep `CoW`, `Hearth`, `Nexus`, `Throne`, `Dynasty`, `SiteGuarantees`); if any remain, split the CoW half out | Overlay/marker suites, grep clean | Decision 2 |
| 7 | Move `Testing/` helpers and tests; split CoW-flavoured fixtures | Filtered runs | |
| 8 | Close-out: gap audit, one full sharded suite, world-gen stage smoke | Full suite | No mutation checks |

## Open Design Questions
1. **Package naming.** The parent table lists the generator under `DraoiHexWorld`; the 2026-11-21 split makes it a separate package. Confirm `DraoiHexWorldTerrain` (it also holds regions, sites, validation and overlay geometry).
