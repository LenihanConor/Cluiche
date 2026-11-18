# Feature Spec: A4 Climate Cycles

**Parent:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.md (Feature 8)
**Status:** Approved (2026-11-17, user, after the locked-GDD alignment). Amended 2026-11-17 (aligned to locked GDD, CHANGE-047; row 6d): GDD field names, cosine SINE, the 3-point ice-age keyframe curve, `phase_days` / `pinned_phase_days`, frozen-phase spin-up and the eighth layer.
**Design source:** GDD `A4_TechnicalSpec.md` §19, `A4_TuningParameters.md` §10b, `A4_WeatherSystem.md` decision 8. A4 was **re-locked 2026-11-17 (CHANGE-047)**. User decisions of 2026-11-17 win where the locked GDD differs (see "GDD differences").
**Depends on:** A4 Feature 1 (`WeatherParameters`, `ParamError`, file envelope conventions, `WeatherTerrain`); A4 Feature 2 (forcing points: solar heating, §6.1 evaporation, §5.1 prevailing wind); A4 Feature 3 (`Capture`, spin-up job, `WeatherRunner`); A4 Feature 4 (save); A4 Feature 6 (preset files). Touched by A5 (reads the start coldness).

## Summary

A4 gets slow climate. Long cycles push three things up and down over years and decades: **air temperature**, **ocean evaporation** and the **latitude of the prevailing winds** (so storm tracks drift toward or away from the equator). The first target is a ~4-year El Niño-style oscillation. The second is a ~40-year ice-age cycle. During the cold phase snow persists, glaciers grow and lakes fall (A5 does the water part).

Cycles are data. A file lists any number of them. Each has a period (in days, authored as whole 364-day game years), a phase, a signed amplitude per field, a shape (a cosine `SINE`, or a `KEYFRAME` curve for lopsided cycles like "slow cooling, fast warming") and an optional regional pattern. A cycle is a **pure function of the game day**: no RNG, and no state except its phase settings. Several cycles add up per field. Position `u = 0` is the **warm point** of every cycle.

The cycles enter the existing pipeline as **forcings**, latched at `Capture` and applied inside `Run`. They add no pass, no new CA state and no new job.

Spin-up does not get longer. The normal 52-week spin-up runs with every cycle **frozen at the world's start phase**. A5's slow stores (glaciers, deep lakes) start from a seed scaled to how cold that phase is. Feature 8 supplies the coldness value. A5 owns the seed.

## Goals

1. A versioned cycle file with strict validation, atomic overrides and hot reload, in the same style as A4's parameter file.
2. Layered cycles: sine or keyframe shape, per-field amplitude, optional regional pattern, any period from a week up.
3. A pure `ClimateCycles` evaluator: the same day, the same definitions and the same start phases always give the same per-cell offsets.
4. A per-world start phase (default: the warm point), from config, world seed or bundle.
5. Offsets applied as forcings inside the existing pipeline, bit-identical at any range count, and exactly zero when no cycle is active.
6. Frozen-phase spin-up, and a coldness value for A5's seed.
7. Presets can switch cycles off or pin a phase.
8. Saved with the game, and observable: current phase and value per cycle, history, and a scrub hook.

## Non-Goals

- **A5's slow-store seed.** A5 maps `Coldness` to initial glacier and lake state. Feature 8 only exposes the value.
- **Dragon and player control of the climate.** That is Feature 7 (influences). Cycles are fixed world rhythms.
- **Random or event-driven climate shifts.** No RNG, by design.
- **Moisture offsets.** An earlier GDD draft had a moisture amplitude. The locked GDD and this spec have none (see "GDD differences").
- **Forecasting.** Nothing reads future cycle values for the player.
- **Changing A4's parameters.** Cycles never touch `WeatherParameters`. They are forcings on top.

## Design

### Calendar

The CoW year is **364 days** (52 weeks; `kDaysPerYear` in `GameSpeedController.h`). Periods in years use that. A "4-year" cycle is 1,456 days and a "40-year" cycle is 14,560 days (aligned to locked GDD §19, which now uses 364-day game years).

### The cycle file

