# Feature Spec: A4 Smoothing

**Parent:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.md (Feature 9)
**Status:** Approved (2026-11-17, user, after the locked-GDD alignment). Amended 2026-11-17 (aligned to locked GDD, CHANGE-047; row 6d): `INFLUENCE_PREVIEW_FADE_DAYS` as a parameter, the Feature 7 preview source, three-buffer notes with the ten-array `WeatherFields`.
**Design source:** GDD `A4_TechnicalSpec.md` §20, `A4_WeatherSystem.md` decision 9, `A4_TuningParameters.md` §10a (`INFLUENCE_PREVIEW_FADE_DAYS`). A4 was **re-locked 2026-11-17 (CHANGE-047)**. User decisions of 2026-11-17 win where the locked GDD differs (see "GDD differences").
**Depends on:** A4 Feature 3 (`Commit`, `WeatherSnapshot`, committed buffers); A4 Feature 7 (Weather Influence API: its active and pending influence list, by name only). Touches V-1 (reader).

**Amended 2026-11-18 (world-sim review, user decisions):** see "World-sim review amendment" below.

## Summary

The weather CA runs once a week. Anything that reads it every frame would see a state that jumps once a week. Feature 9 is a **presentation layer** that hides the jump.

Readers see a **blend of the last two committed weekly states**. The blend position is the day of the week, 7 days long, starting at each commit. It never extrapolates and never shows a state the CA did not produce. The price is a one-week visual lag, which is accepted.

It is **computed on demand per read**. `SmoothedWeather::Sample(cell, t)` is a pure function of two committed states. There is no per-frame or per-day pass over the grid. V-1 can do the same blend in a shader from two textures.

The second job is the **influence preview**. An influence from Feature 7 only lands a week or two after the request (captured at the next world step, applied at N+k, k = 7 days). The preview gives the player an immediate, visual-only hint: clouds building, clouds clearing, a warm or cool tint. It fades in over about a day and hands over to the real effect when that lands. It never feeds back into the CA and is never saved.

Smoothing has no effect on the simulation, its determinism or its saves.

## Goals

1. A pure, allocation-free `SmoothedWeather::Sample` (and a range variant) over two committed states.
2. Defined blend rules for every smoothed field, including the discrete precipitation type and the wind direction on the circle.
3. No work when nobody reads, and no full-grid pass per frame or per day.
4. A per-influence visual preview that fades in, then hands over to the real effect without a double count or a jump.
5. A guarantee that nothing here changes simulation state, determinism or saves.

## Non-Goals

- **A forecast.** Smoothing never shows a state the CA has not produced. Nothing reads next week.
- **Extrapolation or prediction.** The blend runs between two known states only.
- **The gameplay read API** (flight wind cost, visibility, fire risk, footing). It is still undecided (A4 Open Design Question 4; GDD PARK-030). This feature does not build it. `Sample` is its natural input, so the API can be added later without touching this spec's code.
- **V-1's shader and texture upload.** V-1 owns them. This feature defines the blend rules so the shader can match the CPU result.
- **Sub-week simulation.** The CA stays weekly.
- **Influence rules and effects.** Feature 7 owns regions, strengths, falloff and lifetime. The preview only reads its list.

## Design

### What is blended

At each `Commit`, the committed state moves one step back:

```
previous  <- committed
committed <- working
working   <- (old previous buffer, scratch; overwritten by the next Capture)
```

This is a rotation of three buffers: **no copy**. It costs one extra `WeatherFields` buffer (about 266 KB at Large with the ten-array `WeatherFields`; only the fields below are ever read from `previous`). The commit day is the apply day the module passes to `Commit` (Feature 3, amended 2026-11-17). `WeatherSnapshot` views of `previous` and `committed` both stay valid until the next `Commit`, the same lifetime rule as today.

`Commit` also records `commitDay`, the game day it applied (the apply key). `previous` is **not saved**. After a load, or at the very first commit, `previous` is a copy of `committed`, so every blend is a constant until the next commit.

### The blend position

```
t = clamp( ((day - commitDay) + dayFraction) / 7 , 0, 1 )
```

