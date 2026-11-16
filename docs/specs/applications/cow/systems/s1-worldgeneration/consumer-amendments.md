# Feature Spec: Consumer Amendments

**Parent:** @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md

**Gameplay Domains:** simulation

## Summary

Six changes S-1 owed its downstream consumers, found by the A4 Weather dependency audit and the F-3 audit on 2026-11-13, after S-1 closed `Done` the same day. They are collected into one feature so they share a single build, test run and golden check.

Only the **first** is urgent: the shared biome list gates A4 Feature 1 and A1 Feature 2. The rest are either small cleanups or prerequisites for F-3 World Assets, which is not built.

| # | Amendment | Urgency |
|---|---|---|
| 1 | Widen the shared `Biome` list 10 → 13 | **Gates A4 F1 and A1 F2** |
| 2 | Fix the `WorldGenerationModule` partial-registration name leak | Bug in shipped code |
| 3 | Expose the shared `WorldHexGrid` | Gates A4 F5 and A1 F9 layer registration |
| 4 | A path-accepting export seam for `ITerrainBaker::Bake` | **Moved to F-3 Feature 6 (2026-11-16)** |
| 5 | `cow.worldgen.validate` takes a file | **Moved to F-3 Feature 6 (2026-11-16)** |
| 6 | Mark the F-3 participant section "Not built" | Spec text only, no code |

## Problem

S-1's system spec was marked `Done` on 2026-11-13 with an agreed-but-unbuilt obligation in it and three consumer requirements its built API cannot satisfy. Each was found independently:

- **The biome list.** A1 and A3 *write* biomes (`Scrub`, `BurnedAshland`, `DesertifiedPlain`) that S-1's 10-value enum cannot represent. The user decided 2026-11-13 that there should be **one** biome list, because species affinity JSON, layer palettes, the inspector, saves and T-2's editor all want one vocabulary.
- **The F-3 participant.** `worldgenerationpipeline.md` has a "World assets participant" section (amended 2026-10-20) promising `TerrainParticipant`, `Apply(spec)`, `Capture()`, `GetWorldContentHash()`, `ITerrainBaker::Bake()` and `SetWorldValidated()`. None exist in code, the plan has no task for them, and — unlike the sibling `world_bundle` row — the section carries no "not built" marker, so S-1 reads as though they exist.
- **Two frozen APIs.** S-1's file API is name-only (`[A-Za-z0-9_-]{1,64}`, fixed `out_worlds_dir`), but F-3 needs path-based baking. `cow.worldgen.validate` runs on the current in-memory world only, but four consumers need to validate a baked file on disk.
- **One real bug.** A partial command registration permanently leaks sim-hosted names.

## Goals

1. **One biome list** serving S-1, A4, A1 and A3, with the subset S-1 may generate expressed as a predicate rather than a second type.
2. **No silent name leak** when registration partially fails.
3. **One shared hex grid**, reachable by every system that registers cell layers.
4. **A path-accepting file seam** that coexists with the name-only default.
5. **Honest spec text:** nothing in S-1 claims a capability that does not exist.

## Non-Goals

- **Building the F-3 participant itself.** Amendment 6 marks it; the implementation lands with F-3 Feature 6, in F-3's plan.
- **Changing biome classification behaviour.** The three appended values are never produced by classification. Existing worlds, `biomeIndex` values and goldens are unchanged.
- **Zone or adjacency rules for the succession biomes.** See amendment 1.
- **A second biome type.** Explicitly rejected (see Resolved Design Decisions).

## Amendment 1 — one shared biome list (10 → 13)

Append A3's three succession outputs to the existing enum. **Do not renumber.**

```cpp
// Cluiche/CoW/Shared/WorldGeneration/TerrainEnums.h
enum class Biome : uint8_t {
    Ocean, Lake, GrasslandSteppe, TemperateForest, WetlandDelta,
    DesertBasin, AlpineMountain, TundraBoreal, Rainforest, VolcanicAshland,  // 0-9, generated
    Scrub, BurnedAshland, DesertifiedPlain,                                   // 10-12, A3 only
    Count                                                                     // 13
};
constexpr int kGeneratedBiomeCount = 10;
bool IsGeneratedBiome(Biome b);     // b < Biome::Scrub
```

