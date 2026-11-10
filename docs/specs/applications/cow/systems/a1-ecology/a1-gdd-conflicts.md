# A1 Ecology — GDD Conflicts to Resolve

**For:** the A1 Ecology `/spec-system` interview (agenda). Found 2026-10-18 while reading how A1 connects to S-1, A3, A4, S-2 and S-3.

**Sources:** GDD `08_SystemsDeepDive/A1_Ecology/` (EcologySystem, TechnicalSpec), `A3_BiomeSuccession/` (BiomeSuccession, TechnicalSpec), `10_PrototypeStrategy/S2_SingleCellEcology.md`, `S3_LivingEcology.md`, `02_WorldLore/GeographyAndRegions.md`; Cluiche [worldgenerationpipeline.md](../s1-worldgeneration/worldgenerationpipeline.md), [a4-weather.md](../a4-weather/a4-weather.md).

**All resolved 2026-10-18** in [a1-ecology.md](a1-ecology.md) (Architecture + Deviations from GDD A1). The GDD owns intent (AD-003). Resolutions that change GDD intent are recorded as spec deviations and folded back into the GDD.

## How the pieces connect

```
S-1 ──WorldStructure (GetCurrentWorld, SD-S1-010)──► A1 ◄── GameSpeedController (ticks)
A4  ──WeatherUpdated{week} + WeatherSnapshot──────► A1
A4  ◄─IBiomeSeasonalBaseline (A1 implements)─────── A1
A1  ──BiomeSuccessionTriggered(cell, target, source)──► A3 ──legacy writes──► A1 cell state
S-2: A1 equations on one cell, sliders write cell state; no A3, no A4, no neighbours
S-3: S-1 world + A1 + A3 + A4, no dragons; S-2.5 must pass first
```

Design consequence: the per-cell tick logic must be pure and callable without the module, event wiring or neighbours (S-2), and also run grid-wide fed by A4 (S-3).

## Conflicts

| # | Topic | Conflict | Owner | Status |
|---|-------|----------|-------|--------|
| C1 | Biome vocabulary | S-1 and A1 §5 use the GDD's 8 biomes (Grassland, Temperate Forest, ...). `species.json` examples, S-2 and S-3 use lore region names as biomes (VerdantLowlands, Stormridge, Barren, Ancient Interiors) | GDD | Resolved in spec |
| C2 | Succession-output biomes | A1 also needs Scrub, Burned/Ashland, Desertified Plain. S-1 never produces them, but A4's `IBiomeSeasonalBaseline` must cover them once they appear. Burned/Ashland vs Volcanic/Ashland naming collides | Spec | Resolved in spec |
| C3 | Water cells | No A1 rules for Ocean/Lake cells (aquatic-only ecology, or none?) | GDD | Resolved in spec |
| C4 | Ecology state derivation | Thriving/Stable/Stressed/Collapsed (A1 §4) has no defined formula anywhere. S-2 uses five states (adds Depleted, Recovering) | GDD | Resolved in spec |
| C5 | Tick cadence | TechnicalSpec §1b: one tick = one season. §7/§11: Season/Year/Decade/Century bands with delta_scale 1/4/40/400. GameSpeedController ticks daily with bands. §6 says every cell finishes step N before N+1; §11 pseudo-code loops cell-outer | GDD + Spec | Resolved in spec |
| C6 | `year_quality` | Parked by A4 (PARK-A4-001). A1 contradicts itself: §1c rainfall-only, schema calls it a temperature modifier. S-2 has a year-quality selector but no A4, so it needs a stand-in rainfall source | GDD | Resolved in spec |
| C7 | Succession counters | `succession_pressure` is not in the CellState schema, yet A3 polls it yearly. Only desert and grassland counters are written; forest, wetland, tundra and rainforest triggers in A3 have no A1 counter. `pressure_source` is never assigned | GDD + Spec | Resolved in spec |
| C8 | S-3's A3 model | S-3 describes A3 as Barren→Pioneer→Established→Mature→Ancient + Degraded, which matches nothing in locked A3 (biome transitions + SEEDING/ESTABLISHING/COMPLETE recovery) | GDD (S-3) | Resolved in spec |
| C9 | A3 ticking and ownership | A3 "does not subscribe to SimulationTick" but runs yearly recovery and seasonal visual updates. A3 writes legacy modifiers directly into A1's CellState; field ownership must be explicit | Spec | Resolved in spec |
| C10 | Grid topology | A1 written in axial coords and assumes off-map edges (off-map recolonization). S-1 uses Dia's flat-top odd-q `HexTopology`, hard edges, Small 66×42 | Spec | Resolved in spec |
| C11 | Missing world inputs | A1 needs `slope`, `soil_fertility`, `soil_depth`, `river_overlay`, `corridor_node`, route memory. S-1 has river cells (`waterSource`/`downstream`), `Pass` flag, elevation; no soil; Feature 9 (Regions and Sites) not specced. Decide S-1-supplied vs A1 per-biome defaults | Spec | Resolved in spec |
| C12 | Stale references | S-3's 200×200 perf target; A1 backlog row's DiaSaveGame blob API (rejected 2026-10-15, saves are JSON); S-2 cites wrong A1 sections (§3/§4/§7 → §4/§5, §9, §10) | GDD + Backlog | Resolved in spec |
| C13 | S-2 event FSMs | Drought front, herd migration, predator wave, disease bloom are connected-cell clusters and cannot fire on one cell. "Recolonization" event is not defined in A1 §10 | GDD (S-2) | Resolved in spec |
| C14 | Population units | S-2 sliders are integers 0–500; A1 stores 0–1 fractions of `max_density_per_cell` | Spec | Resolved in spec |

**Out of scope for now:** dragon-facing inputs (step 9, hunts, Full Fight, tended tag) and population tokens. No S-2/S-3 consumer; A1 can define the interface without building them.

**Highest-impact:** C4 (ecology state), C5 (tick cadence), C7 (succession counter ownership).
