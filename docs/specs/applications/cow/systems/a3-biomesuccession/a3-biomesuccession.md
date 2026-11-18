# System Spec: A3 Biome Succession

**Parent:** @docs/specs/applications/cow/cow.md  
**Design source:** `Output Docs/GDD/08_SystemsDeepDive/A3_BiomeSuccession/` (DesignDocs repo; `A3_BiomeSuccession.md`, `A3_TechnicalSpec.md`, `A3_TuningParameters.md`, `A3_EdgeCaseMatrix.md`, `A3_PrototypeTestbed.md`, `A3_ProductionBacklog.md`, Locked; amended by CHANGE-036, pending fold-back by the GDD session; the A5 rules are in GDD A5 TechnicalSpec 14.4, CHANGE-048, and the GDD A3 files have not yet been edited)  
**Works with:** @docs/specs/applications/cow/systems/a1-ecology/a1-ecology.md (A1 evaluates the counters; A3 decides the transitions)
**Amended 2026-11-17, A5 Hydrology:** A5 ([a5-hydrology.md](../a5-hydrology/a5-hydrology.md)) now produces the water signals A3's rules need: sustained flooding, exposed lakebed, sustained low river flow and glacier mass. A3 gains rules over them, and Lake is no longer a "no transitions" biome. A5 never writes biome; A3 flips it. First-run thresholds come from the locked GDD A5 (aligned to locked GDD 2026-11-17, CHANGE-048; the GDD A3 documents do not yet carry these rules, the source is GDD A5 TechnicalSpec 14.4).
**Amended 2026-11-18, A1 ecology expansion:** A1's producers, detritus and nutrients are now flow-graph nodes, so rule conditions and legacy deltas name **node ids**: `grass` → `producer.grass`, `tree` → `producer.tree`, `aquatic` → `producer.aquatic`, `soil_fertility` → `nutrient.soil`. The shipped `succession_rules.json` is rewritten to match, and A1's loader rejects an unknown field id. **Tuning note:** the fire legacy's `nutrient.soil +` delta and A1's new ash release (N1, `ash_nutrient_fraction`) both add fertility after a fire; tune one down, or drop the direct delta, during S-3 Living-mode tuning. Rules may now also read `nutrient.soil` in conditions (for example desertification).

**Gameplay Domains:** debug

## Summary

A3 decides when and how a cell's **biome identity** changes. A1 tracks the stress and the recovery conditions as counters, and A3 owns three things:

- **The rules:** which transitions exist (the staging table), and the contents of the succession rule table.
- **What each transition leaves behind:** legacy deltas and a timed legacy marker.
- **How it reads:** the visual state, recovery stages, and dragon acceleration.

A3 is pure CoW-Shared logic that A1's pipeline calls **synchronously** through an `ISuccessionResolver` seam, at A1's step 10 and at the Year and Season bands. Transitions apply through A1's API in the same band, so they're deterministic, and the biome changes between pipeline steps. They never wait a frame.

Degradation is always staged and recovery is always slower. Near-floor biomes (Desert, Alpine, Volcanic) never change biome; they only collapse within it. A3 records history; it doesn't punish.

## Goals

1. **Staging enforced.** Every transition is checked against the staging table, and invalid ones are rejected and logged (EdgeCase 1.1, 4.1, 4.2). A cell can't skip a waypoint.
2. **One counter mechanism.** Forward and recovery paths are both rules in `succession_rules.json`, which A3 authors and A1 evaluates. A3 adds the recovery stages and the per-cell multipliers.
3. **Same-band, deterministic resolution.** Triggers are resolved in cell-index order inside A1's band. The same inputs give the same transitions.
4. **Legacy that reads.** Each transition applies its legacy deltas once and starts a timed legacy marker (scorch, bare roots, dry riverbed). The marker drives the visuals, history and D4.
5. **Visual state.** Normal / Stressed / Transitioning / Post-transition per cell, as data for V-1 and the S-3 overlays.
6. **Save/load.** A3's per-cell state round-trips through E1.
7. **Observable.** A debug domain with overlays, an inspector and `cow.succession.*` commands. S-3 Ecology hosts `SuccessionDebugDomain` alongside A1's (amended 2026-10-19, product review).

## Non-Goals

- **Stress tracking and counter evaluation.** These belong to A1, which evaluates every rule.
- **Dragon behaviour.** The acceleration interface is declared (`SetAcceleration`). B1/B2 drive it from S-4, and the multiplier is 1.0 until then.
- **Post-v1 transitions:** Grassland→Forest, Scrub→Forest, and the restoration of a dried Wetland.
- **Water simulation** (amended 2026-11-17, A5 Hydrology)**.** A5 owns floods, lake levels, river flow and glaciers. A3 only reads them through A1.
- **Event display.** That belongs to D4. A3 publishes the events.
- **Ecology-state collapse in near-floor biomes.** That's A1's ecology state.
- **A stage.** S-3 Ecology hosts A3.

## Architecture

### Placement

