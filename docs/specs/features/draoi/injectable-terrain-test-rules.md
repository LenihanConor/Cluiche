# Feature Spec: Injectable terrain test rules (Draoi test helpers)

**Parent:** @docs/specs/features/draoi/extract-hexworld-terrain.md (Follow-up 3, raised at its close-out 2026-11-22; plan task 7 left these files in CoW)
**Status:** `Approved` (2026-11-23; user delegated both open questions: injection seam and forwarders chosen). Plan: @docs/specs/features/draoi/injectable-terrain-test-rules.plan.md
**Origin:** Backlog Task row "Terrain test helpers need injectable rules" (2026-11-22).
**Depends on:** nothing unbuilt. Mirrors the existing `SetTestAssetPaths` seam in `DraoiHexWorld/Testing/BiomeTestRegistry.h` (host supplies repo-relative paths, `CoWTestAssetPaths.cpp` calls it).

## Summary
Three test helpers and three tests stayed in CoW because they call `TestRules()` / `TestSession`, which read the CoW terrain-rules sidecar and install CoW step seed names, node-kind ids and the shipped guarantee spec. Make those four inputs host-injected through a neutral Draoi test helper (same pattern as `SetTestAssetPaths`), keep a thin CoW half that supplies the CoW values, then move the helpers and tests into Draoi. No production code changes; behaviour, goldens and test counts are unchanged.

## Goals
- `Draoi/DraoiHexWorldTerrain/Testing/` holds a neutral `TerrainTestHost.h`: host-set config plus `TestSidecarText()`, `TestSidecarFile()`, `TestRules()` and `TestSession`, with no `CoW` identifier.
- `CoWTerrainRulesTestHelpers.h` shrinks to the CoW half: a one-time registration of the CoW sidecar path, `CoWStepSeedNames()`, `CoWNodeKindIds()` and the shipped guarantee spec.
- `BiomeTestGrids.h`, `TerrainSliderDriver.h` and `WorldJsonTestWorlds.h` split into a neutral part (moves to `Draoi/DraoiHexWorldTerrain/Testing/`) and a CoW remainder (only what names `CoWSiteRegistry` / CoW node kinds in `WorldJsonTestWorlds.h`).
- `TestBiomeRegistryEdge`, `TestStratumTable` and `TestRegionsAndSitesTypes` run from `Tests/GoogleTests/Draoi/`; any assertion naming CoW content (`"CoW.Node.*"`, `Dragon.Hearth`, `CoW.Regions` seed names) stays in a CoW test file.

## Non-goals
- Moving the ~60 other terrain tests that still use `TestRules()`/`TestSession`; they keep working through the same names and can move later (each is a mechanical follow-up once this lands).
- Changing production `TerrainRules`, `WorldGenSession` or any sidecar.
- A Draoi-owned neutral sidecar fixture (see Open Design Question 1).

## Design

### Neutral half (new, `Draoi::HexWorld::Testing`)
```
struct TerrainTestConfig
{
    std::string      rulesSidecarPath;   // repo-relative, e.g. the CoW terrain_rules.schema.json
    StepSeedNames    stepSeedNames;
    NodeKindIds      nodeKinds;
    const GuaranteeSpec* (*guaranteeSpec)();   // lazy: the shipped spec, installed into TestSession
};
void SetTerrainTestConfig(const TerrainTestConfig&);
const std::string&                 TestSidecarText();
const Dia::Attribute::ParameterFile& TestSidecarFile();
TerrainRules                       TestRules();
struct TestSession : WorldGenSession { TestSession(); };   // installs seed names, node kinds, biome registry, guarantees, rules
```
Same abort-with-message behaviour as today when the config was never set or the file is missing. `ReadFileFromSourceTree` stays the file lookup. The config is set once before any test runs, exactly where `SetTestAssetPaths` is set today (`CoWTestAssetPaths.cpp`).

### CoW half (stays in `Shared/WorldGeneration/CoWTesting/`)
`CoWTerrainRulesTestHelpers.h` keeps its name and its `CoW::WorldGeneration::TestRules()` / `TestSession` spellings as `using` forwarders to the neutral ones, so the ~60 unmoved tests compile unchanged. `CoWTestAssetPaths.cpp` also calls `SetTerrainTestConfig` with the CoW values.