Appending keeps every serialized `biomeIndex` value and `.wld.json` biome name valid, and leaves `BiomeClassify` untouched — it simply never emits 10–12.

**Zone validity and adjacency stay classification predicates.** `IsBiomeValidInZone` and `AreBiomesForbidden` return `false` for the three succession outputs, and their doc comments say so. Succession may legitimately create adjacencies classification forbids — a `BurnedAshland` next to a `WetlandDelta` after a fire — so these must not be used to validate a live world.

**Acceptance criteria**

1. `Biome::Count == 13`; the first ten values keep their current order and numeric values.
2. `IsGeneratedBiome` is true for exactly indices 0–9.
3. `BiomeId` returns `Biome.Scrub`, `Biome.BurnedAshland`, `Biome.DesertifiedPlain` for the new values, and `BiomeFromId` round-trips all 13.
4. `BiomeName` returns display names for all 13.
5. `IsBiomeValidInZone` and `AreBiomesForbidden` return `false` for the three new values in every zone and pairing.
6. The `biomeIndex` cell layer registers with `categoryCount == 13` and its palette supplies 13 distinct colours.
7. `WorldJsonBiomeName` and `WorldJsonParseBiome` handle all 13; a `.wld.json` naming a succession biome imports without error.
8. **After any generation, every cell satisfies `IsGeneratedBiome(cell.biome)`** — run across all four presets at Small, seeds 1–3. This test is the replacement for the type safety a second enum would have given.
9. `worldgen_layers.golden.json` is **unchanged** — the golden hashes layer values, and `biomeIndex` values stay 0–9. A changed golden means something renumbered.

## Amendment 2 — the partial-registration name leak

`WorldGenerationModule.cpp:636` sets `mCommandsRegistered` to the AND of all 18 registrations, and `UnregisterCommands` (`:641`) early-returns when that is false. So if even one registration fails, the names that *did* register are never unregistered. They persist in the always-on `SimQueryHost`, and because a re-entry then registers nothing (all duplicates) and leaves the flag false, the leak compounds on every stage visit.

DiaCellLayers and DiaCellHistory already roll back correctly on partial failure (`CellLayerViewerDomain.cpp:902`, `SnapshotCommands.cpp:418`), so this is a one-module divergence from an established pattern.

**Acceptance criteria**

10. A forced partial registration failure leaves **zero** `cow.worldgen.*` names registered — the successful ones are rolled back in the same call.
11. Enter → exit → enter on the S-1 stage registers and unregisters the full set each time, with no duplicate-rejection errors on the second entry.
12. The rollback path logs one Error naming the command that failed.

## Amendment 3 — expose the shared `WorldHexGrid`

DiaCellLayers registration requires `CellLayerDesc::grid` to point at a `Dia::Graphics::CellGridDesc`, and S-1, A4 and A1 must all register against the **same** one. Today the grid is created and owned inside `WorldGenViewHost`, which is `#ifdef DIA_DEBUG` and private to S-1's view.

Ownership moves to `WorldGenerationModule`, which exposes it. Scope stays `DIA_DEBUG`, matching its only consumers (every cell-layer registrant is debug-only), so Release pays nothing.

**Acceptance criteria**

13. `WorldGenerationModule` exposes the grid (`GetHexGrid()`), and `WorldGenViewHost` uses that instance rather than creating its own.
14. The grid is rebuilt when world dimensions change (a regenerate at a new size) and every registered layer sees the new `CellGridDesc`.
15. Two independent registrants given `GetHexGrid()` produce layers on one grid — the viewer reports a single grid, not two.
16. In Release the grid is not built and no consumer references it.

## Amendment 4 — a path-accepting export seam

> **Moved to F-3 Feature 6 on 2026-11-16** (`f3-worldassets/consumer-adoption.md`, "S-1 seams moved here"). Not built here: no consumer exists until F-3 does. ACs 17–20 are now that feature's A1–A4.

S-1's built export addresses files by **name** only, inside `out_worlds_dir`. F-3 needs `ITerrainBaker::Bake(absolutePath)` writing to `Global/WorldGen/Baked/<id>.wld.json`, and `Apply({"baked": path})` reading a locator-resolved path. S-1's own spec promises `Bake(absolutePath)` at `:224` without noticing its Feature 4 shipped the name-only form.

