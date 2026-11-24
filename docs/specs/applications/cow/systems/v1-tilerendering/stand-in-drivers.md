# Feature Spec: V-1 Stand-in Drivers

## Parent System
@docs/specs/applications/cow/systems/v1-tilerendering/v1-tilerendering.md (Feature 2)

**Gameplay Domains:** rendering, simulation
**Depends on:** Strategic Map Feature 2 (`IMapChannelSource`, publisher, `YearClock`); V-1 Stage and World
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/weather.js` (`WeatherSim`), `js/alive.js` (`FireSim`, `Fauna`, `YearClock.drivers`), `js/app.js` (`healthOf`, time-lapse drift)

## Status
`Draft` (2026-11-20). Drafted from the approved prototype; awaiting user review.

## Summary

Until A1, A3, A4 and A5 are built (and to test the map under conditions no real run produces), V-1 owns **stand-in drivers**: small CoW testbed modules that implement `IMapChannelSource` and fill the same per-cell channels the real systems will. Each has a **Live switch**: when the real system's source exists, the driver steps aside and the real one takes over, with no renderer change.

| Driver | Channel | What it does |
|---|---|---|
| Ecology | Condition | Health 0..1 with regional variation; time-lapse drift (fake history) |
| Weather | Weather | Moving storm fronts, per-cell cloud, precipitation, snow fraction, gust |
| Water | WaterState | Living-year lake level, river flow state, flood, ice |
| Fire | Events | Wildfire CA: spread, douse, scar healing |
| Fauna | Fauna sprites | Herds that avoid water and fire; bird flocks |
| (Clock) | Season | Uses Strategic Map's `YearClock` (not a stand-in) |

The drivers are testbed code. They are **never part of the game build** (V1-002).

## Problem

- V-1 runs after A4/A1/A3 in the build order, but A5, A1 fire and herds will lag. The map's features (fire scars, storms, floods, herds) can't be tuned or checked against nothing.
- Real simulations produce slow, similar scenes. Look-dev needs on-demand extremes: a storm over a plateau, a wildfire, a collapse, a flood.
- When a real system arrives, replacing its stand-in must be a switch, not a rewrite.

## Goals

1. **Five drivers** as `IMapChannelSource`s with the behaviour below, deterministic from a seed.
2. **Live switch per driver**: `v1.driver.live {name, on}` swaps to the real source if one is registered, and the swap is logged; `Version()` bumps.
3. **Controls as commands** (`v1.driver.*`): ecology health and variation, storm count/intensity/wind, fire ignite and auto-ignite, fauna on/off, time-lapse on/off and speed.
4. **Fixed seeds and a step API** so captures and checks are reproducible (`v1.driver.step {n}`).
5. **Fidelity to the approved prototype**: the same algorithms and constants, in C++.

## Non-Goals

- Simulation accuracy. These are plausible, not correct. A4/A1/A5 are the truth.
- Feeding anything back into any real system.
- Saving driver state.
- Being reused by the game.

## Design

### Weather driver (A4 stand-in)

- `N` storms with `{x, y, radius 5..12, intensity 0.6..1.0, phase}` drifting with the wind (wrap-around map).
- Per cell: `g = Σ intensity × (0.85 + 0.15 sin(t·0.7 + phase)) × exp(−d²/r²)`; orographic term `elevation × 0.35 × g`.
- `cloud = min(1, background×0.45 + 1.05 g + orographic)`; `precip = clamp((g + orographic − 0.3)/0.6) × stormIntensity`.
- **Snow fraction** from each cell's own temperature plus a season offset: `clamp((0.45 − (T + seasonT)) / 0.12)`; seasonal offsets `+0.03 / +0.06 / +0.10 / −0.17` for Bloom, Hunt, Dry, Freeze (the sign convention follows the prototype; documented in the code).
- `gust = min(1, g)`.

### Water driver (A5 stand-in)

Uses `YearClock::Drivers`: snow pack, lake level (`0.5 + Bloom·0.25 − Dry·0.3 − Freeze·0.05`), river flow state (Frozen in Freeze, Flood in Bloom, Low in Dry, else Normal), flood and ice flags. Optional manual overrides.

### Ecology driver (A1 stand-in)

`health(c) = clamp(base + variation × (fbm(cellPos × 0.11 + seed) − 0.5) × 1.1)`; the time-lapse mode shifts the noise offset over time (fake decades of drift). Fire scars lower the *rendered* condition (`× (1 − 0.95·scar)`), done by the publisher's combine step, not by the driver itself.

### Fire driver (events stand-in)

Per-cell CA stepped at `1/0.12 s`: ignition (manual or auto in the Dry season with risk² × 0.05 per step on fuel cells with health > 0.4); flames build for 1.2 s then die down (`−(0.22 + 0.8 × rain)`); spread to the six neighbours with probability `0.16 × fuel × max(0, 1 + downwind·(0.5 + 1.5·wind)) × (1 − rain) × (0.6 + risk)`; fuel per biome tag (`fuel`, with values from a small table: grass 0.9, forest 1.0, rainforest 0.45, tundra 0.4, alpine 0.35, wetland 0.25, desert 0.15, volcanic 0.1); a burned cell becomes a scar that blocks re-ignition above 0.4 and heals over 3 game years. Output: `burn`, `scar` per cell.

### Fauna driver

Herds: positions on grass/tundra/wetland cells, random-walk heading from noise, turning away from water and from burning or scarred cells; 7 herds of 10–23 animals. Birds: 5 flocks of 7 on circular paths. Output is a sprite list for Strategic Map's dynamic sprite layer (a separate `Fauna` channel, see Channel Binding ODQ 3).

### Interface

```cpp
namespace CoW::V1 {
    class IStandIn : public CoW::Map::IMapChannelSource {
    public:
        virtual const char* Name() const = 0;
        virtual void Update(float dt, const DriverContext&) = 0;     // wind, season weights, world refs, seed
        virtual void Reset(uint32_t seed) = 0;
        virtual void SetLive(bool live) = 0;                         // when live, IsLive() reflects the real source
    };
    // DriverContext: world layers (read only), wind, season weights/day, fire/weather cross-reads (fire reads weather rain).
}
```

## Acceptance Criteria

1. **Determinism.** Each driver, given a seed and a fixed sequence of `Update(dt)` calls, produces identical channel data (hash) on repeat runs; a different seed changes it.
2. **Weather locality.** With one storm, cells farther than 3 radii have `cloud` equal to the background and `precip == 0`; the maximum precipitation is at the storm centre (±1 cell).
3. **Rain or snow by cell.** In one storm over a world with a cold plateau and a warm lowland, snow fraction is > 0.9 on plateau cells and < 0.1 on lowland cells; forced rain-only or snow-only overrides apply uniformly.
4. **Orography.** For equal storm distance, higher-elevation cells have at least as much precipitation as lower ones.
5. **Water driver.** Over a year, flow state follows Bloom → flood, Dry → low, Freeze → frozen; lake level is within [0.2, 0.75]; values are continuous across season edges (no jumps in lake level or snow between consecutive days).
6. **Ecology bands.** With base health 0.1 / 0.5 / 0.9 the mean condition lands in the Collapsed / Stressed-Stable / Thriving bands; variation 0 gives a uniform map.
7. **Fire spread.** On a uniform dry grassland grid, mean spread probability matches the formula (±15% over many runs); with wind, downwind spread rate exceeds upwind; rain above 0.8 extinguishes fires faster than none; water cells never burn.
8. **Fire scars.** A burned cell has scar 1 after burn-out, blocks re-ignition while scar > 0.4, and heals to 0 in about 3 game years at the current clock.
9. **Fauna.** No herd or bird sprite position lies on water for herds (checked over 10,000 steps); herds never enter a cell with burn > 0 or scar > 0.6; counts match the config.
10. **Live switch.** With a fake "real" source registered, `v1.driver.live {name, on:true}` makes the published channel equal the real source's data and bumps the version; switching back restores the stand-in; the stand-in stops updating while not live.
11. **Reset and step.** `v1.driver.step {n}` advances the drivers `n` fixed steps with no wall clock; two runs of the same `step` sequence from the same seed match.
12. **Isolation.** The driver code is not referenced by `cow_main.diaapp` or any game module (dependency check).
13. **Observability.** Log channel `V1`; per-driver info on reset and live switch; metrics `v1.driver_ms` (per driver) and `v1.fire_cells`, `v1.storm_cells`; health Degraded when a Live driver's real source is not live.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| V1-002 | Stand-ins live in V-1 behind `IMapChannelSource` with a Live switch; never in the game build | AC 10, 12 |
| SM-003 | Weather is per cell | AC 2, 3 |
| SM-004 | Fire and herds are rendered from A1/A3 state; the map does not simulate them | The CA lives here as a stand-in only |
| Feedback: PU boundaries are streams | No cross-PU shared memory | Drivers run on SimPU through the publisher |
| Feedback: no singletons | `ModuleRef` or streams | Drivers are module members of `V1StageModule` |

## Open Design Questions

1. **Fire stand-in vs A1's real fire.** A1 resolves natural fire inside one Season-band step (`ecology.transient.fire_intensity`, `years_since_fire`), so the real data has no continuous daily front. The CA here is therefore the *prototype of the map's cosmetic front playback* (Strategic Map Channel Binding ODQ 2), plus a way to test scars and smoke before A1 exists. When A1 is built, the fire driver switches Live and the playback is seeded from `fire_intensity`; the CA's spread model is then not used.
2. **Weather smoothing.** A4's real output is a blend of two weekly snapshots. The stand-in is continuous, so V-1 cannot test the snapshot blend path. A small "weekly steps" mode (quantised updates with the blend) would. Add when A4 Feature 9 is built.
3. **Season offset sign.** The prototype's seasonal temperature offsets are in "temperature" units where lower is colder; confirm against A4's temperature scale when wiring the real source.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `IStandIn`, driver context, registry in `V1StageModule` | AC 1, 11 | Not Started | sonnet | |
| 2 | Weather driver | AC 2, 3, 4 | Not Started | sonnet | |
| 3 | Water driver and year clock link | AC 5 | Not Started | sonnet | |
| 4 | Ecology driver and time-lapse drift | AC 6 | Not Started | sonnet | |
| 5 | Fire driver | AC 7, 8 | Not Started | sonnet | |
| 6 | Fauna driver | AC 9 | Not Started | sonnet | |
| 7 | Live switch and commands | AC 10 | Not Started | sonnet | |
| 8 | Isolation check, observability | AC 12, 13 | Not Started | haiku | |
