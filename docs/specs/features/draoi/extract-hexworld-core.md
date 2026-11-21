# Feature Spec: Extract DraoiHexWorld core

**Parent:** @docs/specs/applications/draoi/draoi.md (extraction order step (e), part 1)
**Status:** `Draft`
**Origin:** Draoi layer audit, 2026-11-21 (package split and WorldGeneration coupling audit in the parent spec).
**Prerequisite:** @docs/specs/features/draoi/hexworld-core-prerequisites.md (Done). The world-sim kit's Draoi half (`WorldCellFields`, `CellSelectorSet`, codec data half) is owned by @docs/specs/applications/cow/systems/worldstep/worldsim-kit.md; this spec only reserves the destination.

## Summary
Move the content-free world foundation out of CoW into a new Draoi project `DraoiHexWorld` (core). It is mechanical and behaviour-preserving: every golden stays byte-identical, and nothing is rewritten beyond include paths and (if decided) namespace.

**Admission test (honest verdict):** the first consumer is CoW. The second is only *credible*, not real: Weather, Hydrology and Ecology (A4/A5/A1) are specced as separate Draoi packages that depend on this core, but they are CoW-authored. No second game exists. The package passes only on the "credible second consumer" leg, and the argument is structural (the sim packages need a home for cell vocabulary that does not depend on the generator). See Open Design Question 1.

## Goals
- `DraoiHexWorld` builds as a static library with no include of CoW, no `Modules/`, and no generator header.
- CoW compiles against it with unchanged behaviour and goldens.
- Tests for core types live in a Draoi test location and still run under `dia run googletest`.

## Non-goals
- The generator, session, JSON and view code (see @docs/specs/features/draoi/extract-hexworld-terrain.md).
- Writing `WorldCellFields`, `CellSelectorSet` or the codec data half: they land in this project when the kit builds them (the kit plan owns the code); this spec only fixes the project and folder.
- Changing any type, field, enum value, JSON key or save format.
- Moving `biomes.json`, `zones.json` or any species/parameter data (content stays in CoW).

## Design

### Files that move (from `Cluiche/CoW/Shared/`)
| From | To (`DraoiHexWorld/`) | Note |
|---|---|---|
| `World/CellVocabulary.h` | `CellVocabulary.h` | `BiomeIndex`, `DepthZone` (index), `WaterSource`, `TerrainType`, `RegionId`, `TerrainFlag` |
| `World/BiomeRegistry.h/.cpp` | `BiomeRegistry.h/.cpp` | mechanism only; `biomes.json` stays in CoW |
| `World/ZoneTable.h/.cpp` | `ZoneTable.h/.cpp` | `zones.json` stays in CoW |
| `WorldGeneration/WorldHexGrid.h/.cpp` | `WorldHexGrid.h/.cpp` | grid type |
| `WorldGeneration/SharedWorldHexGrid.h` | `SharedWorldHexGrid.h` | |
| `World/Testing/BiomeTestRegistry.h`, `ZoneTestIndices.h` | `Testing/` | helper rule below |
| (future) `WorldCellFields`, `CellSelectorSet`, codec data half | `WorldCellFields.h`, `CellSelectorSet.h`, `WorldCodecData.h` | built by the world-sim kit; names indicative |

Tests moving: `TestBiomeRegistry`, `TestBiomeRegistryData`, `TestBiomeRegistryEdge`, `TestBiomeRegistryLoad`, plus any `ZoneTable`/`WorldHexGrid` tests, from `Cluiche/Tests/GoogleTests/CoW/` to a Draoi test folder (Question 2). Tests that exercise the generator through the registry (`TestBiome*` classify/climate/generator, `TestTerrain*`) stay for the terrain spec.