`Assets/CoW/Global/Weather/climate_cycles.json`, envelope `{"format": "cow.climate_cycles", "version": 1, "climate_cycles": [ ... ]}` (the GDD's `climate_cycles` list key and cycle field names; the envelope and the Cluiche asset path are additions). Same strict rules as `weather_parameters.json` (Feature 1):
- unknown keys are invalid, at every level;
- the load is all-or-nothing;
- the error names the first offending path (`climate_cycles[1].amplitude.temperature`).

```json
{
  "format": "cow.climate_cycles", "version": 1,
  "climate_cycles": [
    {
      "id": "oscillation_4y", "enabled": true,
      "period_days": 1456, "phase_days": 0, "shape": "SINE",
      "amplitude": { "temperature": 0.03, "evaporation": 0.15, "wind_latitude_shift": 0.03 },
      "pattern": { "type": "COAST_INLAND", "coast": 1.0, "inland": 0.5 }
    },
    {
      "id": "ice_age_40y", "enabled": true,
      "period_days": 14560, "phase_days": 0,
      "shape": { "type": "KEYFRAME", "points": [[0.0, 0.0], [0.6, 1.0], [1.0, 0.0]] },
      "amplitude": { "temperature": -0.12, "evaporation": -0.2, "wind_latitude_shift": -0.05 },
      "pattern": { "type": "LATITUDE", "weight_at_equator": 0.6, "weight_at_pole": 1.0 }
    }
  ]
}
```

These are the GDD §10b default values exactly (aligned to locked GDD 2026-11-17).

| Field | Meaning | Range (all provisional unless noted) |
|---|---|---|
| `id` | Unique name, a stable key for saves, presets and commands | 1–31 chars, `[a-z0-9_]`; unique |
| `enabled` | The file's default; presets and commands can override at runtime | bool |
| `period_days` | Integer days. Author whole game years × 364 (4 years = 1,456; 40 years = 14,560) | 7 ≤ `period_days` ≤ 36,400 |
| `phase_days` | Offset added to the day; default 0 = start at the warm point. Saved per world (see Start phase) | integer in [0, `period_days`) |
| `shape` | `"SINE"`, or `{"type": "KEYFRAME", "points": [[u, value], ...]}` | enum |
| keyframe `points` | `[u, value]` pairs. Linear between points, wrapping at phase 1. `u` strictly increasing, first `u` = 0, last `u` = 1, first value = last value (the curve must close) | 2–64 points; values in [-1, 1] |
| `amplitude.temperature` | Signed offset added to the seasonal baseline at wave = 1 | [-0.3, 0.3] |
| `amplitude.evaporation` | Signed change to the ocean-evaporation multiplier at wave = 1 | [-0.9, 0.9] |
| `amplitude.wind_latitude_shift` | Signed shift of §5.1's prevailing-wind band edges, in normalised latitude units, positive = poleward | [-0.25, 0.25] |
| `pattern` | See below. Omitted = `GLOBAL` | enum |
| `pinned_phase_days` | Optional. Freezes the cycle at one point (presets); replaces `day` in the position formula | integer |

There is **no moisture amplitude** (user decision 2026-11-17, kept by the locked GDD).

**Wave.** With `u` in [0, 1):
- `SINE`: `wave(u) = cos(2π u)`, in [-1, 1]. It peaks at `u = 0`, so a positive temperature amplitude gives the warm point at `u = 0`.
- `KEYFRAME`: linear interpolation between the two surrounding points.

**Authoring convention (locked GDD §19).** **`u = 0` is the warm point of every cycle.** A SINE cycle's temperature amplitude is positive, so it peaks at the warm point. A KEYFRAME curve's value at `u = 0` is its warm value, and its `temperature` amplitude is negative when the curve measures how cold it is. The default ice age runs `(0, 0)` warm, slow cooling to `(0.6, 1)` coldest, faster warming back to `(1, 0)`. Nothing can validate this, so it is documented and covered by the default file's tests.

**Patterns.** A pattern gives each cell a weight; the cycle's offset at that cell is `amplitude × weight × wave`.

| Type | Weight | Notes |
|---|---|---|
| `GLOBAL` | 1 | Default |
| `LATITUDE` | `weight_at_equator + (weight_at_pole − weight_at_equator) · latitude(cell)` | Uses `WeatherTerrain::latitude` [0, 1] |
| `COAST_INLAND` | `coast + (inland − coast) · clamp(d / dMax, 0, 1)` | `d` is the hex distance to the nearest ocean or water-source cell, found once by a multi-source breadth-first search in `Initialise`. `dMax` = 6 (provisional; matches Feature 6's "inland" cut-off) |
| `REGIONS` | `weight` on cells whose S-1 region id is in `ids`, 0 elsewhere | Region ids come from S-1's `RegionTable` / `Cell::region`. See Open Design Question 3 |

Pattern weights are computed once at `Initialise` (and on reload) and stored per cycle per cell. At most 16 cycles (provisional) keeps that to 16 float arrays, about 430 KB at Large in the worst case. `GLOBAL` cycles store nothing.

### Start phase (per world)

A cycle's position at game day `d` is the GDD's formula (§19):

```
u(c, d) = ((pinned_phase_days ?? d) + phase_days) mod period_days / period_days      // integer arithmetic, then one divide
```

`phase_days` is the **world's phase** for that cycle, an integer in [0, period_days). **u = 0 is the warm point**, so the default `phase_days = 0` starts every cycle at its warm point (worlds begin warm; A5 TechnicalSpec §13). The saved `phase_days` is the file's value unless the world was created with a `ClimateStartSpec` (a Cluiche extension; the GDD has only the default and the saved value):

| Mode | Saved `phase_days` |
|---|---|
| `warm` (default) | the file's `phase_days` (0 in the default file) |
| `fraction` | `round(f · period_days)`, per cycle id, from config or a bundle |
| `seeded` | `hash(worldSeed, cycleId) mod period_days`, a stable hash with no RNG state |

Precedence: a preset or bundle value, then the `.diaapp` module config, then `warm`. The config block is `"climate": {"file": "Global/Weather/climate_cycles.json", "start": "warm"}`, or `"start": {"ice_age_40y": 0.5}` for per-cycle fractions.

### Switching off and pinning

Each cycle has runtime flags beside its definition:
- `enabled` overrides the file's default;
- `pinned_phase_days` (optional) freezes the cycle at one point: it replaces `d` in the formula above, so `u = ((pinned_phase_days + phase_days) mod period_days) / period_days`, whatever the day (locked GDD §19). Presets set it.

A disabled cycle contributes exactly 0 (not "wave 0": the multiplier path is skipped, so the result is bit-identical to the cycle being absent).

Presets (Feature 6) may carry a `climate` block. Applying a preset **resets** climate flags to "all cycles disabled" first, then applies the block, in the same way `ApplyPreset` already resets forcings. This keeps every existing preset and regression scenario cycle-free unless it asks (Feature 6 amendment).

### Evaluation and where the offsets enter

`ClimateCycles::Evaluate(day, out)` fills three per-cell arrays:

```
tempOffset[c]    = clamp( Σ cycles  amp.temperature · weight(c) · wave , -0.3, +0.3 )
evapMultiplier[c]= clamp( 1 + Σ cycles amp.evaporation · weight(c) · wave , 0.25, 2.0 )
windShift[c]     = clamp( Σ cycles  amp.wind_latitude_shift · weight(c) · wave , -0.25, +0.25 )
```

The clamps are provisional "sane" limits (the GDD gives none). Cycles are summed in file order, in `double`, then stored as `float`.

| Pass (Feature 2) | Use |
|---|---|
| 1, solar heating (§3) | `tempOffset[c]` is added to the seasonal baseline temperature before the clamp |
| 4, primary evaporation (§6.1) | the ocean and open-water rate is multiplied by `evapMultiplier[c]` (the GDD's `(1 + cycle_offsets.evaporation)`). Secondary (ground) evaporation §6.2 is untouched. (Corrected 2026-11-17: this row said "5/6"; evaporation is pass 4) |
| 3, wind (§5.1) | `windShift[c]` is added to every band edge (0.25, 0.35, 0.55, 0.65), so the trade winds and westerlies move toward or away from the poles. Implemented as a lookup at the **effective latitude** `clamp(latitude[c] − windShift[c], 0, 1)`, which is the same thing. A positive shift moves the bands poleward and storm tracks follow them. The column-parity snap rule for ties is unchanged |

A5 sees the offset through A4's temperature, so it needs no separate path.

**When.** The evaluation day is the **step day** (`stepDay` given to `Capture`; the week and season already come from it). `Capture` calls `WeatherSimulation::LatchClimate(evalDay)`, which runs `Evaluate` on SimPU (at most 16 cycles × N cells, about 100k multiply-adds at Large) and copies the three arrays into the working set. `Run` reads only those latched arrays (DFS-002). With no active cycle, the arrays are not built and the pipeline takes its existing path unchanged. The GDD's "step 0 forcing" applies the cycle offsets at the start of the step; here they are latched at `Capture` and consumed by passes 1, 3 and 4 (the same values, no behavioural difference).

### Spin-up (frozen phase)

The 52-week spin-up (Feature 3's worker job, `WeatherRunner`) evaluates the cycles **once**, at the day the live run will start (`freezeDay`, normally 0, or the preset's calendar start), and reuses those offsets for all 52 weeks. Because the first live week evaluates the same day with the same saved phases, the offsets at the end of spin-up and the start of live play are identical: no jump. This is the GDD's frozen-phase spin-up (locked §19): A4 and A5 spin up together for 52 weeks (`SPINUP_WEEKS`) with the cycles held at the world's start phase, and there is **no long spin-up**. A5's slow stores (glaciers, deep lakes) start from a phase-scaled seed that A5 owns, from `StartColdness` below.

`ClimateCycles::Coldness(cell, day)` returns a value in [0, 1] for A5:

```
T(c, d)  = Σ cycles amp.temperature · weight(c) · wave          (enabled and unpinned or pinned)
Tmin(c), Tmax(c) = the same sum with each cycle at its own extreme wave (curve min and max)
coldness = (Tmax − T) / (Tmax − Tmin)        0 = warmest possible, 1 = coldest possible; 0 when Tmax == Tmin
```

`WeatherRunner` exposes `StartColdness(cell)` (coldness at `freezeDay`). A5 maps it to a glacier and lake seed. No coupling beyond this one accessor.

### Save/load (Feature 4 amendment)

Because the cycle is a pure function of the day, the game day (already saved by GameSpeedController) plus each cycle's runtime phase state is the whole story. Feature 4's `climate` block saves:

| Saved | Why |
|---|---|
| `cycles_hash` | 64-bit hash of the effective cycle definitions (like `params_hash`). A mismatch warns and runs under the current file |
| per cycle: `id`, `enabled`, `phase_days`, `pinned_phase_days` (optional) | The GDD's saved set (locked §19: each cycle's `enabled`, `phase_days` and `pinned_phase_days`). A seeded or config-chosen start never has to be re-derived on load |

Not saved: the per-cell offset arrays, pattern weights and wave values. They are recomputed from the definitions and the day. Matching on load is by `id`. A saved id missing from the current file is dropped with a warning. A file cycle missing from the save takes its phase from the current `ClimateStartSpec`. A save with no `climate` block loads as "no active cycles" with no warning. (`cycles_hash` is a Cluiche addition to the GDD's saved set.)

A step in flight at save time needs nothing extra: its working set already contains the latched offsets (DFS-005).

### Debug and observability

No new console names (the weather names stay at 8, with `cow.weather.influence` from Feature 7). Both use the existing argument fields (`what` for the `info` query, `action` for `params`):
- `cow.weather.info {"what":"cycles", "day": <optional>}` returns, per cycle: id, enabled, pinned, `periodDays`, `u`, `wave`, and the global-weight contribution to each of the three fields. It also returns the totals and `coldness` for a chosen cell. With no `day` it uses the current step day. **With any `day` it is the scrub hook**: S-2.5 and S-2.6 call it for the scrubbed day, because evaluation is pure and needs no recording.
- `cow.weather.params {"action":"cycle", "id":..., "enabled":bool | "pin": pinnedPhaseDays | "unpin":true | "start": phaseDays}` queues a runtime change. It is latched at the next `Capture`. `{"action":"reload","target":"cycles"}` re-reads the file between steps.
- Panel card: one row per cycle (id, `u` as a percentage, wave, on/off/pinned) plus totals.
- Cell layer `weather.climate_offset` (the applied `tempOffset`, min −0.3, max +0.3). It is the **eighth scalar base layer** (Feature 5 registers it beside the seven field layers; with the two wind component layers that is ten layers in all). It shows the regional pattern and can be added to a history `track` list. It is **not** tracked by default (the `history` config is capped at 255 characters and every layer shortens the scrub window).
- The history of the cycle values themselves needs no ring: `Evaluate(day)` reproduces any past day exactly. Where DiaCellHistory offers a scalar series, S-2.5 may also sample the per-cycle waves into it. This is uncertain: DiaCellHistory tracks cell layers today, and a scalar series may not exist (see "Files Touched").
- Metrics: one gauge per cycle, `cow.weather.cycle.<id>.wave`, written once per `Capture` on SimPU (at most 16; SD-PERF-7).
- Logging, channel `WTH`: Info on load and reload (path, cycle count) and on a runtime change; Warning on a clamp, an unknown preset cycle id or a `cycles_hash` mismatch; Error on a bad file.
- Health: an invalid file gives **Degraded** and **no active cycles**. A missing file means no cycles and an Info log (cycles are optional).

## Public API

```cpp
namespace CoW::Weather {

    enum class CycleShape       : uint8_t { Sine, Keyframe };      // JSON "SINE" / "KEYFRAME" (locked GDD §19)
    enum class CyclePatternType : uint8_t { Global, Latitude, CoastInland, Regions };

    struct CycleKeyframe  { float u; float value; };
    struct CycleAmplitude { float temperature; float evaporation; float windLatitudeShift; };
    struct CyclePattern   { CyclePatternType type = CyclePatternType::Global;
                            float a = 1.0f, b = 1.0f;          // latitude: weight_at_equator/weight_at_pole; coast: coast/inland; regions: weight (b unused, outside weight is 0)
                            DynamicArrayC<uint16_t> regionIds; };   // regions only: S-1 region ids

    struct ClimateCycleDef {
        Dia::Core::StringCRC   id;
        char                   idText[32];
        bool                   enabled = true;
        uint32_t               periodDays = 0;
        uint32_t               phaseDays = 0;                 // default 0 = warm point
        bool                   hasPinned = false;
        uint32_t               pinnedPhaseDays = 0;           // presets
        CycleShape             shape = CycleShape::Sine;
        DynamicArrayC<CycleKeyframe> keyframes;               // Keyframe only
        CycleAmplitude         amplitude{};
        CyclePattern           pattern{};
    };

    struct ClimateCycleSet { DynamicArrayC<ClimateCycleDef> cycles; };      // at most 16

    // Same conventions as weather-data.md: false = invalid, out untouched or empty, err names the path.
    bool LoadClimateCycles   (const Json::Value& root, ClimateCycleSet& out, ParamError& err);
    bool ApplyCycleOverrides (const Json::Value& overrides, ClimateCycleSet& inOut, ParamError& err);   // atomic
    uint64_t HashClimateCycles(const ClimateCycleSet& set);

    enum class ClimateStartMode : uint8_t { Warm, Fraction, Seeded };
    struct ClimateStartSpec {
        ClimateStartMode mode = ClimateStartMode::Warm;
        DynamicArrayC<Dia::Core::StringCRC> ids;              // Fraction mode: per-cycle id
        DynamicArrayC<float>                fractions;        //                and its fraction
    };

    struct ClimatePhaseState {                                 // runtime, saved (the GDD's saved set)
        Dia::Core::StringCRC id;
        bool     enabled;
        uint32_t phaseDays;
        bool     pinned;
        uint32_t pinnedPhaseDays;
    };

    struct ClimateOffsets {                                    // latched per Capture; read-only in Run
        DynamicArrayC<float> temperature;                      // added to baseline temperature
        DynamicArrayC<float> evaporationMultiplier;            // ocean/open-water evaporation
        DynamicArrayC<float> windLatitudeShift;                // positive = poleward
        bool active = false;                                   // false = arrays empty, pipeline unchanged
    };

    class ClimateCycles {
    public:
        // Computes pattern weights and the per-cell coast distance. regionOfCell is S-1's Cell::region per cell
        // (null when a REGIONS pattern is not used). False if a cycle names a region id absent from the world.
        bool Initialise(const ClimateCycleSet& set, const WeatherTerrain& terrain,
                        const ClimateStartSpec& start, uint64_t worldSeed,
                        const DynamicArrayC<uint16_t>* regionOfCell = nullptr);

        double WaveAt(uint32_t cycleIndex, uint64_t day) const;          // pure: [-1, 1]
        double PhaseAt(uint32_t cycleIndex, uint64_t day) const;         // u in [0, 1)
        void   Evaluate(uint64_t day, ClimateOffsets& out) const;        // pure
        float  Coldness(uint32_t cell, uint64_t day) const;              // [0, 1], for A5

        void   SetEnabled(StringCRC id, bool enabled);
        void   Pin(StringCRC id, uint32_t pinnedPhaseDays);   void Unpin(StringCRC id);
        void   DisableAll();                                             // ApplyPreset reset
        uint32_t GetCycleCount() const;
        const ClimatePhaseState& GetPhaseState(uint32_t cycleIndex) const;
        void   RestorePhaseState(const ClimatePhaseState& state);        // load
    };

    // WeatherSimulation additions
    //   void SetClimate(const ClimateCycles* climate);          // null = no cycles (default)
    //   void LatchClimate(uint64_t evalDay);                    // Capture and the runner call this; fills the working ClimateOffsets
    // WeatherRunner additions
    //   Start(..., uint64_t climateFreezeDay = 0);              // spin-up evaluates once at this day
    //   float StartColdness(uint32_t cell) const;
}
```

## Acceptance Criteria

### File and validation
1. A valid file loads. `DefaultCycles()` (the shipped file's content) loads and has the two cycles above, equal to the GDD §10b values (periods 1,456 and 14,560 days, `phase_days` 0, the amplitudes and patterns listed).
2. Rejected, with the error naming the path: wrong `format` or `version`; an unknown key at any level (including the removed `period`, `phase_offset` and any moisture amplitude); duplicate or malformed `id`; unknown `shape` or pattern `type`; `period_days` out of range; `phase_days` ≥ `period_days`; a non-finite number; an amplitude out of range; more than 16 cycles.
3. Keyframe rules: `u` strictly increasing, starting at 0 and ending at 1, first value = last value, 2–64 points, values in [-1, 1]. Each violation is rejected.
4. A failed load changes nothing: `ApplyCycleOverrides` is atomic, and a rejected load leaves the previous set active.
5. `period_days` is taken as written: no unit conversion exists. The default periods are whole game years of 364 days (1,456 = 4 × 364, 14,560 = 40 × 364), asserted by a test against `kDaysPerYear`.
6. `HashClimateCycles` is stable for equal sets and changes when any definition field changes.

### Waves and patterns
7. SINE (`cos 2πu`, locked GDD §19): `WaveAt` is 1 at `u = 0`, 0 at `u = 0.25`, -1 at 0.5, 0 at 0.75. A positive temperature amplitude therefore peaks at `u = 0`, the warm point.
8. KEYFRAME: values at the keyframe points are exact, and midway between two points is the average. The curve wraps at phase 1. A curve with a slow rise and a fast fall gives an asymmetric wave.
9. The default ice-age curve is `(0, 0) → (0.6, 1) → (1, 0)`: wave 0 at `u = 0`, 0.5 at `u = 0.3`, 1 at `u = 0.6`, 0.5 at `u = 0.8`, 0 at `u = 1`, so cooling takes 60% of the period and warming back 40%. With the `-0.12` amplitude the temperature offset at `u = 0.6` is −0.12 at a `weight` of 1.
10. `u(d)` uses integer arithmetic: `WaveAt(d)` equals `WaveAt(d + period_days)` exactly, for 1,000 sampled days.
11. `phase_days` and `d` shift `u` as the formula says, and wrap at the period.
12. Patterns, on a hand-built terrain:
    - `GLOBAL`: weight 1 everywhere.
    - `LATITUDE`: weight `weight_at_equator` (0.6) at the equator, `weight_at_pole` (1.0) at the pole, linear between.
    - `COAST_INLAND`: weight `coast` at a coastal cell, `inland` at distance ≥ 6, linear between.
    - `REGIONS`: `weight` on cells in `ids`, 0 elsewhere. A region id absent from the world makes `Initialise` return false.

### Summing, clamps, switches
13. Two cycles sum per field. Order does not matter to within `float` rounding, and file order is the defined order.
14. The temperature total, the evaporation multiplier and the wind shift never leave their clamps, even with 16 cycles at maximum amplitude.
15. A disabled cycle gives results bit-identical to the cycle being absent from the file.
16. A pinned cycle (`pinned_phase_days`) gives the same wave on every day, equal to the wave at `u = ((pinned_phase_days + phase_days) mod period_days) / period_days`. `Unpin` restores the day-driven wave.
17. With no cycles, `ClimateOffsets::active` is false, and **104 weeks of a run are bit-identical to a `WeatherSimulation` with no climate set** (the regression that protects Features 1–6).

### Start phase
18. `warm`: the saved `phase_days` is the file's (0 in the default file) for every cycle, and day 0 is the warm point: wave 1 for `oscillation_4y` (cosine) and wave 0 for `ice_age_40y` (keyframe), so both temperature offsets are at their warm values.
19. `fraction`: `phase_days = round(f · period_days)`, per id; an unknown id warns and is ignored.
20. `seeded`: the same seed and id always give the same `phase_days`; different seeds differ for at least one cycle in a set of 8 seeds; no RNG state is touched.
21. Precedence: preset beats config, config beats default.

### Pipeline
22. Temperature: with a uniform `tempOffset` of +0.05, a no-wind, no-moisture world's seasonal baseline rises by 0.05 before the clamp (pass 1 test).
23. Evaporation: with `evapMultiplier = 2` on an ocean cell, primary evaporation doubles (unless saturated). Secondary evaporation is unchanged.
24. Wind: with `windShift = +0.1`, the latitude band boundary moves poleward by 0.1, including the transition lerp. With shift 0, directions equal the existing function exactly.
25. Range independence: all fields are bit-identical at 1, 3, 7 and 17 ranges, and with ranges run in reverse, over 104 weeks with both default cycles on.
26. Capture isolation: after `LatchClimate`, poisoning the `ClimateCycles` object does not change `Run` (DFS-002).
27. Deferred vs synchronous: a `WeatherStage` driven through the manual executor matches `WeatherRunner` bit-for-bit over 104 weeks with cycles on.

### Spin-up
28. Frozen phase (locked GDD §19): spin-up offsets are identical for all 52 weeks and equal `Evaluate(freezeDay)`.
29. Continuity: the offsets of the last spin-up week equal those of the first live week, for `warm`, `fraction` and `seeded` starts.
30. `StartColdness`: 0 on the default `warm` start; 1 for a cell when every cycle is pinned at its coldest wave; always in [0, 1]; 0 when `Tmax == Tmin`. It is deterministic.
31. Spin-up length stays `spinup_weeks` (`SPINUP_WEEKS`, 52). Enabling cycles does not change it, and nothing extends it ("spin-up extended by cycles" is gone).
31a. (amended 2026-11-17) `StartColdness` is the only value the cycles hand to A5 for its phase-scaled slow-store seed; A5 owns the seed.

### Presets
32. `ApplyPreset` disables every cycle before applying its `climate` block, so a preset without the block is cycle-free whatever ran before.
33. A preset block can disable a named cycle and pin another at a given `pinned_phase_days`. An unknown id is rejected as an invalid preset.

### Save
34. Save and reload round-trips each cycle's `enabled`, `phase_days` and `pinned_phase_days`. A reload continues bit-identically to the uninterrupted run, with a seeded start and a pin both set.
35. A `cycles_hash` mismatch warns and keeps the saved phases for matching ids. A save without a `climate` block loads with no active cycles and no warning.
36. A save taken with a step in flight reloads identically.

### Debug
37. `cow.weather.info {"what":"cycles"}` reports each cycle's `u` and `wave`, and they equal `PhaseAt` and `WaveAt`. With `"day": D` they equal the values at `D`, whatever the current day.
38. `cow.weather.params {"action":"cycle", ...}` changes take effect at the next `Capture`, never mid-`Run`. The command count stays 8 (Feature 5, with Feature 7's `cow.weather.influence`).
39. `weather.climate_offset` (the eighth scalar base layer) equals the latched `tempOffset` for the committed week. The layer has min and max in its schema.
40. Gauges: at most one per cycle, written on SimPU once per `Capture`. An invalid file leaves health Degraded with no active cycles.

## Files Touched

| File | Change |
|---|---|
| `Cluiche/CoW/Shared/Weather/ClimateCycles.h/.cpp` | New: defs, wave and phase maths, patterns, `Evaluate`, `Coldness`, start spec, phase state |
| `Cluiche/CoW/Shared/Weather/ClimateCycleFile.h/.cpp` | New: load, validate, overrides, hash |
| `Cluiche/CoW/Shared/Weather/WeatherSimulation.h/.cpp` | `SetClimate`, `LatchClimate`, working `ClimateOffsets` (Feature 2's file) |
| `Cluiche/CoW/Shared/Weather/WeatherPipeline*.cpp` | Read `ClimateOffsets` in passes 1, 3 and 4 (§6.1) (Feature 2's files) |
| `Cluiche/CoW/Shared/Weather/WeatherRunner.h/.cpp` | `climateFreezeDay`, `StartColdness` (Feature 3's file) |
| `Cluiche/CoW/Modules/WeatherModule.cpp`, `WeatherSaveParticipant.cpp` | Load/reload the file, `LatchClimate` in `Capture`, `climate` save block |
| `Cluiche/CoW/Modules/WeatherDebugDomain.cpp`, command executor | `cycles` info action, `cycle` and `reload` params actions, panel rows, the `weather.climate_offset` layer, gauges |
| `Cluiche/CoW/Shared/Weather/WeatherPreset*.cpp` | `climate` block and the reset rule (Feature 6's file) |
| `Cluiche/Assets/CoW/Global/Weather/climate_cycles.json` | New: the two default cycles |
| `Cluiche/Assets/CoW/Applications/cow_main.diaapp` and the S-2.5 stage config | `climate` config block |
| `Tests/GoogleTests/CoW/Weather/TestClimateCycles*.cpp` | New: ACs 1–40 |
| `Cluiche/CoW/Shared/Weather/Testing/ClimateTestCycles.h` | New: hand-built cycle sets and terrains for tests |
| `CoW.vcxproj` (+ filters), `GoogleTests.vcxproj` (+ filters) | Via `dia docs vcxproj-add` |

No Dia files expected, so no AD-001 sign-off. If DiaCellHistory has no scalar-series API and S-2.5 wants one, that would be a Dia change and must be flagged first.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-002 | ProcessingUnit/Phase/Module architecture; no singletons | `ClimateCycles` is owned by `WeatherModule`. Nothing is global |
| PD-004 | No STL containers in public APIs | `DynamicArrayC`, `StringCRC` and fixed char arrays throughout |
| PD-005 | x64 is the only build target | Wave maths in `double` with one fixed formula. Bit-identity is claimed per build on x64; AC 10 and the golden values catch drift |
| AD-002 | Four-tier dependency model | CoW-Shared pure logic, module wiring in `Modules/` |
| AD-003 | GDD owns design intent | The GDD is locked (CHANGE-047). Every number not in the GDD is marked provisional |
| DFS-002 | `Run` reads only the captured working copy | Offsets are latched at `Capture` into the working set (AC 26) |
| SD-PERF-7 | Telemetry cost budgets | At most one gauge per cycle, written once per `Capture` on SimPU. No per-cell telemetry |

## GDD differences

Re-checked against the locked GDD (CHANGE-047, 2026-11-17). Items the GDD now matches are struck. Where the locked GDD still differs, the Cluiche decision is kept.

1. ~~**Fields.**~~ **Resolved by GDD §19 (locked):** amplitude is `{temperature, evaporation, wind_latitude_shift}`; no moisture amplitude.
2. ~~**Shapes.**~~ **Resolved by GDD §19 (locked):** shapes are `SINE` and `KEYFRAME`; the ice age is the 3-point curve `(0, 0), (0.6, 1), (1, 0)`. The 17-point RAISED curve is gone from this spec.
3. ~~**Period.**~~ **Resolved by GDD §19 (locked):** 364-day game years, 1,456 and 14,560 days. Only `period_days` is accepted (the earlier days/weeks/years unit form is dropped).
4. ~~**Start phase.**~~ **Resolved by GDD §19 (locked):** `phase_days`, default 0 = warm point, `u = frac((day + phase_days) / period_days)`. The world start modes (`warm`, `fraction`, `seeded`) remain a Cluiche extension that sets the saved `phase_days`.
5. ~~**Pinning.**~~ **Resolved by GDD §19 (locked):** `pinned_phase_days`.
6. **Saved state (partly differs, Cluiche kept).** The GDD saves `enabled`, `phase_days` and `pinned_phase_days`. Here the save also holds a definition hash (`cycles_hash`).
7. ~~**Spin-up.**~~ **Resolved by GDD §19 (locked):** frozen-phase 52 weeks, phase-scaled A5 seed, no long spin-up.
8. **File (still differs, Cluiche kept).** The GDD has `data/climate_cycles.json`. Here it is `Assets/CoW/Global/Weather/climate_cycles.json` with a `cow.climate_cycles` envelope, strict like the parameter file. The list key is the GDD's `climate_cycles`.
9. **Sum clamps and caps (new, Cluiche).** The GDD gives none. Here they are provisional (16 cycles, 64 keyframes, the three total clamps).
10. ~~**Defaults not in the GDD.**~~ **Resolved by GDD §10b (locked):** the wind-shift amplitudes (+0.03 and −0.05) and the 3-point curve are the GDD's.
11. **Invalid file (new, Cluiche).** A bad parameter file falls back to GDD defaults. A bad cycle file falls back to **no cycles**, because cycles are optional and a surprise ice age is worse than none.
12. **`REGIONS` pattern (partly differs, Cluiche kept).** The GDD's `REGIONS { ids, weight }` has no outside weight, so outside is 0 here too (the earlier `outside_weight` field is dropped). Ids are S-1 region ids, resolved through `Cell::region`.

## Amendments owed elsewhere

**A4 items applied 2026-11-17 (row 6d of `a4-weather.plan.md`):**

- **A4 system spec:** done. The "Coupled spin-up" paragraph now says 52 weeks with cycles frozen at the start phase; Feature 8's row says frozen-phase spin-up and "phase settings saved"; Open Design Question 5 is closed; the Features table links this spec.
- **A4 Feature 2:** done. `ClimateOffsets` input to passes 1, 3 and 4; the wind prevailing-direction helper takes an effective latitude (`PrevailingDirection(latitude − shift)`).
- **A4 Feature 3:** done. `LatchClimate` in `Capture`; `WeatherRunner::Start(climateFreezeDay)` and `StartColdness`; module config `climate` block; loading and hot reload of the cycle file.
- **A4 Feature 4:** done. The `climate` save block (AC 34–36).
- **A4 Feature 5:** done. The `cycles` info action; the `cycle` and `reload target:cycles` params actions; the eighth scalar base layer `weather.climate_offset` (ten layers with the wind components); panel rows; gauges.
- **A4 Feature 6:** done. The preset `climate` block and the rule that `ApplyPreset` disables all cycles first.

**Still owed outside the A4 files:**

- **A5 system spec, `hydrology-module.md` and `water-pipeline.md`:** the "extended to the longest active climate cycle's settling period" wording (`hydrology-module.md` lines about spin-up length and its GDD-differences table) is stale; it is 52 weeks frozen-phase. The "Initial state and spin-up" text and A5 Open Design Question 1 (long coupled spin-up; cached start state) become moot. A5 owns the glacier and lake seed from `StartColdness(cell)`, needs a parameter set for it (Feature 1) and a regression that a cold start settles by the end of the 52 weeks.
- **F-3 World Assets:** a bundle may carry a `ClimateStartSpec`, and the cycle file may become a weather-pack-like asset.
- **S-2.5 / S-2.6:** a cycle panel and the scrub hook; a "cycles off" default for the nine GDD success tests.
- **GDD:** the fold-back list above.

## Test Plan

TDD, in this order, on hand-built terrains (`ClimateTestCycles.h`):
1. File load, validation and overrides.
2. Wave maths and the integer phase formula.
3. Patterns, including the coast distance search.
4. Summing, clamps, disable and pin.
5. Start phase modes.
6. The three pipeline entry points, one test each.
7. Range independence, capture isolation and deferred-vs-synchronous.
8. Frozen-phase spin-up, continuity and coldness.
9. Preset reset, save round-trip, debug commands.

The "no cycles equals no climate" test (AC 17) is written first against the Feature 2 pipeline, as it protects everything already built. Scoped `--filter="*Climate*"` while iterating and one full `--shards 2` run at the end. The exhaustive gap pass is a plan task. No mutation checks.

## Open Design Questions

1. **Will the CA damp or amplify the amplitudes?** The temperature offset is added to the seasonal baseline each week, then moves through transport (§8) and pressure. The delivered swing may be smaller or larger than ±0.03 or -0.12. Should amplitudes mean "forcing size" (as here) or "target swing", with a per-cycle gain found by calibration? The first S-2.5 run on the default file decides. Until then amplitudes are forcing sizes.
2. **Warm start versus seeded starts.** Worlds start warm by default, so every new game has the ice age 20 years away. Seeded starts give variety but may begin deep in an ice age. Should `seeded` be limited to a safe range (for example the warm half of the curve) so no world begins in the coldest phase? Not decided; `fraction` can already do it by hand.
3. ~~**Where do named region sets come from?**~~ **Resolved by GDD §19 (locked): `REGIONS { ids, weight }` uses region ids**, here S-1 `RegionTable` / `Cell::region` ids (world-specific). Needed only if a cycle really wants one; the two default cycles do not.
4. **Linear keyframes have corners.** A piecewise-linear curve has a visible kink in the offset at each point, and the CA responds to the rate of change. Smooth interpolation (cubic or smoothstep) is possible but costs the exact-at-points, easy-to-test property. The GDD's default ice age has only three points, so the kink at the `u = 0.6` apex is the sharpest one. Linear is the default (the GDD says "linear between points"); revisit if S-2.5 shows kinks in the debug layers.

## Status

**Status:** Approved (2026-11-17, user, after the locked-GDD alignment). Amended 2026-11-17 (aligned to locked GDD, CHANGE-047; row 6d). The GDD is locked. User decisions encoded: data-driven layered cycles in a versioned file; temperature, ocean evaporation and prevailing-wind latitude shift as the three fields; `SINE` (cosine, peaks at `u = 0`) or `KEYFRAME` shape; optional latitude, coast/inland or region pattern; deterministic pure function of the day; summed per field with clamps; per-world `phase_days` with a warm default of 0; presets can switch off or pin (`pinned_phase_days`); **frozen-phase 52-week spin-up with A5 seeding its slow stores from a coldness value**; applied as forcings at `Capture`/`Run`; debug via existing commands, a layer and a scrub hook; `enabled`, `phase_days` and `pinned_phase_days` saved. Approval waits on Features 2 and 3 being built and on A5 agreeing the coldness hand-off.

Plan: [climate-cycles.plan.md](climate-cycles.plan.md) (written 2026-11-17, not started).