### Splits
| File | Neutral (moves) | CoW remainder |
|---|---|---|
| `BiomeTestGrids.h` | all of it (only `TestRules()` is CoW-bound; goes through the neutral helper) | none |
| `TerrainSliderDriver.h` | all of it | none |
| `WorldJsonTestWorlds.h` | `MakeCell`, `Same*`, `MakeTinyWorld`, `GenerateWorld` taking an injected `SiteRegistry` | `CoWTestSiteRegistry()`, `TestImportContext()`, the CoW node-kind ids used by `MakeTinyWorld` sites (parameterised by `NodeKindIds`) |

`WorldJsonTestWorlds.h` is the only non-trivial split: `MakeTinyWorld` places sites with `BuiltinNodeKinds::Corridor()` etc., so it takes the node-kind ids from the test config instead.

## Binding Decisions
- PD-011/PD-012: nothing in `Draoi/` names `CoW`, `Hearth`, `Nexus`, `Throne` or `Dynasty` (grep check, as in the parent spec AC 1). The existing `SetTestAssetPaths` precedent already lets Draoi tests read CoW data through host-supplied paths, so a repo-relative CoW path set by the host is allowed; a hard-coded one in `Draoi/` is not.
- Test helpers live in `<Module>/Testing/`, not GoogleTests (project convention).

## Acceptance Criteria
1. `Draoi/` grep for `CoW`, `Hearth`, `Nexus`, `Throne`, `Dynasty` returns nothing new.
2. The three helpers live in `Draoi/DraoiHexWorldTerrain/Testing/` (neutral parts) and the three tests in `Tests/GoogleTests/Draoi/`; all registered in the vcxproj and filters.
3. Total test count and pass count are unchanged (CoW-name assertions split out, not dropped); `worldgen_layers.golden.json` and the JSON export goldens are byte-identical.
4. A test that never sets the config aborts with a clear message rather than reading a stale path.
5. `dia check deps`, `dia check arch`, `dia check sln-sync` and `dia docs precommit` pass.

## Tasks
| # | Task | Test | Notes |
|---|------|------|-------|
| 1 | Add neutral `TerrainTestHost.h` (config, `TestRules`, `TestSidecar*`, `TestSession`); reduce `CoWTerrainRulesTestHelpers.h` to forwarders; register the CoW config in `CoWTestAssetPaths.cpp` | RED-GREEN: unset-config abort message; full `TerrainRules`/`WorldGenSession` suites unchanged (`--filter=TerrainRules*:WorldGenSession*`) | Pins the seam |
| 2 | Move `BiomeTestGrids.h` and `TerrainSliderDriver.h` to Draoi Testing; update includes in the ~24 dependent tests | `--filter=Biome*:Terrain*` | Mechanical |
| 3 | Split `WorldJsonTestWorlds.h`; move the neutral half | `--filter=WorldJson*:WorldGenSessionImport*` | Node kinds injected |
| 4 | Move `TestBiomeRegistryEdge` and `TestStratumTable` to `Tests/GoogleTests/Draoi/` | their own suites | CoW-name assertions to a CoW file |
| 5 | Split `TestRegionsAndSitesTypes` (CoW node-kind names stay) and move the neutral half | its suite | |
| 6 | Close-out: grep check, `dia check deps/arch/sln-sync`, `dia docs precommit`, one full sharded suite, gap audit | Full suite | No mutation checks |

## Open Design Questions
1. **Seam vs Draoi-owned fixture.** Host-injected config (this spec) reuses the CoW sidecar, so Draoi tests exercise real rules but cannot run without a CoW checkout path. A small Draoi-owned neutral sidecar would make the Draoi slice self-contained (matters for the "split GoogleTests" backlog item) but forks the rules and can drift. This spec chooses injection, consistent with `SetTestAssetPaths`; revisit when GoogleTests is split.
2. **Forwarders vs bulk include rewrite.** Keeping `CoW::WorldGeneration::TestRules()` forwarders avoids touching ~60 tests now but leaves two spellings. Alternative: rewrite all includes in one mechanical pass in task 2.