`day` is the current game day and `dayFraction` in [0, 1) is the position inside the day (the frame's share, if a reader wants sub-day smoothness). `SmoothWeekFraction(day, dayFraction)` computes it. At the commit, `t = 0` and readers still see `previous`, which is exactly what they saw one tick earlier, so there is no jump. Seven days later `t = 1` and they see `committed`, and the next commit rotates the buffers so `t` restarts at 0 on that same state. Commits are exactly 7 days apart (apply keys never slip, DFS-003), so `t` stays in [0, 1) in normal play and the clamp is a safety net only.

Readers pass `t` (one float) to `Sample`. They do not need to know about days.

### Field rules

| Field | Rule |
|---|---|
| `temperature`, `cloudCover`, `precipitation` (intensity), `snowPack`, `waterToGround`, `windSpeed` | `lerp(prev, curr, t)`. All stay inside their field ranges because lerp of two in-range values is in range |
| `windDirection` | Shortest arc on the circle (below) |
| `PrecipitationType` | Discrete rule (below) |
| `SnowCover` (bool) | `smoothed snowPack > kSnowCoverThreshold` (0.1). Derived, never blended |
| `moisture`, `pressure`, `pressureAnomaly` | Not smoothed. They are internal CA state, not reader fields |

**Wind direction.** Angles are radians in [0, 2π). With `a = prev`, `b = curr`:
1. If `windSpeed` of one end is below `kCalmSpeed` (0.02, provisional), its direction means nothing. Use the other end's direction for the whole week.
2. Otherwise `delta = wrap(b - a)` into (−π, π]. Direction is `a + t · delta`, wrapped back into [0, 2π).
3. At an exact reversal (`|delta| = π`) the arc goes counter-clockwise (positive), a fixed tie rule so the result is deterministic and identical in a shader.

Speed is blended separately by lerp. Opposite winds give a speed that stays at the lerp of the two while the direction swings through 90° at `t = 0.5`. Open Design Question 2 asks whether a vector blend (which gives a calm moment at the reversal) would look better.

**Precipitation type.** Type is discrete, so it is never blended into a third kind (no "sleet"):
1. If both ends have intensity ≤ `kPrecipEpsilon` (1e-4, provisional), the type is `None`.
2. If only one end has precipitation, take that end's type. Its intensity fades in or out by the lerp.
3. If both have precipitation and the types differ, take the type of the end nearer in time: `prev` while `t < 0.5`, `curr` from `t = 0.5`.
4. If the smoothed intensity is ≤ `kPrecipEpsilon`, the type is `None`.

### API shape

`SmoothedWeather` holds **const views** of the two states and `commitDay`. It owns no field data, so binding it costs nothing. It lives on whatever PU reads it. How a non-SimPU reader gets the two states is V-1's and the module's concern (Open Design Question 1); this class is pure logic and has no PU or module dependency.

`Sample` is a leaf function: no allocation, no locks, no logging, no profiling scope. The range variant is the same loop over a span. The per-read cost is 7 lerps, one angle blend and one small branch.

### Influence preview

**Input.** Feature 7's active and pending influence list, through a small read-only interface (`IInfluencePreviewSource`). For each influence it supplies: kind (storm, warmer, cooler, wetter, drier), a per-cell weight (its region and falloff, which Feature 7 already computes), `requestDay`, and `landedDay` (the commit day that first includes it in the committed state, or `kNotLanded`). Expired influences drop out of the list and out of the preview.

**Strength over time.** For each influence, `s` is the preview strength in [0, 1]:

```
pending (landedDay == kNotLanded):   s = clamp((today - requestDay + dayFraction) / kFadeInDays, 0, 1)      // fade in over ~1 day
landed:                              s = (1 - t)                                                             // t = the smoothing week fraction since landedDay
```

While the effect is pending, the preview grows to full strength in about one game day (`kFadeInDays` is the parameter `influence.preview_fade_days`, GDD `INFLUENCE_PREVIEW_FADE_DAYS` = 1, range 0.25–3; Feature 1 table row, latched at `Capture` and read by the preview through `Update`) and stays there. When the effect lands, the real change starts to arrive through the smoothing blend (`t` rising from 0 to 1 over the next 7 days). The preview fades out at the same rate, so the sum is continuous and nothing is counted twice. After 7 days it is gone. If the influence expires while pending, its preview is dropped at once.

**Look.** Each influence adds a small visual delta, scaled by `s × weight(cell)`, to the **read-out** of a sample, never to the simulation. The constants are visual only and **provisional**:

| Kind | Cloud cover | Precipitation intensity | Temperature |
|---|---|---|---|
| storm | + 0.5 | + 0.3 | none |
| wetter | + 0.25 | + 0.1 | none |
| drier | scaled by `1 − 0.5·s·w` | none | none |
| warmer | none | none | + 0.05 (tint) |
| cooler | none | none | − 0.05 (tint) |

Results are clamped to each field's range. Several influences on one cell add, then clamp.

**Storage.** `InfluencePreview` keeps, per influence, a sparse list of `(cell, weight)` built once when the influence appears (from Feature 7's weight function). Each game day, at most once, it recomputes the small per-cell delta only for the cells the previews touch (`touchedCells`), into three arrays sized N that are otherwise zero. With no active preview there is no work at all. A read adds `delta[cell]` to a `Sample`: `SampleWithPreview`.

**Isolation.** `InfluencePreview` takes only const references (`SmoothedWeather`, the source list). It has no pointer to `WeatherSimulation`, its state is not part of any save participant, and removing it changes no simulation output (AC 22, 23). After a load, pending influences (which Feature 7 saves) restart their preview at full strength with no fade-in, because the request time is not meaningful to the player any more.

### Determinism and saves

- `Sample` reads committed buffers only and writes nothing. It can run on any thread that holds the two views.
- It never runs inside `Run` (DFS-002); `Run` does not read `previous`.
- The rotation at `Commit` changes which buffer is which, not any value. A run with smoothing and one without are bit-identical in committed state (AC 21).
- Nothing here is saved. `previous` is rebuilt as a copy of `committed` on load.

### Instrumentation

- Metrics: one gauge, `cow.weather.preview_active` (influence previews live), written on SimPU. No per-read telemetry (SD-PERF-7).
- Debug: the panel card shows `commitDay`, `t` and the live preview count. A debug toggle on the wind overlay can show smoothed or raw wind. No console name is added.
- Logging, channel `WTH`: Warning if `t` had to be clamped by more than a day (a late commit); Info when a preview is dropped because its influence expired while pending.

## Public API

```cpp
namespace CoW::Weather {

    struct SmoothedSample {
        float temperature, cloudCover, precipitationIntensity, windDirection, windSpeed, snowPack, waterToGround;
        PrecipitationType precipitationType;
        bool  SnowCover() const { return snowPack > kSnowCoverThreshold; }
    };

    constexpr float kCalmSpeed = 0.02f;      // provisional
    constexpr float kPrecipEpsilon = 1e-4f;  // provisional

    // Pure helpers, shared by the CPU path, the tests and the V-1 shader reference.
    float WeekFraction(uint64_t day, float dayFraction, uint64_t commitDay);          // clamp((day - commitDay + dayFraction)/7, 0, 1)
    float BlendAngle(float prev, float curr, float t);                                // shortest arc, ccw tie at pi, result in [0, 2pi)
    PrecipitationType BlendPrecipType(PrecipitationType prev, float prevIntensity,
                                      PrecipitationType curr, float currIntensity, float t);

    class SmoothedWeather {
    public:
        // Const views. prev == curr is valid (first commit, after load). No ownership, no copy.
        void Bind(const WeatherSnapshot& previous, const WeatherSnapshot& current, uint64_t commitDay);
        bool IsBound() const;

        float WeekFraction(uint64_t day, float dayFraction) const;
        SmoothedSample Sample(uint32_t cell, float t) const;
        // Bulk read for tools and bulk consumers. out holds count entries, owned by the caller.
        void SampleRange(uint32_t firstCell, uint32_t count, float t, SmoothedSample* out) const;
    };

    // Read-only view of Feature 7's active and pending influence list.
    // Implemented by WeatherModule over Feature 7's InfluenceBook (amended 2026-11-17, row 6d):
    // requestDay = InfluenceInfo::addedDay, landedDay = InfluenceInfo::landedDay (kNotLanded until the Commit that first includes it).
    class IInfluencePreviewSource {
    public:
        virtual ~IInfluencePreviewSource() = default;
        virtual uint32_t GetCount() const = 0;
        virtual InfluencePreviewInfo Get(uint32_t index) const = 0;     // kind, requestDay, landedDay, sparse weights
    };

    class InfluencePreview {
    public:
        void Update(const IInfluencePreviewSource& source, uint64_t day, float t);   // at most once per game day; no-op with no previews
        SmoothedSample SampleWithPreview(const SmoothedWeather& smoothed, uint32_t cell, float t) const;
        uint32_t GetActiveCount() const;
        void Clear();                                                              // load, world change
    };
}
```

`WeatherModule` gains `GetSmoothed()` (const) beside `GetSnapshot()`.

## Acceptance Criteria

### Blend position and buffers
1. `WeekFraction` is 0 at the commit day, 3.5/7 at day +3 with fraction 0.5, and clamps to 1 at day +7 and beyond, and to 0 before the commit.
2. After two commits, `previous` holds the first committed state and `committed` the second, bit for bit, and no field data was copied (the rotation swaps ownership only).
3. Both views stay valid and unchanged until the next `Commit`.
4. At the first commit and after a load, `previous` equals `committed`, so every `Sample` equals the committed value for any `t`.

### Scalar fields
5. `Sample(c, 0)` equals the previous state and `Sample(c, 1)` equals the committed state, for all seven smoothed fields.
6. At `t = 0.25` each scalar field is exactly `lerp` of the two ends.
7. No smoothed value ever leaves the field's range, over randomised pairs of in-range states.
8. At `t = 0` right after a commit, a sample equals what the previous tick showed at `t = 1` (no jump at the commit).
9. No extrapolation: a field that is rising for three commits never exceeds the larger end at any `t`.

### Wind
10. Direction blends the short way round: from 350° to 10° at `t = 0.5` gives 0°, not 180°.
11. An exact reversal blends counter-clockwise and gives the same result on every call.
12. With either end calm (speed < `kCalmSpeed`), the direction is the other end's for every `t`.
13. Results are in [0, 2π), including across the 0 / 2π wrap.
14. Speed is the lerp of the two speeds, independent of direction.

### Precipitation type
15. Neither end precipitating gives `None`. One end precipitating gives that end's type for every `t`.
16. Both precipitating with different types gives `prev`'s type below `t = 0.5` and `curr`'s from 0.5.
17. A smoothed intensity ≤ `kPrecipEpsilon` gives `None`, whatever the ends' types.
18. The output is always one of `None`, `Rain` or `Snow` (no third value is possible).
19. `SnowCover()` follows the **smoothed** snow pack against 0.1.

### Isolation and cost
20. `Sample` and `SampleRange` are const, allocate nothing and take no lock (checked by an allocation-counting test and by the signature). `SampleRange` equals a loop of `Sample`.
21. Committed fields after 104 weeks are bit-identical with a reader sampling every day and with no reader at all, and identical to a build with `previous` never read.
22. No full-grid work without a reader: a test with a counting hook shows `Update` does nothing, and nothing runs per day, when there is no active preview and no read.
23. Nothing in the preview or the smoothing is serialised: a `WeatherSaveParticipant` round trip is byte-identical with previews active and without.

### Influence preview
24. A pending influence's preview strength rises from 0 to 1 over `kFadeInDays` and then holds at 1.
25. At landing (`landedDay` set), `s = 1 − t`, so at `t = 0` the preview is full strength and the sum of the preview and the real blended-in change is continuous across the commit. At `t = 1` the preview is exactly gone.
26. Looks, on a hand-built grid:
    - storm and wetter raise cloud cover and precipitation only inside the influence's weight;
    - drier lowers cloud cover and never below 0;
    - warmer and cooler change temperature only;
    - weights scale the delta.
27. Several overlapping influences add, then clamp to each field's range.
28. An influence that expires while pending drops its preview at once and logs once.
29. After `Clear()` or a load, pending previews appear at full strength with no fade-in.
30. With no active influences, `Update` does no work and `SampleWithPreview` equals `Sample`.
31. Removing the preview changes no simulation output; the preview class holds no non-const path to `WeatherSimulation` (checked at compile time by its signature).

## Files Touched

| File | Change |
|---|---|
| `Cluiche/CoW/Shared/Weather/SmoothedWeather.h/.cpp` | New: `SmoothedSample`, helpers, `SmoothedWeather` |
| `Cluiche/CoW/Shared/Weather/InfluencePreview.h/.cpp` | New: `IInfluencePreviewSource`, `InfluencePreview` |
| `Cluiche/CoW/Shared/Weather/WeatherSimulation.h/.cpp` | `previous` buffer, three-buffer rotation in `Commit`, `commitDay`, `GetPreviousSnapshot` (Features 2 and 3's file) |
| `Cluiche/CoW/Modules/WeatherModule.h/.cpp` | `GetSmoothed()`, bind at `Commit`, `Clear` previews on load and world change, one gauge |
| `Cluiche/CoW/Modules/WeatherDebugDomain.cpp` | Panel rows (`commitDay`, `t`, preview count), raw/smoothed wind toggle |
| `Cluiche/CoW/Shared/Weather/Testing/SmoothingTestStates.h` | New: hand-built state pairs and a fake influence source (test-utilities rule) |
| `Tests/GoogleTests/CoW/Weather/TestSmoothedWeather.cpp`, `TestInfluencePreview.cpp` | New: ACs 1–31 |
| `CoW.vcxproj` (+ filters), `GoogleTests.vcxproj` (+ filters) | Via `dia docs vcxproj-add` |

No Dia files expected, so no AD-001 sign-off.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-002 | ProcessingUnit/Phase/Module architecture; no singletons | Pure classes bound to views by the module. No globals |
| PD-004 | No STL containers in public APIs | Raw pointer plus count for bulk reads, `DynamicArrayC` inside the preview |
| AD-002 | Four-tier dependency model | CoW-Shared pure logic |
| AD-003 | GDD owns design intent | The GDD is locked (CHANGE-047). Preview strengths are provisional; the fade-in time is the GDD's |
| DFS-002 | `Run` reads only the captured working copy | Smoothing never runs inside `Run` and `Run` never reads `previous` (AC 21) |
| SD-PERF-7 | Telemetry cost budgets | One gauge, written on SimPU. No per-read or per-cell telemetry |

## GDD differences

1. **Blend.** The GDD's `lerp(previous_committed, current_committed, days_since_last_commit / 7)` is kept. Here the blend is computed on demand per read, not as a standing presentation pass, and a sub-day fraction is allowed.
2. **Wind.** The GDD says shortest-arc slerp. Here the tie at an exact reversal and the calm-end rule are defined.
3. **Precipitation type, snow cover and the field list** are not specified by the GDD. The discrete-type rule and the derived snow-cover rule are new.
4. **Preview hand-over (nearly resolved).** The locked GDD §20 says the preview "fades in over about a day, then hands over to the real CA effect once that reaches the blended state". Here the preview fades **in** over `preview_fade_days` (1), holds while pending, and fades **out over the 7 days after landing** while the real effect blends in, so there is no jump at the commit and no double count. That is the GDD's hand-over made exact.
5. **Preview strengths** (cloud, rain and tint amounts) are new, provisional and visual only. The GDD gives only the fade-in time.
6. **Saves.** The GDD says the preview is visual only. Here it is also explicitly not saved, with a defined restart after load.

## Amendments owed elsewhere

**A4 items applied 2026-11-17 (row 6d of `a4-weather.plan.md`):**

- **A4 system spec:** done. The Features table row 9 links this spec; the State section notes the third (`previous`) buffer and three copies of the ten-array `WeatherFields`; the output section gains `GetSmoothed()`.
- **A4 Feature 3:** done. `Commit` rotates three buffers and records `commitDay`; `WeatherSnapshot` views of both stay valid until the next commit; `previous` is rebuilt from `committed` on load (Feature 4 saves nothing new).
- **A4 Feature 7:** done. `InfluenceInfo` carries `addedDay` (= `requestDay`) and `landedDay`; the module implements `IInfluencePreviewSource`; `addedDay` is already saved (AC 29 covers a missing one).
- **A4 Feature 5:** done. Panel rows and the raw/smoothed wind toggle.

**Still owed outside the A4 files:**

- **V-1:** consumes `previous` and `committed` (as two textures or two snapshot views), uses the same blend rules and constants, and gets `t` from the game clock. Needs its own spec amendment and a shader reference test against `BlendAngle` and `BlendPrecipType`.
- **A4/A5 gameplay read API (A4 Open Design Question 4, PARK-030):** when decided, it can build on `Sample`.
- **GDD:** the fold-back list above.

## Test Plan

TDD, in this order, on hand-built state pairs (`SmoothingTestStates.h`):
1. `WeekFraction` and the buffer rotation.
2. Scalar lerp, ranges and no-jump continuity.
3. `BlendAngle`, including wrap, tie and calm ends.
4. `BlendPrecipType`.
5. Allocation, locking and isolation checks (AC 20–23).
6. The influence preview: fade-in, hand-over, looks, clamps, expiry, clear.

Scoped `--filter="*Smooth*:*InfluencePreview*"` while iterating and one full `--shards 2` run at the end. The exhaustive gap pass is a plan task. No mutation checks.

## Open Design Questions

1. **How does a reader off SimPU get the two states?** V-1 may render on another PU, and cross-PU reads must be streams, not shared buffers. Options: a FrameStream carrying both states' smoothed fields (two copies of about 8 floats per cell, about 430 KB at Large each commit), or the sim side uploading two textures at each commit and V-1 blending in the shader with `t` from the game clock. This spec assumes the class is pure and the transport is V-1's. The choice affects the cost of a commit and is open.
2. **Shortest arc or vector blend for wind reversals?** Shortest arc swings the direction through 90° while the speed stays high, which can look like a sudden gale turning on the spot. Blending the wind vectors instead gives a natural calm at the reversal, but the direction and speed then no longer match the CA's two states in between. The GDD says slerp, so this spec keeps it. Decide against the first V-1 render.
3. **Are the preview strengths honest?** Previews use fixed visual constants, not the real effect size. A storm preview of +0.5 cloud may be much more or less than what the CA delivers a week later, so the hand-over can feel like a surprise. The alternative is to scale the preview from Feature 7's own strength and falloff. That needs Feature 7 to expose the expected size per kind; unsettled until Feature 7 is written.

## World-sim review amendment (2026-11-18)

User decisions from the world-sim review. Each item **supersedes** any contradicting text in this spec. Sources: [summary](../../../../../research/worldsim_data_driven/summary.md) §0, §0.1; [parameter-files](../../../dia/systems/diaattribute/parameter-files.md); [biome-registry](../s1-worldgeneration/biome-registry.md); [worldsim-kit](../worldstep/worldsim-kit.md); [grid-search-helpers](../../../dia/systems/diapathfinding/grid-search-helpers.md); [blackboard-scopes](../../../dia/systems/diablackboard/blackboard-scopes.md).

1. **Preview kinds (system item 14).** The visual-only influence preview reads the influence kind catalogue (Feature 7), so a new kind previews with no code change. Supersedes any hard-coded preview kinds.
2. **World summaries (item 18).** Smoothing never alters the world-blackboard summaries; they carry committed-week values. Gameplay Open Q (Open Design Question 4 of the system spec) is unchanged except that these summaries are the only gameplay-facing A4 values published.
3. **Parameters (item 1).** `INFLUENCE_PREVIEW_FADE_DAYS` is a sidecar row; the Parameters text about the descriptor table is superseded.
4. **Naming (item 9).** Any schema key referenced here is `cow.weather.layers/<field>`.

## Status

**Status:** Approved (2026-11-17, user, after the locked-GDD alignment). Amended 2026-11-17 (aligned to locked GDD, CHANGE-047; row 6d). The GDD is locked. User decisions encoded: the CA stays weekly; readers see a blend of the last two committed weekly states by day of the week; never extrapolates, never shows a state the CA did not produce, accepts a one-week lag; computed on demand per read by a pure `Sample` (and range variant), no per-frame or per-day grid pass, V-1 may blend from two textures; fields, wind direction and discrete precipitation type rules defined; a visual-only influence preview that fades in over about a day and hands over at N+k, never feeding back and never saved; smoothing never affects simulation determinism or saves; the gameplay read API is not built here. Approval waits on Features 3 and 7.

Plan: [weather-smoothing.plan.md](weather-smoothing.plan.md) (written 2026-11-17, not started).

**Amended 2026-11-18 (world-sim review).** Status stays `Approved`. Amended 2026-11-18 (world-sim review decisions; user-approved designs in the specs referenced below).

## Draoi layer amendment (2026-11-21)

The pure-simulation part of this feature moves into `DraoiHexWorldWeather` with the rest of the A4 sim; the stage adapter, commands, save participant, debug wiring and content data stay in CoW. The split follows the "Draoi layer amendment" in @docs/specs/applications/cow/systems/a4-weather/a4-weather.md. The detailed section-by-section split is written when this feature is planned (#6 in the build order), not now.
