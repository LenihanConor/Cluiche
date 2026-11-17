# Plan: S-1 Consumer Amendments

**Spec:** @docs/specs/applications/cow/systems/s1-worldgeneration/consumer-amendments.md
**Status:** Done

Six amendments, sequenced rather than bundled. **Amendment 1 (the shared biome list) runs first and alone**, because it gates A4 Feature 1 and A1 Feature 2 and nothing else in this feature does. Amendments 4 and 5 are F-3 gated and deliberately last — see spec Open Design Question 2, which asks whether they belong in F-3's plan instead. Decide that before starting row 8.

Amendment 6 (the NOT BUILT marker on the World assets participant section, plus S-1's "Outstanding after close" note) was **applied on 2026-11-13 when the spec was written**, because it corrected text that actively misled. AC 25 is already met; AC 26 remains.

## Amendment 1 — shared biome list (ACs 1–9)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | RED: assertions in `TestBiomeTypes.cpp` for ACs 1–5 — `Biome::Count == 13`; the first ten values keep their current order *and* numeric values; `IsGeneratedBiome` true for exactly 0–9; `BiomeId`/`BiomeFromId` round-trip all 13; `BiomeName` non-"Unknown" for all 13; `IsBiomeValidInZone` and `AreBiomesForbidden` false for the three new values in every zone and pairing | Quoted RED output showing compile failure on `Biome::Scrub` and the new assertions failing | Done | sonnet | TDD per `.claude/skills/tdd.md`. `IsGeneratedBiome` and `kGeneratedBiomeCount` are new public API, so RED is mandatory. Pin numeric values explicitly (`static_cast<uint8_t>(Biome::VolcanicAshland) == 9`) — that assertion is what protects the goldens; Done 2026-11-16 |
| 2 | GREEN: append `Scrub`, `BurnedAshland`, `DesertifiedPlain` to `Biome` in `TerrainEnums.h`; add `kGeneratedBiomeCount = 10` and `IsGeneratedBiome(Biome)`; fill the **four** exhaustive switches; fix `BiomeTypes.h`'s header comment (it says the enum is in `TerrainTypes.h`; it is in `TerrainEnums.h`) | Task 1 assertions green; scoped `*Biome*` run clean | Done | sonnet | **The four switches are the real risk:** `BiomeTypes.cpp:10` (`BiomeIdString`), `:40` (`BiomeName`), `:73` (`IsBiomeValidInZone`), `BiomeClassify.cpp:120`. Each currently ends `case Biome::Count: break;` and falls through to a default return, so a missing case is **silent, not a compile error**. Add all three cases to each and confirm no `default:` label hides a gap. Zone/forbidden return `false` for the three, with the doc comment saying they are classification predicates; Done 2026-11-16 |
| 3 | AC 6: confirm `WorldLayerSource.cpp:156` auto-derives `categoryCount` to 13 (it uses `static_cast<uint32_t>(Biome::Count)` — expect **no edit**), then add three `values` entries to the `biomeIndex` enum in `Cluiche/Assets/CoW/Stages/S1WorldGenerationTestbedStage/worldgen.schema.json` (~line 765) | A test asserting the registered `biomeIndex` layer reports `categoryCount == 13`; `dia validate schema` clean | Done | sonnet | Palette colours: spec Open Question 3 asks whether the three want deliberate hues (burned / scrubbed / desertified) rather than the next three categorical entries. **Pick deliberate hues and flag them for the user's eye** — this is the one visual judgement in the amendment; Done 2026-11-16 |
| 4 | AC 7: add the three names to `WorldJsonBiomeName` (`WorldJson.cpp:147`); confirm `WorldJsonParseBiome` auto-updates via `kBiomeCount` (expect no edit) | Export/import round-trip of a hand-built world containing a `Scrub` cell preserves it; an unknown biome name still fails cleanly | Done | sonnet | Use `Testing/WorldJsonTestWorlds.h`. This is what makes a future "export the live post-succession world" possible rather than blocked |
| 5 | AC 8: the generation-wide invariant — after any generation **every** cell satisfies `IsGeneratedBiome(cell.biome)`, across all four presets at Small, seeds 1–3 | New test in `TestBiomeAcceptance.cpp`, 12 worlds | Done | sonnet | This is the replacement for the type safety the rejected two-enum design would have given. It is the single most important test in the amendment — without it nothing stops classification emitting a succession biome; Done 2026-11-16 |
| 6 | AC 9: re-run the S-1 layer goldens and confirm `worldgen_layers.golden.json` is **unchanged** | `dia run googletest --filter="*Golden*"` with quoted output | Done | haiku | Expected result: unchanged, because `biomeIndex` values stay 0–9. **A changed golden means task 2 renumbered something** — stop and diagnose rather than running `DIA_UPDATE_GOLDEN=1`; Done 2026-11-16 |
| 7 | Close-out: gap audit of ACs 1–9, edge cases (`Biome::Count` passed to each helper, an out-of-range cast, `BiomeFromId` with a near-miss id), then scoped `--filter="*Biome*:*WorldGen*:*Terrain*"` and one full `dia run googletest --shards 2` | Gap list in notes; both runs quoted | Done | sonnet | No mutation check ([[feedback-no-mutation-checks]]). Observation scan: the five pillars over every file touched — expect nothing, since this is vocabulary, but record the scan; Done 2026-11-16 |

**Amendment 1 is Done when rows 1–7 are Done.** At that point A4 Feature 1 and A1 Feature 2 are unblocked; say so in the commit.

## Amendments 2 and 3 (ACs 10–16)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 8 | Amendment 2, ACs 10–12: roll back partial command registration in `WorldGenerationModule` so a failure leaves zero names registered; one Error naming the failing command | A fake registrar failing the Nth registration leaves zero names; enter → exit → enter registers cleanly with no duplicate-rejection errors | Done | sonnet | Real bug in shipped code. `WorldGenerationModule.cpp:636` sets `mCommandsRegistered = ok` (AND of 18) and `:641` early-returns on `!mCommandsRegistered`. Copy the established pattern from `CellLayerViewerDomain.cpp:902` / `SnapshotCommands.cpp:418`. AC 11 is the stage-re-entry case the CellLayers E2E flakes live in; Done 2026-11-16 |
| 9 | Amendment 3, ACs 13–16: move `WorldHexGrid` ownership to `WorldGenerationModule` with `GetHexGrid()`; `WorldGenViewHost` consumes it rather than creating its own; rebuild on a dimension change; `DIA_DEBUG` scope | Two registrants on one grid report a single grid; a regenerate at a new size refreshes every layer's `CellGridDesc`; Release compile check | Done | sonnet | `WorldHexGrid` is non-copyable and non-movable — store behind a `unique_ptr` or construct in place. Gates A4 F5 and A1 F9; Done 2026-11-16 |

## Amendments 4 and 5 (ACs 17–24) — F-3 gated

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 10 | **Decision gate, not code:** resolve spec Open Design Question 2 — do amendments 4 and 5 stay here or move into F-3's plan? They have no consumer until F-3 is built | — | Done | — | Ask the user. Building an interface with no consumer is what produced the drift this whole feature is cleaning up; **Decided 2026-11-16 (user): move 4 and 5 into F-3 Feature 6** |
| 11 | Amendment 4, ACs 17–20: a path-accepting export/import seam beside the unchanged name-only default; refuse traversal outside the caller's root | Round-trip to a temp dir outside `out_worlds_dir`; the name-only surface still refuses path separators | Moved | sonnet | Blocked on row 10; **Moved to F-3 Feature 6 (consumer-adoption.md, "S-1 seams moved here")** |
| 12 | Amendment 5, ACs 21–24: `cow.worldgen.validate {file}` loads a `.wld.json` and validates it without touching the loaded world or its revision; reply carries the root-relative path | File form against known-good and known-bad worlds; the no-arg form unchanged | Moved | sonnet | Blocked on row 10. The relative path is what lets `SetWorldValidated(relPath, ok)` key a catalogue record; **Moved to F-3 Feature 6 (consumer-adoption.md, "S-1 seams moved here")** |
| 14 | Amendment 8 (added 2026-11-17, A5 Hydrology), ACs 32–36: persist bed elevation and the basin table in `.wld.json`, bump `cow.world` to version 2, read both back on import | Export → import → export byte-identical; imported, `world_file` and fresh worlds give identical bed elevation and basin tables; a malformed basin table fails cleanly; goldens unchanged | Pending | sonnet | Proposed, not started. Settle spec Open Question 4 (version 1 files) first. Needs `json-export-import.md` updated. Gates A5 for imported worlds. Plan Status stays Done for amendments 1–7's rows; this row is new work |
| 13 | AC 26: record the two `GetWorldContentHash` open items (FNV-1a over canonical export text; whether S-1 links `CoWWorldAssetsCore`) in F-3 Feature 6's spec or plan | — | Done | haiku | AC 25 already met 2026-11-13; Done 2026-11-16: both items recorded in F-3 consumer-adoption.md |

## Rules for this plan

- **Commit after each task**, with the plan row updated in the same commit.
- Update rows with `dia docs plan docs/specs/applications/cow/systems/s1-worldgeneration/consumer-amendments.plan.md <#> --status <S>`.
- Scoped `--filter` while iterating; one full `--shards 2` run at the end of each amendment, not per task.
- No task on opus. Ask the user before changing that.