Add a path-accepting seam **alongside** the name-only default, which is unchanged: `cow.worldgen.export` keeps writing to `out_worlds_dir` by name.

**Acceptance criteria**

17. A path-accepting export writes a byte-identical `.wld.json` to an arbitrary absolute path, outside `out_worlds_dir`.
18. A path-accepting import reads such a file and behaves exactly as the name-based import (same locked state, same `WorldGenerationComplete`).
19. The name-only command surface is unchanged: `cow.worldgen.export {name}` still refuses path separators.
20. Path traversal outside the caller-supplied root is refused and logged.

## Amendment 5 — `cow.worldgen.validate` takes a file

> **Moved to F-3 Feature 6 on 2026-11-16** (same section). ACs 21–24 are now that feature's B1–B4.

Built as current-world-only with `nullptr, 0` params (`WorldGenerationModule.cpp:608`); `WorldGenSession::Validate` reads no file and refuses with `"no world generated yet"`. Four consumers need the file form — F-3 Features 3 and 6, T-2 Feature 4, and S-3.5 Author mode's "run Validate in game" lock — because the point is validating a **baked file on disk** that is not the loaded world. `SetWorldValidated(relPath, ok)` also needs a relative path the no-arg form cannot produce.

**Acceptance criteria**

21. `cow.worldgen.validate {file}` loads a `.wld.json`, runs the guarantee validators against it and reports the result **without** changing the loaded world or its revision.
22. `cow.worldgen.validate` with no argument keeps its current behaviour exactly.
23. The reply carries the validated file's path relative to its root, so a caller can key a catalogue record.
24. A missing, malformed or dimension-invalid file is a clean failure reply, not a crash or a world change.

## Amendment 6 — mark the F-3 participant section "Not built"

Spec text only. `worldgenerationpipeline.md:216-226` gains a "Not built — F-3 Feature 6" marker in the style of the `world_bundle` row at `:236`, so no reader believes the participant exists.

The implementation stays F-3's. Two items in that section need F-3's attention when it is scheduled, and are recorded here rather than silently inherited:

- **`GetWorldContentHash()`** is specified as FNV-1a 64 over canonical export text. The only hashes in code are the per-layer debug `LayerHash` and test-only helpers — neither is a document hash, neither is stable across formatting.
- **The shared helper placement.** F-3 expects `WorldContentHash(Json)` to live in `CoWWorldAssetsCore`, which would make S-1 link an F-3 library. S-1's spec never agreed to that build dependency. Decide at F-3 Feature 6 whether S-1 links it or owns its own copy.

**Acceptance criteria**

25. The section carries an explicit not-built marker naming F-3 Feature 6.
26. The two open items above are recorded in F-3's Feature 6 spec or its plan, not left only in S-1.

## Public Interfaces

```cpp
namespace CoW::WorldGeneration {
    // Amendment 1
    enum class Biome : uint8_t { /* 13 values, see above */ };
    constexpr int kGeneratedBiomeCount = 10;
    bool IsGeneratedBiome(Biome b);
}

namespace CoW {
    class WorldGenerationModule {
        // Amendment 3 (DIA_DEBUG)
        const WorldGeneration::WorldHexGrid* GetHexGrid() const;
    };
}
```

Amendments 4 and 5 extend existing entry points rather than adding public types; their exact shapes are settled in the plan, because both must preserve the built name-only surface unchanged.

## Binding Decisions

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-001 | StringCRC for ids | `BiomeId` keeps returning `StringCRC`; the enum is the storage form, strings are the JSON form |
| AD-001 | Sign-off for engine edits | **None needed.** Every change is CoW code under `Cluiche/CoW/` |
| AD-003 | GDD owns design intent | The three succession biome ids are A3's existing GDD content; this feature only makes them representable. No new design, nothing owed to the DesignDocs owner |
| SD-S1-010 | World hand-off contract | Unchanged. Amendments 4 and 5 add seams beside the hand-off, never replace it |

## Dependencies

**Requires:** nothing unbuilt. S-1, DiaCellLayers, DiaValueSchema and `SimQueryHostModule` are all built.