### Project, include root, naming
- New static-library project `DraoiHexWorld.vcxproj` (+ `.filters`) in a Draoi folder, solution folder `Draoi` in `Cluiche.sln` (`dia check sln-sync` must pass). Files are flat in the package; include root `#include <DraoiHexWorld/BiomeRegistry.h>`.
- Depends on Dia only. Add a module doc `dia.draoi.hexworld.architecture.module.md` (`dia.module.v1`), then `dia docs registry` and `dia check deps`.
- `CoW.vcxproj` adds a project reference and include directory; the old CoW paths are deleted, not shimmed. Include sites are updated per file group.
- Helpers live in `DraoiHexWorld/Testing/` per the Draoi `Testing/` rule; GoogleTests includes them from there.

### Namespace
Today `CoW::World::` (the comment in `CellVocabulary.h` says namespaces are unchanged). Decision (user, 2026-11-21): rename to `Draoi::HexWorld` as part of task 5, a separate commit.

## Binding Decisions
- PD-011/PD-012: no include up into CoW, no modules in the package.
- Parent spec package split (agreed 2026-11-21): core holds grid/world types, `BiomeIndex`/`DepthZone`, `BiomeRegistry`, `WorldCellFields`, `CellSelectorSet`, codec data half; nothing from the generator.
- **Layout (user, 2026-11-21):** a top-level `Draoi/` folder beside `Dia/`, one project per package, namespaces `Draoi::HexWorld` (etc.).
- **Tests (user, 2026-11-21):** Draoi tests live in their own `DraoiTests` project and exe, not GoogleTests (GoogleTests is already large). `DraoiTests` links Dia and Draoi only, so the no-CoW rule is enforced by the build. Tests that need CoW data (`biomes.json`, `guarantees.json`, CoW goldens) stay in GoogleTests.
- Escalation rule: a new Draoi project needs user confirmation of placement before Task 1 starts.

## Acceptance Criteria
1. `DraoiHexWorld` compiles with no include path to CoW; `dia check deps` is clean.
2. Full CoW GoogleTests pass; `worldgen_layers.golden.json` and all JSON goldens are byte-identical.
3. No moved header remains under `Cluiche/CoW/Shared/World/` or `WorldGeneration/` and no duplicate exists.
4. Moved tests run from the `DraoiTests` project under `dia run draoitests --filter=...`; CoW-data tests still run under `dia run googletest`.
5. `dia docs precommit` and `dia check sln-sync` pass; module doc and registry updated.

## Tasks
| # | Task | Test | Notes |
|---|------|------|-------|
| 1 | Create project, vcxproj/filters, sln folder, module doc; CoW reference | Empty lib builds; `dia check deps` | Needs user sign-off on the new Draoi project |
| 2 | Move `CellVocabulary`, `ZoneTable`, `WorldHexGrid`, `SharedWorldHexGrid`; update includes | Scoped filters for Zone/Grid/Terrain; goldens | Mechanical |
| 3 | Move `BiomeRegistry` + `Testing/` helpers; update includes | `TestBiomeRegistry*` | Registry must not include generator (true after prerequisites) |
| 4a | `DraoiTests` project and `dia` target `draoitests` (run, launch, build, `--filter`, `--shards`; add to sln, `dia check sln-sync`) | `dia run draoitests` runs an empty then a first moved test | CLI special-cases targets (`launch.py`, `diagnose.py`, `check.py`); needs CLI changes |
| 4b | Move tests that need no CoW data to `DraoiTests`; split mixed files | Filtered runs in both projects | |
| 5 | Namespace change to `Draoi::HexWorld` | Full suite, goldens | Separate commit |
| 6 | Close-out: gap audit, edge tests, one full sharded suite | Full suite | No mutation checks |

## Open Design Questions
1. **Admission.** No second game exists; Weather/Hydrology/Ecology are the credible second consumers. Proceed on that basis, or hold extraction until the first sim package is built so core is shaped by two real clients?
2. **Resolved 2026-11-21:** layout, namespace and separate test project (see Binding Decisions).
3. **Split of mixed test files.** Some biome-registry tests load shipped CoW data; decide per file whether to port them to a Draoi fixture or leave them in GoogleTests.
