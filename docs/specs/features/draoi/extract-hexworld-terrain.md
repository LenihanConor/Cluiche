# Feature Spec: Extract DraoiHexWorldTerrain (S-1 generator)

**Parent:** @docs/specs/applications/draoi/draoi.md (extraction order step (e), part 2)
**Status:** `Draft`
**Origin:** Draoi layer audit, 2026-11-21 (WorldGeneration coupling audit and placement list).
**Prerequisites:** @docs/specs/features/draoi/guarantee-set-injection.md (Approved, in build) must land first; @docs/specs/features/draoi/extract-hexworld-core.md must be Done.

## Summary
Move the S-1 world generator (terrain, biome classification, regions, sites, session, JSON/files) into a Draoi package `DraoiHexWorldTerrain` that depends only on `DraoiHexWorld` core and Dia. CoW keeps content and view code. Behaviour-preserving; goldens byte-identical.

**Admission test (honest verdict):** one real consumer (CoW, with its editor/viewer), no second game. It is the largest reusable gameplay logic in the repo and is content-free once the injection and prerequisite work land, but today it passes only as "credible second consumer" (another hex game would reuse it). Weather/Hydrology/Ecology do **not** depend on it (core only), so they do not count as consumers. See Open Design Question 1.

## Goals
- `DraoiHexWorldTerrain` builds with includes of Dia and core only; no `CoW`, no `Modules/`.
- CoW's world-gen module, viewer host and tests link it with unchanged behaviour.
- Everything CoW-specific is data (`guarantees.json`, `biomes.json`, `zones.json`, terrain rules sidecar) or injected code (`ISitePlacer`s registered by CoW).

## Non-goals
- Changing generation output, JSON, or the registry plug-in path.
- Weather/Hydrology/Ecology packages (separate).
- Moving CoW module wiring (world-gen module, view host, debug domains, commands).

## Design

### Moves to `DraoiHexWorldTerrain/` (from `Cluiche/CoW/Shared/WorldGeneration/`)
- Terrain: `TerrainGenerator`, `TerrainLandmass`, `TerrainMountains`, `TerrainRelief`, `TerrainValleys`, `TerrainRivers`, `TerrainDrainage`, `TerrainClassify`, `TerrainDepthZones`, `TerrainTypes`, `TerrainRules`, `TerrainTableNavigationProfile`, and the generator-only remainder of `TerrainEnums.h` (`BarrierForm`; core owns the cell enums).
- Biomes: `BiomeClassify`, `BiomeClimate` (including the pin-rule table with its five built-ins `water_ocean`, `water_lake`, `vent`, `alpine`, `wetland`).
- Regions and sites: `RegionPartition`, `RegionRepair`, `Regions.h`, `RegionsAndSites`, `Sites`, `SiteRegistry`, `NodesCorridor`, `NodesFill`, `NodesRiver`, `PlaceResult.h`.
- Validation: `ValidationTypes`, `ReachabilityValidator`, `LayoutCheck`, `NavigationQuery`, `WorldLayoutGuarantee`, `GuaranteeSpec` (the generic data-declared rules from guarantee-set-injection).
- Run and session: `WorldGenSession`, `GenerationRun`, `GenerationGuard`, `GenerationReport`, `DeriveSubSeed`.
- I/O and presets (audit: they depend on `SiteRegistry`, `TerrainRules`, `WorldGenSession`): `WorldJson`, `WorldJsonImport`, `WorldFiles`, `WorldShapePreset`, `WorldLayerSource`.
- `Testing/` helpers move to `DraoiHexWorldTerrain/Testing/`, except any that name CoW content: `TestGuarantees.h`, `TestPlacers.h` and `GuaranteeSpecTestHelpers.h` need a per-file check (split, leaving the CoW fixture half in CoW).
- Tests: the matching `Test*` files (Terrain*, generator Biome*, Region*, Validation*, WorldGen session/layer goldens, Navigation, Reachability, GuaranteeSpec) move to the Draoi test location chosen in the core spec; golden fixtures follow their tests.

### Stays in CoW
- View code: `WorldCameraController` (single consumer, fails admission), `WorldOverlayGeometry`, and `Modules/WorldGen*` (module, view host, debug domain, commands).
- Content: `SiteGuarantees.h/.cpp` (whatever remains after injection), CoW `guarantees.json` + schema, `biomes.json`, `zones.json`, terrain parameter files.
- Dragon-layer site placers (`ISitePlacer` implementations registered by CoW in code).

### Project and naming
Static library `DraoiHexWorldTerrain`, include root `#include <DraoiHexWorldTerrain/TerrainGenerator.h>`. Depends on `DraoiHexWorld` and Dia. Module doc, sln folder, registry and `dia check deps` as in the core spec. Namespace follows the core spec's decision.

## Binding Decisions
- PD-011/PD-012: no modules, never includes CoW.
- Parent spec split: terrain -> core is a one-way dependency; Weather, Hydrology, Ecology must not depend on terrain.
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
| 6 | Move `Testing/` helpers and tests; split CoW-flavoured fixtures | Filtered runs | |
| 7 | Close-out: gap audit, one full sharded suite, world-gen stage smoke | Full suite | No mutation checks |

## Open Design Questions
1. **Admission.** One real consumer. Extract now (the audit placed it and it is clean after injection) or hold until a second hex game or tool needs it, extracting only core meanwhile?
2. **Overlay geometry.** `WorldOverlayGeometry` stays as view code. Once marker overlays are data-driven it may be generic enough for the package; keep in CoW until a second viewer exists?
3. **Package naming.** The parent table lists the generator under `DraoiHexWorld`; the 2026-11-21 split makes it a separate package. Confirm `DraoiHexWorldTerrain` (it also holds regions, sites and validation).