**Required by:**
- **A4 Weather Feature 1** and **A1 Ecology Feature 2** — amendment 1 only.
- **A4 Feature 5** and **A1 Feature 9** — amendment 3.
- **F-3 World Assets Features 3, 4 and 6** — amendments 4, 5 and 6.
- **T-2 Feature 4**, **S-3.5 Author mode** — amendment 5.
- **A3** — amendment 1 (`ApplyBiomeTransition` needs the three values to exist).

## Test Strategy

- **Biome:** ACs 1–9 as unit tests, plus the generation-wide invariant (AC 8) across four presets × three seeds at Small, and a golden re-run to prove AC 9.
- **Leak:** a fake registrar whose Nth registration fails, asserting zero names remain (AC 10); a stage re-entry test (AC 11) — the pattern the CellLayers E2E flakes live in.
- **Grid:** two registrants on one grid (AC 15); a regenerate at a different size (AC 14); a Release compile check (AC 16).
- **Paths:** round-trip export/import to a temp directory outside `out_worlds_dir` (ACs 17–18); the name-only surface regression (AC 19); a traversal attempt (AC 20).
- **Validate:** file form against a known-good and a known-bad baked world (ACs 21, 24); the no-arg regression (AC 22); the relative-path reply (AC 23).
- **Close-out:** gap audit and edge-case pass; scoped `--filter` while iterating; one full `--shards 2` run at the end. No mutation checks.

Scoped filter for iteration: `*Biome*:*WorldGen*:*Terrain*`.

## Resolved Design Decisions

1. **One biome list, not two types** (user decision 2026-11-13). A second `RuntimeBiome` (13) beside `Biome` (10) was proposed and rejected: it would have given compile-time proof that classification cannot emit a succession biome, but at the cost of two palettes, two name tables and a mapping call at every system boundary — and `IBiomeSeasonalBaseline` would have had to take the wider type anyway, since A4 declares it and A1 implements it. Cohesion won; AC 8 is the replacement for the lost type safety.
2. **Append, never renumber.** Keeps existing `biomeIndex` values, `.wld.json` names and the committed goldens valid, and leaves `BiomeClassify` untouched.
3. **Zone validity and adjacency remain classification predicates** and return `false` for the three new values, rather than being given invented rules. Succession is not classification.
4. **The name-only file surface is preserved.** Amendments 4 and 5 add seams; they do not migrate the built commands, because three specs and a shipped test suite depend on the current behaviour.
5. **The F-3 participant is marked, not built.** Its consumer does not exist yet, and building an interface with no consumer invites exactly the drift this feature is cleaning up.

## Open Design Questions

1. ~~**Does S-1's system spec stay `Done`?**~~ **Resolved 2026-11-13 (approved):** S-1 keeps `Done`, because its original scope genuinely is complete and this new scope is held in a child feature rather than reopening the system. S-1's Status carries an "Outstanding after close" paragraph naming this spec, and the World assets participant section carries a NOT BUILT banner, so nothing reads as implemented that is not.
2. ~~**Should amendments 4 and 5 be deferred into F-3's plan instead?**~~ **Resolved 2026-11-16 (user): yes, moved into F-3 Feature 6.** They have no consumer until F-3 is built, and consolidating them here was justified by "one build, one test run". If F-3 slips a long way, they are speculative work in S-1's tree. The alternative is shipping amendments 1–3 now and moving 4–5 into F-3 Feature 6. Amendment 1 is urgent either way.
3. **Does the `biomeIndex` palette need designed colours?** AC 6 requires 13 distinct colours. The three succession biomes are visually meaningful — burned, scrubbed, desertified — so the palette may want deliberate hues rather than the next three from the categorical list. Cheap to change later; worth asking whoever owns the layer palettes.

## Status

**Status:** `Approved` (2026-11-13). Found by the A4 Weather dependency audit and the F-3 audit, both the same day, after S-1 closed `Done`. No plan yet — create `consumer-amendments.plan.md` when implementation starts.

**Update 2026-11-16:** amendments 1–3 are built (plan rows 1–9 Done). Amendments 4 and 5 moved to F-3 Feature 6, and AC 26 (the two `GetWorldContentHash` open items) is met there. Amendment 6 was applied 2026-11-13. This feature is complete for S-1's purposes; ACs 17–24 are tracked in F-3.

**Blocking (historical):** A4 Feature 1 and A1 Feature 2 (amendment 1 only). Amendments 2 and 3 are independent; 4, 5 and 6 are F-3 gated.
