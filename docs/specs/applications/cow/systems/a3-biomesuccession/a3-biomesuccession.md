# System Spec: A3 Biome Succession

**Parent:** @docs/specs/applications/cow/cow.md  
**Design source:** `Output Docs/GDD/08_SystemsDeepDive/A3_BiomeSuccession/` (DesignDocs repo; `A3_BiomeSuccession.md`, `A3_TechnicalSpec.md`, `A3_TuningParameters.md`, `A3_EdgeCaseMatrix.md`, `A3_PrototypeTestbed.md`, `A3_ProductionBacklog.md`, Locked; amended by CHANGE-036, pending fold-back by the GDD session)  
**Works with:** @docs/specs/applications/cow/systems/a1-ecology/a1-ecology.md (A1 evaluates the counters; A3 decides the transitions)

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

**Content ownership** (amended 2026-10-19, product review): A3 owns the contents of `succession_rules.json` (A1 C7). A1 ships an initial file translated from A3 TechnicalSpec §4 so A1 can be exercised before A3 exists. A1 evaluates every rule and provides the file schema; A3 authors the conditions and increments. The rule format, with one addition made here:

```json
{ "id": "grassland_drought_scrub", "from": "Grassland", "to": "Scrub", "source": "DROUGHT",
  "direction": "forward", "band": "Year",
  "conditions": [ {"field": "drought_years", "op": ">", "value": 8},
                  {"field": "grass", "op": "<", "value": 0.2},
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

These numbers are starting values. They are tuned live in S-3's Controlled mode, and saved through A1's tuning write-back.

**Tundra condition** (amended 2026-10-19, product review): the GDD's "migration routes severed" isn't measurable. It becomes `years_low_herbivore_inflow`, a new A1 history field: the number of consecutive years in which herbivore migration into the cell was below `low_inflow_threshold`. This is a saved state field tracked per cell by A1, not read from the ledger.

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
| Desert, Alpine, Volcanic, Ocean, Lake | none (near-floor or water) |

### Legacy: one-shot deltas plus a timed marker

At transition, A3 hands A1 the pair's deltas, which **apply once**. A1's own processes then recover them: ash decays, soil depth rebuilds slowly, and seed drift refills the seed bank.

The legacy **marker** (type and years) is presentation and history only. It drives the visuals, D4 and the inspector, and expiring it undoes nothing. A1's `RemoveLegacy` is dropped (amendment below).

| From → To | Marker | Years (range) | Deltas |
|-----------|--------|---------------|--------|
| Grassland → Scrub | BareRoots | 5–10 | `soil_depth -0.10`, `seed_bank -0.10` |
| TemperateForest → Scrub | BareRoots | 10–20 | `soil_depth -0.15`, `tree = 0` |
| TemperateForest → BurnedAshland | Scorch | 20–50 | `ash +0.6`, `soil_fertility +0.2` |
| Grassland → BurnedAshland | Scorch | 5–15 | `ash +0.4`, `soil_fertility +0.15` |
| Scrub → BurnedAshland | Scorch | 5–15 | `ash +0.4`, `soil_fertility +0.15` |
| Scrub → DesertifiedPlain | DryRiverbed | 20–100 | `water_gradient -0.2`, `seed_bank = 0` |
| Wetland → Grassland | DryRiverbed | 10–30 | `aquatic = 0` (`river_overlay` unchanged) |
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

`SuccessionDebugDomain` (SimPU, hosted by S-3 Ecology; amended 2026-10-19, product review) follows the A1 pattern: commands first, and the panel calls them.

- **Overlays (CellField):** visual state, recovery stage, legacy marker, highest rule progress (forward or recovery), and the cells rejected by staging this band.
- **Inspector:** the cell's biome history (`last_transition`), every rule's counter and which conditions pass, the marker and years left, the recovery stage, and acceleration.
- **Commands:** `cow.succession.inspect`, `cow.succession.transition` (forced, still checked against staging), `cow.succession.accelerate`, `cow.succession.rules.reload`, `cow.succession.rejections`.
- **Instrumentation:** `DIA_PROFILE_SCOPE` for Resolve, OnYear and OnSeason. Metrics: transitions per band by pair, rejections, cells per visual state. Health is Degraded when the rule table references an unknown biome or field.
- **Logging** (added 2026-10-19, product review): on the `ECO` channel (shares A1's channel): rejected transitions (staging violation, cell, from, to, source) at Info; accepted transitions at Info; recovery stage changes at Info; rule-table validation errors at Error.

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
| 1 | Succession Data & Rules | `SuccessionData` (staging, legacy, visual/recovery params, bonuses) load and validate; the shipped `succession_rules.json` (12 rules); rule-table validation against A1's biome ids and fields. Pure logic, TDD | TBD | Draft |
| 2 | Resolver | `SuccessionSimulation::Resolve`: duplicates, staging, `ApplyBiomeTransition`, legacy deltas, marker years by hash, output records; EdgeCase 1.1–4.2 as tests. Needs the A1 seam (amendment 1). Pure logic, TDD | TBD | Draft |
| 3 | Year/Season Ticks & Recovery | `OnYear` (visual state, recovery stages, marker countdown, acceleration lapse), `OnSeason`, `GetRecoveryMultiplier`, `SetAcceleration`; asymmetry test (degrade about 35 years vs recover 200–400 years at default tuning, A3 §2.3). Pure logic, TDD | TBD | Draft |
| 4 | SuccessionModule & Save | SimModule wiring, resolver registration via `ModuleRef`, message structs, save participant and round-trip, `cow_main.diaapp` entry | TBD | Draft |
| 5 | Succession Debug | `SuccessionDebugDomain`: commands, overlays, inspector, metrics, health. Included in the A1/S-3 HTML mockup | TBD | Draft |

**Build order:** 1 → 2 → 3 → 4 → 5. Features 2 and 3 need A1 Features 1–5 plus amendments 1 and 2. A1 amendment 3 (ignition) belongs with A1 Feature 4. Amendment 4 (inflow history) belongs with A1 Feature 4.

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
8. **Ocean and Lake have no transitions.** They are added to the near-floor row.
9. **GeographyAndRegions Part 6 lists Grassland → Forest Edge and Forest → Grassland.** A3 v1 excludes both: Forest Edge isn't a biome id, and Forest → Grassland isn't in the staging table. GeographyAndRegions should note them as post-v1.

## Dependencies

**Requires:**
- **A1 Ecology**, Features 1–5, plus amendments 1–4.
- GameSpeedController (bands, via A1), E1, DiaMessageBus, `VisualDebuggerModule`, DiaAPI `CommandRegistry`, Dia CellField (dia-needs #14) for overlays.

**Required by:** A1 (resolver, optional), A4 (`BiomeTransitionComplete`), S-3 Ecology (Living mode transitions, the A3 scenario, the succession long-run check), and later V-1, D4, C3, B1 and B2.

## Test Strategy

**Unit (Features 1–3, hand-built grids, A1 with `StubWeatherInput`):**
- Every staging pair, valid and invalid.
- Every legacy row applies its exact deltas, clamped.
- Marker years are deterministic and inside their range.
- Duplicate triggers in one band are discarded. Rejected triggers keep their counter.
- A transition resets every counter for the cell, and a pending recovery is cancelled by a forward transition (EdgeCase 2.1, 3.1, 3.2).
- Stressed at 50% progress.
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

## Status

**Status:** `Approved` (2026-10-18). A1 amendments folded into the A1 spec. No plan yet; create `a3-biomesuccession.plan.md` when implementation starts. Interview done the same day: synchronous resolver, recovery as rules, one-shot legacy, natural ignition in A1. Smaller calls made in drafting: tundra proxy, Forest Edge excluded, hashed marker years, A3's own debug domain, Ocean/Lake no transitions.