**CoW-Shared** (AD-002). A3 is consumed by A1, A4, V-1, D4, C3 and S-3 Ecology. Pure logic lives in `Cluiche/CoW/Shared/Succession/`. `SuccessionModule` and the save participant live in `Cluiche/CoW/Modules/`. `SuccessionDebugDomain` lives in `Cluiche/CoW/Modules/` and is hosted by S-3 Ecology (amended 2026-10-19, product review). Test helpers ship in `Cluiche/CoW/Shared/Succession/Testing/`.

```
SimPU
  ├─ EcologyModule (A1)
  │    StepBands(band) ── step 10 ──► ISuccessionResolver::Resolve(triggers, sim)      ┐ same band,
  │                    ── Year ─────► ISuccessionResolver::OnYear(sim)                  │ cell-index
  │                    ── Season ───► ISuccessionResolver::OnSeason()                   ┘ order
  │    publishes CellEcologyChanged, BiomeSuccessionTriggered (observable),
  │             BiomeTransitionComplete, BiomeRecoveryProgressed  ← from the resolver's output
  └─ SuccessionModule (A3)
       owns SuccessionSimulation and its save participant
       registers the resolver with EcologyModule via ModuleRef
       SuccessionDebugDomain hosted by S-3 Ecology (amended 2026-10-19, product review)
```

The resolver is optional. If A3 is absent, A1 publishes its triggers and nothing changes (A1 soft dependency). A4 already listens for `BiomeTransitionComplete`, and applies the new biome the next week.

### Why synchronous

A1 publishes at the end of a band. A bus subscriber would get the trigger in the Reaction pass, which can't post `BiomeTransitionComplete`, so it would apply the transition a frame later. That leaves the transition between A1 bands, at a point that depends on frame timing.

Resolving inside A1's step 10 instead:
- applies each transition before steps 11 and 12 run, and before the next band, deterministically
- lets the module publish every event once, at the end of the band

`BiomeSuccessionTriggered` is still published, so D4 and the debug tools can see rejected and accepted triggers.

### Per-cell state

`SuccessionFields`: one `DynamicArrayC` per field, indexed by cell (SoA, same as A1 and A4).

| Field | Type | Notes |
|-------|------|-------|
| `visual_state` | enum | `Normal` / `Stressed` / `Transitioning` / `PostTransition` |
| `transition_seasons_remaining` | uint8 | Cosmetic. 0 when not transitioning |
| `legacy_type` | enum | `None` / `Scorch` / `BareRoots` / `DryRiverbed` |
| `legacy_years_remaining` | uint16 | Marker countdown |
| `recovery_stage` | enum | `None` / `Seeding` / `Establishing` / `Complete` |
| `acceleration` | float | Recovery multiplier from dragons, 1.0 until S-4 |
| `last_transition` | packed | from, to, year. For the inspector and history |

There's no recovery counter here: it is a rule counter in A1's `succession_pressure`.

### Succession rule table (A3 authors, A1 evaluates)

**Content ownership** (amended 2026-10-19, product review): A3 owns the contents of `succession_rules.json` at `Assets/CoW/Global/Succession/succession_rules.json` with envelope `"format": "cow.succession_rules", "version": 1` (A1 C7; amended 2026-10-19, F-3 World Assets). A3 has no F-3 participant and no packs in v1 (amended 2026-10-20, F-3 Feature 6): the rules file is a hand-authored defaults file, read by A1 as before. A1 ships an initial file translated from A3 TechnicalSpec §4 so A1 can be exercised before A3 exists. A1 evaluates every rule and provides the file schema; A3 authors the conditions and increments. The rule format, with one addition made here:

```json
{ "id": "grassland_drought_scrub", "from": "Grassland", "to": "Scrub", "source": "DROUGHT",
  "direction": "forward", "band": "Year",
  "conditions": [ {"field": "drought_years", "op": ">", "value": 8},
                  {"field": "producer.grass", "op": "<", "value": 0.2},
                  {"field": "seed_bank", "op": "<", "value": 0.3} ],
  "increment": 0.08, "decay": 0.04 }
```

- **`direction: forward`:** degradation and fire. The counter decays when the conditions fail.
- **`direction: recovery`:** Scrub→Grassland, BurnedAshland→Grassland, DesertifiedPlain→Scrub. The rule uses `decay: 0`, so stalled progress is kept (A3 §6, EdgeCase 2.1). A1 multiplies its increment by A3's `acceleration` for the cell.
- **The shipped rules** are one per staging-table transition. Each one is translated from the condition columns in A3 TechnicalSpec §4 and §6.1, with the default values from A3 and A1 TuningParameters:

