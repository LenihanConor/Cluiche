**Spec:** @docs/specs/features/draoi/extract-hexworld-terrain.md
**Status:** In Progress

Decisions (2026-11-22, user): the package holds no CoW strings; CoW-specific values are injected, not excepted from the grep check. Package name `DraoiHexWorldTerrain` confirmed (Open Design Question 1) and approved (task 2 sign-off). Namespace `Draoi::HexWorld` (as core).

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Confirm guarantee-set-injection Done and core extracted; grep the files-to-move for CoW identifiers | Grep clean | Done | sonnet | Gate passed 2026-11-22: no includes outside WorldGeneration. Remaining CoW strings: log category "CoW" (~15 sites: BiomeClassify, GenerationRun, GuaranteeSpec, ReachabilityValidator...), reseed ids `CoW.Regions`, `CoW.Node.Corridor`, `CoW.Node.Fill` (RegionsAndSites.cpp), comments naming Hearth/Nexus/Dynasty (GuaranteeSpec.h), test fixtures `Testing/GuaranteeSpecTestHelpers.h` (`CoW.Site.*`), `WorldJsonTestWorlds.h` (`Dragon.Hearth`) |
| 2 | Create `Draoi/DraoiHexWorldTerrain` static-lib project, vcxproj/filters, module doc, sln entry (Draoi folder); CoW and GoogleTests project references and include dirs (template: commit `bd8ce4dcf`) | Empty lib builds; `dia check deps`; `dia check sln-sync` | Done | sonnet | Sign-off given 2026-11-22. BiomeRegistry* 47 pass, deps OK (40 modules), sln-sync in sync. Module doc maturity alpha; `dia check deps` regenerated diaarchitecture.folders.md (+2 lines) |
| 2b | Inject CoW specifics: (a) reseed step names (`Regions`, `Node.Corridor`, `Node.Fill`) become inputs with neutral defaults (`HexWorld.*`), CoW passes its current strings so RNG streams and goldens stay byte-identical; (b) log category becomes `"HexWorld"` (update log-capture tests); (c) reword Hearth/Nexus/Dynasty comments; (d) split CoW-flavoured test fixtures (`GuaranteeSpecTestHelpers.h`, `TestGuarantees.h`, `TestPlacers.h`, `WorldJsonTestWorlds.h`) into neutral (moves) and CoW (stays) halves. Done in place, before the moves | Scoped Region*/Guarantee*/WorldGen* filters; goldens byte-identical | Not Started | sonnet | Added 2026-11-22 |
| 3 | Move terrain + biome classification files and the generator-only `BarrierForm` remainder | Terrain/Biome filtered suites, goldens | Not Started | sonnet | Mechanical |
| 4 | Move regions, sites, nodes, validation | Region/Validation/Reachability suites | Not Started | sonnet | |
| 5 | Move session, run, guard, report, JSON, files, presets, layer source | WorldGen* suites, goldens | Not Started | sonnet | |
| 6 | Move `WorldOverlayGeometry` (+ its tests); delete `HexWorldAccess.h` bridge; move the 3 mixed tests (`TestBiomeRegistryEdge`, `TestStratumTable`, `TestRegionsAndSitesTypes`); grep `CoW`, `Hearth`, `Nexus`, `Throne`, `Dynasty`, `SiteGuarantees` clean | Overlay/marker suites, grep clean | Not Started | sonnet | Decision 2 |
| 7 | Move `Testing/` helpers and tests (neutral halves); CoW keeps its fixtures | Filtered runs | Not Started | sonnet | |
| 8 | Close-out: gap audit, one full sharded suite (`--shards >= 2`), world-gen stage smoke | Full suite | Not Started | sonnet | No mutation checks |