| Rule | From → To | Source | Conditions (defaults) |
|------|-----------|--------|-----------------------|
| grassland_drought | Grassland → Scrub | DROUGHT | `drought_years > 8`, `grass < 0.2`, `seed_bank < 0.3` |
| grassland_fire | Grassland → BurnedAshland | FIRE | `fire_intensity > 0.6` (increment 1.0: immediate) |
| scrub_desert | Scrub → DesertifiedPlain | DROUGHT | `drought_years > 8`, `seed_bank < 0.1` |
| scrub_fire | Scrub → BurnedAshland | FIRE | `fire_intensity > 0.6` |
| forest_drought | TemperateForest → Scrub | DROUGHT | `tree < 0.3`, `drought_stress > 6` |
| forest_fire | TemperateForest → BurnedAshland | FIRE | `fire_intensity > 0.7` |
| wetland_drying | Wetland → Grassland | DRYING | `water < 0.3` (increment = 1 / `wetland_dry_years`, decay 1.0) |
| tundra_cold | Tundra → Scrub | COLD | `temperature < 0.15`, `years_low_herbivore_inflow > 5` |
| rainforest_canopy | Rainforest → Scrub | DISEASE | `tree < 0.4`, `disease_load > 0.6` |
| scrub_recovery | Scrub → Grassland | (recovery) | `water >= 0.4`, `seed_bank >= 0.2`; increment 0.04 |
| ash_recovery | BurnedAshland → Grassland | (recovery) | `water >= 0.35`, `years_since_fire >= 5`; increment 0.05 |
| desert_recovery | DesertifiedPlain → Scrub | (recovery) | `seed_bank >= 0.1`, `water >= 0.5`; increment 0.005 |
| lake_inundation | any land biome → Lake | INUNDATION | `inundated_weeks >= 104`, not protected. Clears `pre_glacial_biome` (Decade, hydrology; aligned to locked GDD 2026-11-17, CHANGE-048) |
| lakebed_to_wetland | Lake → Wetland | DRYING | `exposed_weeks >= 52` (Decade, hydrology) |
| dry_riverbed_marker | (no biome change, `to: none`) | DRYING | river cell, `low_flow_weeks >= 26`; sets the `DRY_RIVERBED` marker only (Decade, hydrology) |
| glaciation | land → AlpineMountain or TundraBoreal | COLD | `glacier_weeks >= 260`, not Ocean or Lake, `pre_glacial_biome` unset (Decade, hydrology) |
| glacial_retreat | glaciated cell → `pre_glacial_biome` | COLD | `glacier_free_weeks >= 260`, `pre_glacial_biome` set, not Lake (Decade, hydrology) |

The table is 17 rules: 12 above plus the five hydrology rules (rule ids for the last five are Cluiche's).

These numbers are starting values. They are tuned live in S-3's Controlled mode, and saved through A1's tuning write-back.

**Tundra condition** (amended 2026-10-19, product review): the GDD's "migration routes severed" isn't measurable. It becomes `years_low_herbivore_inflow`, a new A1 history field: the number of consecutive years in which herbivore migration into the cell was below `low_inflow_threshold`. This is a saved state field tracked per cell by A1, not read from the ledger.

### Water and ice rules from A5 (amended 2026-11-17, A5 Hydrology)

A5 exposes four signals through `HydrologySnapshot`. A1 reads them and presents them to the rule table as cell fields (see A1, Hydrology input), so A3 authors them as ordinary rules and A1 evaluates them. A5 never writes biome.

| Signal (from A5) | Rule field | Rule (first-run default) | Notes |
|---|---|---|---|
| `inundatedWeeks` | `inundated_weeks` | Any land biome → Lake at `inundated_weeks >= 104` (2 years). Protected cells (corridor, crossing, Hearth, Nexus, world-throne) are skipped. **Clears `pre_glacial_biome`** | Source `INUNDATION` (new) |
| `exposedWeeks` | `exposed_weeks` | Lake → Wetland at `exposed_weeks >= 52` (1 year) | A5 counts only on cells A3 has as Lake. The existing wetland-drying rule can then take the cell to Grassland. Source `DRYING` |
| sustained low `riverFlow` | `low_flow_weeks` | River cell with `low_flow_weeks >= 26` sets the `DryRiverbed` legacy marker | **Marker-only rule: `to: none`.** A3 sets the marker and does not call `ApplyBiomeTransition`, so A1's other counters are not reset. No biome change; `river_overlay` is unchanged (DECISION-064/065). Source `DRYING`. **Frozen weeks neither count nor reset** `low_flow_weeks` (a frozen river is not a dry one) |
| `glacierWeeks` (glacier mass) | `glacier_weeks` | Glaciation at `glacier_weeks >= 260` (5 years), biome not Ocean or Lake, and not already glaciated (`pre_glacial_biome` unset, so it fires **once per ice age**): → AlpineMountain if `elevation >= ALPINE_ELEVATION` (0.7), else TundraBoreal. **No new biome.** A3 saves `pre_glacial_biome` per cell. **A cell already at its target** (an Alpine cell at elevation >= 0.7, or a Tundra cell below it) only records `pre_glacial_biome` and has no biome change | Source `COLD` |
| `glacierWeeks` (retreat) | `glacier_weeks` | Glaciated cell (`pre_glacial_biome` set), biome not Lake, with `glacier_free_weeks >= 260` (5 years) → the saved `pre_glacial_biome`, with a `BARE_ROOTS` legacy marker. Retreat bypasses staging and clears `pre_glacial_biome`. A5 counter: `glacierFreeWeeks` (`glacier_free_weeks`), which counts while glacier mass is below `GLACIER_BIOME_MIN` | Source `COLD`. Ocean and Lake never glaciate, so they never get `pre_glacial_biome` |

- **Thresholds (aligned to locked GDD 2026-11-17, CHANGE-048).** The numbers above are GDD A5's first-run defaults (A5 TechnicalSpec 14.4 and A5 TuningParameters: `lake_inundation_weeks` 104, `lakebed_exposed_weeks` 52, `glaciation_weeks` / `glacial_retreat_weeks` 260 / 260 (retreat compares with `glacier_free_weeks`), `ALPINE_ELEVATION` 0.7; the 26-week dry riverbed is from 14.4). They go in the shipped rule file and `succession_parameters`. The glacier-mass floor that starts `glacier_weeks` is A5's (`GLACIER_BIOME_MIN`), not A3's. A5's counter names in Cluiche are `inundatedWeeks`, `exposedWeeks`, `lowFlowWeeks`, `glacierWeeks`.
- **`pre_glacial_biome`** is a new saved A3 field per cell (written at glaciation, read at retreat). It rides in `SuccessionSaveParticipant`'s arrays.
- **Bands.** All five rules are checked at the **Decade band** (GDD CH-A5-07: a cell can sit under water for up to about 10 years before it officially becomes Lake; V-1 shows inundation live, so only the biome lags). They read A5's weekly counters at the band.
- **New rule source:** `INUNDATION` joins `DROUGHT`, `FIRE`, `OVERGRAZE`, `DISEASE`, `DRYING` and `COLD` (aligned to locked GDD 2026-11-17, CHANGE-048; the earlier `FLOOD` and `THAW` are dropped). Exposed lakebed and dry riverbed use `DRYING`; glaciation and retreat use `COLD`. The T-1 `source` enum grows by `INUNDATION` only.
- **Guards (GDD A3 TechnicalSpec 10b, EdgeCaseMatrix):** inundation → Lake clears `pre_glacial_biome`, so a retreat can never turn a lake back into land. Any other transition keeps the record. Glaciation fires once per ice age because it requires `pre_glacial_biome` unset. A cell already at its glaciation target only records its old biome.
- **Open item (known GDD item):** the `DRY_RIVERBED` marker duration reuses `legacy_dry_riverbed_years` (20-100 years), which is likely too long for a river that dries for half a year. The marker is refreshed at each Decade check where low flow still holds. Revisit the duration in S-2.6 Rivers & Floods.
- **Staging table changes** are listed under the table below.
- **Without A5** (S-3 Controlled mode, tests) these fields read 0, so none of these rules fire. Existing rules are unchanged.

### Resolution (A1 step 10)

`Resolve(triggers, sim)` handles the band's triggers in cell-index order:

1. **Duplicates:** if the cell already transitioned this band, or is `Transitioning`, the trigger is discarded (EdgeCase 1.2).
2. **Staging check** against the table below. A rejected trigger is logged and published as rejected. Its counter is **not** reset, so pressure stays (EdgeCase 1.1). Near-floor biomes always reject (EdgeCase 4.1, 4.2).
3. **Apply:** `sim.ApplyBiomeTransition(cell, to, legacy.deltas)`. A1 sets the biome, applies and clamps the deltas, and resets **all** of that cell's counters, forward and recovery (EdgeCase 3.1, 3.2).
4. **A3 state:**
   - `visual_state = Transitioning`, with `transition_seasons_remaining = transition_animation_seasons` (2)
   - the legacy marker and its years are set
   - `recovery_stage = Complete` for a recovery transition, `None` for a forward one
   - `acceleration = 1.0`
5. **Output:** a `BiomeTransitionComplete{cell, from, to, nexus_node, hearth_site}` record. The module publishes it.

Recovery transitions are resolved the same way. They arrive as triggers from recovery rules.

**Staging table** (A3 TechnicalSpec §4.1; v1). Anything not listed is rejected.

| From | Valid targets |
|------|---------------|
| Grassland | Scrub, BurnedAshland |
| Scrub | DesertifiedPlain, BurnedAshland, Grassland (recovery) |
| TemperateForest | Scrub, BurnedAshland |
| Wetland | Grassland |
| Tundra | Scrub |
| Rainforest | Scrub |
| BurnedAshland | Grassland (recovery) |
| DesertifiedPlain | Scrub (recovery) |
| Lake (amended 2026-11-17, A5 Hydrology) | Wetland (exposed lakebed); the existing wetland-drying rule can then take it to Grassland |
| Land biomes that can flood (amended 2026-11-17, A5 Hydrology) | Lake (sustained inundation). Every land biome qualifies except protected cells (GDD A5 14.4) |
| Tundra, Alpine (amended 2026-11-17, A5 Hydrology) | glaciated: AlpineMountain (elevation >= 0.7) or TundraBoreal for glaciated cells; retreat returns to `pre_glacial_biome` |
| Desert, Volcanic, Ocean | none (near-floor or water) |

Alpine was in the "none" row. Glaciation and retreat now make it a possible target and source; Alpine otherwise stays near-floor. The per-biome staging rows are still to be written; the GDD A3 documents do not carry them yet.

### Legacy: one-shot deltas plus a timed marker

At transition, A3 hands A1 the pair's deltas, which **apply once**. A1's own processes then recover them: ash decays, soil depth rebuilds slowly, and seed drift refills the seed bank.

The legacy **marker** (type and years) is presentation and history only. It drives the visuals, D4 and the inspector, and expiring it undoes nothing. A1's `RemoveLegacy` is dropped (amendment below).

| From → To | Marker | Years (range) | Deltas |
|-----------|--------|---------------|--------|
| Grassland → Scrub | BareRoots | 5–10 | `soil_depth -0.10`, `seed_bank -0.10` |
| TemperateForest → Scrub | BareRoots | 10–20 | `soil_depth -0.15`, `producer.tree = 0` |
| TemperateForest → BurnedAshland | Scorch | 20–50 | `ash +0.6`, `nutrient.soil +0.2` |
| Grassland → BurnedAshland | Scorch | 5–15 | `ash +0.4`, `nutrient.soil +0.15` |
| Scrub → BurnedAshland | Scorch | 5–15 | `ash +0.4`, `nutrient.soil +0.15` |
| Scrub → DesertifiedPlain | DryRiverbed | 20–100 | `water_gradient -0.2`, `seed_bank = 0` |
| Wetland → Grassland | DryRiverbed | 10–30 | `producer.aquatic = 0` (`river_overlay` unchanged) |
| Lake → Wetland (amended 2026-11-17, A5 Hydrology) | none | — | TBD (aquatic species cleared, soil values from the new biome's start values) |
| Land → Lake (amended 2026-11-17, A5 Hydrology) | none | — | TBD (land producers cleared) |
| BurnedAshland → Grassland | Scorch (fading) | 5–15 | none (ash and fertility already decaying in A1) |
| Scrub → Grassland, DesertifiedPlain → Scrub | none | — | none |

The Scrub → BurnedAshland row was missing from the GDD table, so A3 uses the Grassland → BurnedAshland values for it.

**Marker years** are picked inside each range by a hash of `(world seed, cell, transition year)`. This is deterministic and uses no RNG stream: A3 has none (F-2's eight streams don't include it), and it doesn't need one.

### Year and Season ticks

- **`OnYear(sim)`:**
  - Visual state: `Stressed` if `GetSuccessionProgress(cell)` (highest forward counter) ≥ `stressed_visual_threshold` (0.5); otherwise `Normal`, unless the cell is transitioning or has a marker.
  - Recovery stage: when a recovery counter first crosses `recovery_stage_1_threshold` (0.3), the stage becomes `Seeding`. At `recovery_stage_2_threshold` (0.7) it becomes `Establishing`. Each change emits a `BiomeRecoveryProgressed{cell, stage}` record.
  - Legacy markers count down. At 0 the marker clears and the visual state returns to `Normal`.
  - Acceleration is cleared where no qualifying dragon refreshed it this year (EdgeCase 2.2). This is a no-op until S-4.
- **`OnSeason()`:** `transition_seasons_remaining` counts down. At 0 the visual state becomes `PostTransition`, or `Normal` if there is no marker.

### Dragon acceleration (interface only)

`SetAcceleration(cell, archetype)` is called by B1/B2 from S-4.

- **Bonuses:** Verdant ×1.8 on Scrub, Verdant ×1.5 on BurnedAshland, Farmer ×1.6 on Scrub.
- **Condition:** a bonus only applies while the recovery rule's conditions hold (A3 §6.3: "nothing to accelerate").
- **Expiry:** it lapses at the next Year tick unless refreshed.

Until S-4, `acceleration` is 1.0. `cow.succession.accelerate` sets it by hand for testing.

### Natural fire (A1 amendment, resolved here)

Without dragons, nothing in A1 starts a fire, so Burned/Ashland could never emerge in S-3. **A1 gains seeded natural ignition** in its Season band:

- **Chance per cell:** `p = base_ignition × drought_stress_fit × fuel × temperature_fit`, where fuel is grass + shrub + tree biomass.
- **RNG:** draws use the A1 stream in cell-index order.
- **After ignition:** the fire spreads with the existing `fire_spread_rate`, burns biomass into ash, and sets `fire_intensity`, which is what A3's fire rules read.
- **Debug:** `cow.ecology.ignite` starts a fire by hand.

This is recorded as A1 amendment 3 below, and is new GDD content for the fold-back.

### Save/load (E1)

`SuccessionSaveParticipant` saves the `SuccessionFields` arrays as JSON arrays. The recovery counters live in A1's `succession_pressure`, so A1's save covers them. Rules and tuning reload from data. A save, continue, load, continue sequence must be bit-identical.

### Debug and tools

`SuccessionDebugDomain` (SimPU, hosted by S-3 Ecology; amended 2026-10-19, product review; amended 2026-10-19, DiaCellLayers/DiaCellHistory) follows the A1 pattern: commands first, and the panel calls them.

- **Overlays (amended 2026-10-19, DiaCellLayers/DiaCellHistory):** transitions and recovery stage changes are emitted as DiaCellHistory events. The visual-state overlay becomes a registered DiaCellLayers layer. Other layers: visual state, recovery stage, legacy marker, highest rule progress (forward or recovery), and the cells rejected by staging this band.
- **Inspector:** the cell's biome history (`last_transition`), every rule's counter and which conditions pass, the marker and years left, the recovery stage, and acceleration.
- **Commands:** `cow.succession.inspect`, `cow.succession.transition` (forced, still checked against staging), `cow.succession.accelerate`, `cow.succession.rules.reload`, `cow.succession.rejections`.
- **Instrumentation:** `DIA_PROFILE_SCOPE` for Resolve, OnYear and OnSeason. Metrics: transitions per band by pair, rejections, cells per visual state. Health is Degraded when the rule table references an unknown biome or field.
- **Logging** (added 2026-10-19, product review): on the `ECO` channel (shares A1's channel): rejected transitions (staging violation, cell, from, to, source) at Info; accepted transitions at Info; recovery stage changes at Info; rule-table validation errors at Error.

**Layer ids** (amended 2026-10-19, consistency pass): A3 registers `succession.visual_state` (Category: Normal / Stressed / Transitioning / Post-transition), `succession.recovery_stage` (Category: none / Seeding / Establishing / Complete) and `succession.legacy_marker` (Float: marker years left). Succession progress is A1's `ecology.succession_progress`. Event types: `cow.succession.transition`, `cow.succession.recovery_stage`.

## Public Interfaces

```cpp
namespace CoW::Succession {

    // Pure logic. Unit-tested directly.
    class SuccessionSimulation final : public CoW::Ecology::ISuccessionResolver {
    public:
        bool Initialise(uint32_t cellCount, const SuccessionData& data, uint64_t worldSeed);
        void Resolve(const CoW::Ecology::TriggerList& triggers, CoW::Ecology::EcologySimulation& sim,
                     SuccessionOutput& out) override;                      // A1 step 10
        void OnYear(const CoW::Ecology::EcologyView& view, SuccessionOutput& out) override;
        void OnSeason() override;
        float GetRecoveryMultiplier(uint32_t cell) const override;           // A1 multiplies recovery increments
        void SetAcceleration(uint32_t cell, Dia::Core::StringCRC archetype, bool active);   // B1/B2, S-4
        bool IsValidTransition(Dia::Core::StringCRC from, Dia::Core::StringCRC to) const;
        SuccessionView GetView() const;
        void RestoreState(const SuccessionFields& fields);
    };

    struct SuccessionData { /* staging table, legacy table, visual/recovery params, acceleration bonuses */ };
    bool LoadSuccessionData(const Json::Value& params, SuccessionData& out);

    class SuccessionModule : public Dia::ApplicationFlow::SimModule {
    public:
        const SuccessionSimulation& GetSimulation() const;
    };
}
```

`ISuccessionResolver` is declared in A1 (`Cluiche/CoW/Shared/Ecology/`), so A1 has no dependency on A3. `SuccessionOutput` holds the transition, rejection and recovery-progress records, and `EcologyModule` publishes them at the end of the band. The messages `BiomeTransitionComplete` and `BiomeRecoveryProgressed` are hand-written structs in `Cluiche/CoW/Modules/SuccessionMessages.h`. A4 already subscribes to `BiomeTransitionComplete`.

## Amendments to A1 (approved with this spec, folded into the A1 spec 2026-10-18)

1. **The `ISuccessionResolver` seam.** A1's step 10 collects the band's triggers and calls `Resolve`. The Year and Season bands call `OnYear` and `OnSeason`. Without a resolver, triggers are only published.
2. **Recovery rules.** Rules gain `direction` (`forward` / `recovery`). Recovery increments are multiplied by `GetRecoveryMultiplier(cell)`. `ApplyBiomeTransition` resets every counter for the cell. **`RemoveLegacy` is removed**, because legacy deltas are one-shot.
3. **Natural ignition**, at the Season band (see Natural fire), plus the `cow.ecology.ignite` command. New tuning constants: `base_ignition`, `ignition_drought_fit`, `ignition_temperature_fit`.
4. **The `years_low_herbivore_inflow` history field**, with a `low_inflow_threshold` constant. It's used by the tundra rule.

## Features

| # | Feature | Description | Spec | Status |
|---|---------|-------------|------|--------|
| 1 | Succession Data & Rules | `SuccessionData` (staging, legacy, visual/recovery params, bonuses) load and validate; the shipped `succession_rules.json` (17 rules: the 12 stress and recovery rules plus the five hydrology rules `lake_inundation`, `lakebed_to_wetland`, `dry_riverbed_marker`, `glaciation`, `glacial_retreat`; ids are Cluiche's); rule-table validation against A1's biome ids and fields. Pure logic, TDD | TBD | Draft |
| 2 | Resolver | `SuccessionSimulation::Resolve`: duplicates, staging, `ApplyBiomeTransition`, legacy deltas, marker years by hash, output records; EdgeCase 1.1–4.2 as tests. Needs the A1 seam (amendment 1). Pure logic, TDD | TBD | Draft |
| 3 | Year/Season Ticks & Recovery | `OnYear` (visual state, recovery stages, marker countdown, acceleration lapse), `OnSeason`, `GetRecoveryMultiplier`, `SetAcceleration`; asymmetry test (degrade about 35 years vs recover 200–400 years at default tuning, A3 §2.3). Pure logic, TDD | TBD | Draft |
| 4 | SuccessionModule & Save | SimModule wiring, resolver registration via `ModuleRef`, message structs, save participant and round-trip, `cow_main.diaapp` entry | TBD | Draft |
| 5 | Succession Debug | `SuccessionDebugDomain`: commands, overlays, inspector, metrics, health. Included in the A1/S-3 HTML mockup | TBD | Draft |

**Build order:** 1 → 2 → 3 → 4 → 5. Features 2 and 3 need A1 Features 1–5 plus amendments 1 and 2. A1 amendment 3 (ignition) belongs with A1 Feature 4. The A5 rules (amended 2026-11-17, aligned to locked GDD 2026-11-17, CHANGE-048) need A5's `HydrologySnapshot` through A1's hydrology input, so they land after A1 Feature 7 and A5 Feature 3; the existing 12 rules do not wait for A5. Amendment 4 (inflow history) belongs with A1 Feature 4.

## Binding Decisions

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-001 | StringCRC for ids | Biome, rule, archetype and legacy ids |
| PD-002 | ProcessingUnit/Phase/Module architecture | `SuccessionModule` is a SimModule on SimPU. The resolver is registered through `ModuleRef`, with no singletons |
| PD-004 | No STL containers in public APIs | `SuccessionFields` and outputs use `DynamicArrayC` |
| AD-002 | Four-tier dependency model | CoW-Shared |
| AD-003 | GDD owns design intent | Deviations are listed below for the GDD session |
| AD-005 | One stage per testbed | A3 owns no stage. S-3 Ecology hosts it |

## Deviations from GDD A3

For the GDD session to fold back, on top of CHANGE-036:

1. **A3 resolves synchronously inside A1's step 10**, through `ISuccessionResolver`. It doesn't wait for the event on the bus. `BiomeSuccessionTriggered` is still published, as an observable event.
2. **Recovery counters are recovery rules in `succession_rules.json`** (`direction: recovery`, decay 0). A1 evaluates them and A3 supplies a multiplier. The `recovery_counter` field leaves `SuccessionState`.
3. **Legacy deltas apply once.** The legacy duration is a marker for visuals, history and D4, and removing it undoes nothing. This replaces "Persists for duration, then removed" (TechnicalSpec §5).
4. **Marker years are picked from a deterministic hash** within each table range.
5. **Added legacy row:** Scrub → BurnedAshland (Scorch, 5–15 years), which the table had left out.
6. **Tundra's "migration routes severed"** becomes `years_low_herbivore_inflow > 5`.
7. **Natural ignition is added to A1.** Fire can now emerge without dragons.
8. **Ocean and Lake have no transitions.** They are added to the near-floor row. **Amended 2026-11-17 (A5 Hydrology): Ocean keeps no transitions. Lake now transitions both ways with land** (land → Lake on sustained inundation, Lake → Wetland on sustained exposure), because A5 lets lakes grow and shrink. Matches GDD A5 14.4 (CHANGE-048: "This adds Lake's first transitions"); the GDD A3 documents still say none.
10. **A5 water and ice rules** (amended 2026-11-17, A5 Hydrology): inundation, exposed lakebed, dry riverbed (sustained low river flow) and glacier rules, new source `INUNDATION`. First-run thresholds are fixed (104 / 52 / 26 / 260 weeks, all at the Decade band; aligned to locked GDD 2026-11-17, CHANGE-048). Glaciated cells use existing biomes and `pre_glacial_biome` is saved. River courses stay static (DECISION-064/065).
9. **GeographyAndRegions Part 6 lists Grassland → Forest Edge and Forest → Grassland.** A3 v1 excludes both: Forest Edge isn't a biome id, and Forest → Grassland isn't in the staging table. GeographyAndRegions should note them as post-v1.

## Dependencies

**Requires:**
- **A1 Ecology**, Features 1–5, plus amendments 1–4.
- **A5 Hydrology** (amended 2026-11-17; GDD locked 2026-11-17, CHANGE-048; Cluiche spec Draft, unbuilt), soft: `inundatedWeeks`, `exposedWeeks`, `lowFlowWeeks` and `glacierWeeks`, read through A1. Without it the water and ice rules never fire.
- GameSpeedController (bands, via A1), E1, DiaMessageBus, `VisualDebuggerModule`, DiaAPI `CommandRegistry`, Dia CellField (dia-needs #14) for overlays. DiaCellLayers, DiaCellHistory and DiaValueSchema (layers, events, tooltips), all Approved, not built (amended 2026-10-19, consistency pass).

**Required by:** A1 (resolver, optional), A4 (`BiomeTransitionComplete`), S-3 Ecology (Living mode transitions, the A3 scenario, the succession long-run check), and later V-1, D4, C3, B1 and B2.

## Test Strategy

**Unit (Features 1–3, hand-built grids, A1 with `StubWeatherInput`):**
- Every staging pair, valid and invalid.
- Every legacy row applies its exact deltas, clamped.
- Marker years are deterministic and inside their range.
- Duplicate triggers in one band are discarded. Rejected triggers keep their counter.
- A transition resets every counter for the cell, and a pending recovery is cancelled by a forward transition (EdgeCase 2.1, 3.1, 3.2).
- Stressed at 50% progress.
- (amended 2026-11-17, A5 Hydrology) With a stub hydrology input: sustained `inundated_weeks` turns land into Lake; sustained `exposed_weeks` turns Lake into Wetland; low river flow sets the `DryRiverbed` marker without moving `river_overlay`; glaciated cells go to AlpineMountain or TundraBoreal and return to `pre_glacial_biome` on retreat; frozen river weeks neither count nor reset `low_flow_weeks`; Ocean and Lake never glaciate; with no A5 none of these fire.
- Seeding at 0.3, Establishing at 0.7, transition at 1.0.
- Stalled recovery keeps its progress.
- The acceleration multiplier applies only while the conditions hold, and lapses after a year without refresh.
- **Asymmetry:** at default tuning, Grassland → DesertifiedPlain takes about 35 years under held drought. DesertifiedPlain → Grassland takes 200–400 years under held recovery conditions with seed drift (A3 §2.3, PrototypeTestbed Test 2). Reported, with a ±25% band.

**Integration (Feature 4):**
- A trigger at step 10 changes the biome before step 12 in the same band.
- `BiomeTransitionComplete` is published once per transition, and A4 picks it up the next week.
- Save at year 50, continue to 100, load year 50, continue to 100: identical.

**Performance:** Resolve plus OnYear at Large (6,656 cells) under 1 ms. This replaces the GDD's 10,000- and 100,000-cell targets, which assumed 200×200.

**Proof:** S-3 Ecology's A3 scenario (Living mode) and its succession long-run check (≥ 10% of cells transitioned).

## Open Design Questions

1. **Will natural ignition give a believable fire regime at default tuning?** Neither "fire every few decades in dry grassland" nor "never" is guaranteed. S-3 Controlled mode tunes it first, and the GDD may want to set the target fire return intervals per biome.
2. **Is the tundra inflow proxy right?** `years_low_herbivore_inflow` stands in for "migration routes severed". If it fires too easily in naturally sparse tundra, it may need to compare against the cell's own historical inflow instead of a fixed threshold.

## Open question added 2026-11-17 (A5 Hydrology)

3. **Aquatic to land and back.** A1 runs an aquatic-only ecology on Lake cells and land ecology elsewhere. `ApplyBiomeTransition` for Lake ↔ land must reinitialise the cell's producers and species from the new biome's starting values, and A1 must accept a land cell that becomes aquatic mid-run. That is an A1 change (Feature 5) to settle before the Lake rules are built. **Partly answered by the locked GDD (2026-11-17, CHANGE-048):** for land → Lake, land populations leave through the migration solver and any that can't move are lost (A5 14.3, using A1's existing rule that land species can't enter Lake). A5 EC-16 also covers a hand-edited Lake that A5 never inundated. Still open: the producer and soil re-initialisation values for Lake → Wetland and for land → Lake, and A1 accepting a mid-run aquatic switch. The question stays open.

## T-1 consumer amendment (2026-11-16, approved with T-1 Feature 12)

Requested by @docs/specs/applications/cow/systems/t1-worldsiminspector/cross-system-causes.md, so a succession transition can be linked to the pressure that caused it.

- The `cow.succession.transition` DiaCellHistory event payload is `{ cell, from, to, rule, source }`: `rule` is the `succession_rules.json` id, and `source` is that rule's `source` (for example `DROUGHT`).
- Its `kind: "event"` DiaValueSchema entry lists these five fields, with `source` as an enum of the rule sources in use.
- The `BiomeTransitionComplete` message is unchanged (A4 and others still read `{cell, from, to, nexus_node, hearth_site}`).
- Forced transitions (`cow.succession.transition`, the debug command) emit `rule: "forced"` and `source: "FORCED"`.

**Added criterion:** every transition event carries a `rule` that exists in the loaded rules file (or `forced`) and the matching `source`; the schema entry validates.

## Status

**Status:** `Approved` (2026-10-18; T-1 consumer amendment added 2026-11-16). A1 amendments folded into the A1 spec. No plan yet; create `a3-biomesuccession.plan.md` when implementation starts. Interview done the same day: synchronous resolver, recovery as rules, one-shot legacy, natural ignition in A1. **Amended 2026-11-17 (A5 Hydrology; aligned to locked GDD 2026-11-17, CHANGE-048):** new water and ice rules, Lake ↔ land transitions, new source `INUNDATION`, marker-only `to: none` rules, `pre_glacial_biome`, 17 rules; status stays `Approved`; thresholds now set (104 / 52 / 26 / 260 weeks, Decade band). Smaller calls made in drafting: tundra proxy, Forest Edge excluded, hashed marker years, A3's own debug domain, Ocean/Lake no transitions.
