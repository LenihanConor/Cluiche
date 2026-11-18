# Feature Spec: A4 Weather Influence API

**Parent:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.md (Feature 7)
**Status:** Draft (2026-11-17)
**Design source:** GDD A4 TechnicalSpec §18 (Influence API, DECISION-131), TuningParameters §10a, WeatherSystem decision 7. The GDD draft was reopened `[~]` under CHANGE-047 and is **not locked**. Per AD-003 this spec cannot be Approved until the GDD session re-locks A4. User decisions of 2026-11-17 win where the draft differs (see "GDD differences").
**Depends on:** A4 Feature 2 (pipeline passes, forcing hooks A–D, `Capture`/`Run`/`Commit`); A4 Feature 3 (`WeatherModule`, `WeatherStage`, mutation queue, sim-hosted commands, `SetHydrologyProvider`); A4 Feature 4 (save record, pending blob). Soft: A5 Feature 3 (translates the ground-water part).
**Consumed by:** A4 Feature 5 (`force storm` routes here), A4 Feature 9 (Smoothing: reads the active/pending list for its preview), A5 Feature 3 (`extraWater` and sink translation), A1 (`water_dragon_emission` moves onto this path), future dragon abilities (not designed; see the backlog review row).

## Summary

A public, deterministic way for **any CoW system** to push the weather. Five kinds: **storm, warmer, cooler, wetter, drier**. Each request names its source, a region, a strength, a lifetime in weeks and a falloff.

- **Storm** is an **impulse**. In its first step it lowers pressure and adds moisture in its region. After that the CA carries the moisture with the wind like any natural storm. It is not pinned.
- **Warmer, cooler, wetter, drier** are **sustained**. They are applied every week of their lifetime at a fixed region.
- Overlapping influences **sum**. A per-field cap limits the total change per cell per week. A hard cap limits how many influences are live. Extra requests are **rejected with a reason**, never silently dropped.
- The API is a C++ interface on `WeatherModule`, reached by `ModuleRef`. Debug commands wrap it.
- Influences are captured at the step's `Capture`, so a request takes effect at the next kick and lands at apply key N+k. They are saved with the game.

This feature generalises the CA forcing hooks of Feature 2. It reuses the `Capture` latch, the fixed hook points and the `Commit` aging. It adds no parallel path through the weather code.

## Goals

1. One validated entry point (`AddInfluence`) that returns an id or a reason.
2. Per-cell evaluation that is a pure function of the latched influence list. Bit-identical at any range count, and deferred or synchronous.
3. Storm as a one-step impulse, the four others as sustained fields, with falloff over a circle region.
4. Overlap sums in ascending id order, with per-field caps and a hard active cap.
5. Influences are saved and reload deterministically, including a save taken mid-step.
6. A5 can translate the ground-water part of wetter and drier from a defined per-cell value.
7. Feature 9 can read the active and pending list. Feature 7 renders nothing.

## Non-Goals

- **Dragon abilities and any dragon content.** Enabled by this API, not designed. See the backlog row "Review the A4 weather influence API for dragon abilities" in `docs/BACKLOG.md` (Loose Ends).
- **Rendering or previewing influences.** Feature 9 does the immediate visual preview. It only reads the list this feature exposes.
- **Climate cycles** (Feature 8). They are separate deterministic functions of the game day and are not influences.
- **Ground-water physics.** A5 owns water on the ground. This feature only produces a signed per-cell water weight.
- **Forecasting.** Nothing reads next week's state.
- **A Dia API.** The API is CoW-only. Lifting it to Dia is not planned (backlog row).
- **Ownership rules between systems** (who may remove whose influence, per-source quotas). See Open Design Question 3.

## Design

### Influence record

```
InfluenceDesc (what a caller submits)
  kind          Storm | Warmer | Cooler | Wetter | Drier
  source        StringCRC system id, uint32_t entity (0 = none)   who asked; debug and attribution only
  region        CellSet { sorted cell indices }  OR  Circle { centre cell, radius in hex steps >= 0 }
  strength      float, (0, max_strength]
  lifetimeWeeks uint16, [1, max_lifetime_weeks]
  falloff       None | Linear | Smooth        Circle only; a CellSet is always None

InfluenceInfo (what the book holds and ListInfluences returns)
  id            uint32, assigned by A4 at AddInfluence, strictly increasing, never reused in a world
  desc          the InfluenceDesc, with a CellSet stored sorted and de-duplicated
  addedDay      uint32, the game day of the call (informational, for debug and history)
  weeksApplied  uint16, completed steps that included it
  inFlight      bool, latched into the currently captured step
  state         Pending (weeksApplied == 0 and not in flight) | InFlight | Active
```

`source` is a `StringCRC` plus an optional entity id. It is saved as numbers. Names are resolved only for debug text. It never affects the simulation.

### The influence book

`InfluenceBook` (pure, SimPU only, `Cluiche/CoW/Shared/Weather/`) is the authoritative list. `WeatherModule` owns one. It is **not** inside `WeatherSimulation::Run`'s reach. `Run` only sees an immutable `InfluenceSnapshot` latched by `Capture`.

- `Add(desc, rules)` validates, assigns the next id and appends. It returns `InfluenceResult{ id, reason }`.
- `Remove(id)` deletes the entry at once.
- `Latch()` (called inside `WeatherStage::Capture`) marks every live entry `inFlight` and returns a snapshot copy, sorted by ascending id, with a `firesImpulse` flag per storm (`weeksApplied == 0`).
- `Age()` (called inside `WeatherStage::Commit`, after `sim.Commit`) increments `weeksApplied` on every `inFlight` entry, clears `inFlight`, and deletes entries with `weeksApplied >= lifetimeWeeks`.
- `Clear(reason)` empties the book (new world, preset, state replace by import of another world).

Why a book outside the simulation: `AddInfluence` must answer **now** with an id or a reason, while the working set may be mid-`Run`. This follows Feature 3's rule that every mutation is latched at `Capture`, and it keeps `Run` a pure function of what `Capture` latched (DFS-002).

**Weeks are steps, not wall time.** `lifetimeWeeks` counts applied steps. A retried or discarded step does not age anything.

### Evaluation per cell per week

Pass 0, **Influence**, is added to the pipeline (Feature 2). It runs first in `Run`, as a parallel pass over cell ranges. For each cell it loops the snapshot in **ascending id order** and builds four sums:

| Sum | Contributors (weight `w` = `strength × falloffWeight`) | Per-week effect at weight 1 |
|---|---|---|
| `dPressure` | Storm, first step only: `-w × storm_pressure` | pressure drop |
| `dTemperature` | Warmer `+w × temp`, Cooler `-w × temp` | temperature change |
| `dMoisture` | Storm, first step only: `+w × storm_moisture`; Wetter `+w × moisture`; Drier `-w × moisture` | moisture change |
| `waterWeight` | Wetter `+w`, Drier `-w`, in strength units | handed to A5 |

Then each sum is clamped to its cap (see Caps). Each cell writes only its own scratch entries. The float sum order is the snapshot order, so the result depends on neither range count nor range order.

**Region weight.**
- `CellSet`: `w = strength` for member cells (binary search on the sorted list), `0` elsewhere.
- `Circle`: `d` is the hex-step distance from the centre. `d > radius` gives 0. Otherwise `t = d / (radius + 1)` and the weight is `strength × f(t)`, where `f` is `1` for `None`, `1 − t` for `Linear`, and `1 − t²(3 − 2t)` for `Smooth`. Radius 0 is the single centre cell at weight `strength`. The edge ring keeps a small non-zero weight, so a radius of R really affects R steps. Only `+ − ×` are used, so the result is bit-exact. `sinf`/`cosf` are not.
- Hex distance is computed from cube coordinates of the flat-top odd-q layout. If `HexTopology` exposes a distance, use it (check in plan task 0). Otherwise A4 owns a pure helper and Dia is not edited.

**Skip.** With an empty snapshot pass 0 is skipped and no scratch is read. A run with no influences is bit-identical to a run built before this feature (AC 24).

### Where the deltas land (hooks)

The existing forcing hooks are at fixed points because the pipeline would otherwise erase a change before it mattered (pass 2 recomputes pressure from temperature, and pass 1 relaxes temperature toward its seasonal target). Influence deltas use the same idea:

| Delta | Applied | Why there |
|---|---|---|
| `dTemperature` | right after pass 1 (Solar) | pass 1 relaxes toward the seasonal baseline, so a delta before it would be partly erased. Later transport moves it |
| `dPressure` | right after pass 2 (Pressure), the hook A point | pass 2 overwrites pressure, so this is the earliest point pass 3 (Wind) sees it |
| `dMoisture` | right after pass 4 (Evaporation), the hook C point | transport (passes 5a/5b) then carries it with the wind |

Each result is clamped to its field range after the add. **Holds win.** A `PressureHold` or `MoistureHold` forcing writes its value after the influence delta at the same hook, so a debug hold overrides an influence on the same cell.

### Storm impulse

A storm acts in **one** step: the step that first latches it (`weeksApplied == 0`).

**Carried pressure anomaly (user decision 2026-11-17).** A4 gains a per-cell field `pressureAnomaly`. It is signed (0 = none), saved, and clamped to ±`anomaly_clamp`.

- **Seeding.** In the storm's first step it adds a negative anomaly over its region: `storm_pressure_seed × weight`, subject to the pressure cap. It also adds its moisture seed.
- **Every week:**
  - Pass 2 sets `pressure = computed-from-temperature + pressureAnomaly`, then clamps it. The low therefore steers the wind (pass 3) and is seen by `find storms`.
  - Transport carries the anomaly with the wind, using the same gather and the same upwind edge rule as moisture.
  - The anomaly then decays: `pressureAnomaly *= pressure_anomaly_decay`.

The storm is a real, moving low for a few weeks (a half-life of about 2 weeks at the provisional 0.7), and its moisture rains out along the way. It is **not** pinned to its region. Natural storms may use the same field later; that is not required now.

The storm record stays in the book for its `lifetimeWeeks` (listed, counted against the active cap, saved). It seeds nothing after its first step, because what it started is now carried by the CA.

### Sustained kinds

Warmer, cooler, wetter and drier apply their delta **every step** of their lifetime at a fixed region. Wetter adds moisture; drier removes it. Warmer and cooler change temperature. A step with weight 0 in a cell changes nothing there.

### Caps and rejection reasons

All numbers are **provisional** (marked P). They live in `weather_parameters.json` under a new `influence` block (Feature 1 amendment owed), are latched at `Capture`, and are validated on load.

| Parameter | Default | Range | Meaning |
|---|---|---|---|
| `max_strength` | 1.0 P | 0.5–2.0 | Upper bound of one influence's `strength` |
| `max_lifetime_weeks` | 13 P | 1–52 | Longest lifetime of one influence |
| `max_active` | 64 P | 8–256 | Live influences at once (pending + in flight + active) |
| `storm_pressure` | 0.2 P | 0.05–0.4 | Pressure drop per weight 1 |
| `storm_moisture` | 0.2 P | 0.05–0.4 | Moisture added per weight 1 |
| `temp` | 0.1 P | 0.02–0.2 | Temperature change per weight 1 |
| `moisture` | 0.15 P | 0.05–0.3 | Moisture change per weight 1 |
| `cap_temperature` | 0.2 P | 0–1 | Max `|dTemperature|` per cell per week |
| `cap_moisture` | 0.3 P | 0–1 | Max `|dMoisture|` per cell per week |
| `cap_pressure` | 0.4 P | 0–1 | Max pressure drop per cell per week |
| `cap_water` | 1.0 P | 0–2 | Max `|waterWeight|` per cell per week |
| `max_radius` | 32 P | 0–64 | Largest Circle radius |
| `max_set_cells` | 8192 P | 64–65536 | Total cells held by all CellSet regions |
| `default_falloff` | Smooth P | enum | Used by commands when the caller names none |

The first seven rows are the GDD TuningParameters §10a defaults. The four caps and the two storage limits are new, chosen to be the right order of magnitude, not tuned.

Rejection is an explicit **reason**, never a silent drop:

| Reason | When |
|---|---|
| `NotReady` | No world is initialised and spun up (`IsReady()` false) |
| `BadKind` | Unknown kind value |
| `BadStrength` | Not finite, `<= 0`, or `> max_strength` |
| `BadLifetime` | `0` or `> max_lifetime_weeks` |
| `EmptyRegion` | A cell set with no cells |
| `CellOutOfRange` | A cell index (set member or centre) is outside the grid |
| `RadiusTooLarge` | Circle radius `> max_radius` |
| `FalloffOnCellSet` | A cell set with a falloff other than `None` |
| `RegionStorageFull` | The cell set would take the total over `max_set_cells` |
| `ActiveCapReached` | The book already holds `max_active` entries |

Validation is all-or-nothing. A rejected call changes nothing, not even the id counter. Each rejection logs one Warning with the reason and the source, and counts in a metric.

### Capture and apply timing

- `AddInfluence` runs on SimPU, between kicks or during a step. It assigns the id and the entry is `Pending`.
- The next `WeatherStage::Capture` latches the book. The influence first affects **that** step's `Run`, and the result is visible at the step's apply key N+k (DFS-002, DFS-003).
- A removal is immediate in the book. A step already captured keeps the influence it latched, so the removed influence still acts in that one step. The next `Capture` no longer sees it.
- `Commit` ages the book. Expiry happens at the commit of the last applied week.
- A step that is discarded (world regenerated, Feature 3) ages nothing. A new world revision, a preset load and a state import over another world's dimensions clear the book with one Info log (Open Design Question 4).
- `AddInfluence` is **SimPU-only** (asserted). Callers on other PUs go through the sim-hosted command.

### A5 hand-off

Pass 0 also writes `waterWeight` (after its cap) into `WeatherHandoff::influenceWater`, a transient array like `glacierInput`. It is **not saved**.

- **Sign.** Positive is wetter, negative is drier. Units are strength units, dimensionless.
- **A5's translation** (A5 Feature 3, owed): wetter → `WaterInput.extraWater` (positive), drier → a contribution to `WaterSinks.draw` (the magnitude). The conversion to cell-depth units is A5's own parameter (the GDD calls it `INFLUENCE_GROUND_WATER`). A4 does not know soil or ground water.
- A5 reads `influenceWater` from A4's working copy inside the same `Run`, the same way it reads `waterToGround`.
- **A4's own part still applies.** Wetter and drier change air moisture whether or not A5 exists.
- **Without A5** (`HydrologyInputs.present == false`): `influenceWater` is still computed and exposed, nothing consumes it, and A4 applies only its own fields. Nothing is Degraded.

### Save and load (Feature 4 participant)

The weather record gains an `influences` object. It is part of the committed save, not the pending blob:

```json
"influences": {
  "next_id": 17,
  "items": [
    { "id": 12, "source": 3042951201, "source_entity": 0, "kind": 3, "added_day": 4410,
      "region": { "mode": "circle", "centre": 2114, "radius": 4, "falloff": 2 },
      "strength": 0.6, "lifetime_weeks": 8, "weeks_applied": 2, "in_flight": false }
  ]
}
```

- A cell-set region writes `{"mode":"cells","cells":[...]}`.
- `Validate` (consistency only) rejects: items over `max_active`, an id that is not strictly ascending or is `>= next_id`, an unknown kind or falloff, `weeks_applied >= lifetime_weeks`, a lifetime or strength out of range, a cell list that is unsorted, duplicated or has an index `>= cell_count`, a centre `>= cell_count`, and a total cell count over `max_set_cells`.
- A dimension mismatch against the loaded world discards the influences along with the rest of the weather state (Feature 4, step 3).
- A save taken mid-step reloads identically: `in_flight` entries are restored as in flight, the pending blob's working set already includes their effect, and the apply-time `Commit` ages them. No influence needs to be in the pending blob.
- Size at the caps: 64 items, at most 8,192 set cells in total, about 0.1 MB of JSON.

### Debug and commands

One new sim-hosted name, **`cow.weather.influence`**, with mandatory help (description non-empty, at most 256 characters). It follows Feature 5's conventions: a JSON object with an `action` field (Feature 5 uses `action` throughout, so this feature does too, not `op`), errors name the field, nothing is queued on error.

| `action` | Arguments | Behaviour |
|---|---|---|
| `add` | `kind`, `source` (name, default `"debug"`), `strength`, `weeks`, `falloff` (optional), and one region: `cells` (list) or `centre` + `radius` | Calls `AddInfluence`. The reply names the id and says it takes effect at the next weekly step, or names the rejection reason |
| `remove` | `id` | Calls `RemoveInfluence` |
| `list` | none | Calls `ListInfluences` |
| `clear` | `source` (optional name) | Removes every influence, or those of one source |

- The existing `cow.weather.force` action `storm` becomes a **thin call** into `AddInfluence` with source `"debug.force"` (Feature 5 amendment owed). `intensity` maps to `strength` (scaled by `max_strength`), `cell` is the centre, and a new optional `radius` (default 3 P) is added. `high_pressure` and `rain` keep using the Feature 2 hooks.
- Weather's sim-hosted name count becomes **8** (Feature 5 had 7). The per-PU cap is 192, and the earlier 64 figure is stale (system spec Gap-pass amendment 3).
- Metrics, registered with `MetricInfo` and written once per applied step on SimPU (SD-PERF-7): `cow.weather.influences_active` (gauge) and `cow.weather.influences_rejected` (counter).
- Logs on `WTH`: Info on add, remove and expiry (id, kind, source), Warning on a rejection (reason, source), Info once per world when the book is cleared.
- `DIA_TRACE_ZONE` on `AddInfluence`, the latch and the pass-0 evaluation; `DIA_PROFILE_SCOPE` on pass 0.

## Public API

New in `Cluiche/CoW/Shared/Weather/` (`CoW::Weather`; Dia containers, no STL in public APIs).

```cpp
namespace CoW::Weather {

    enum class InfluenceKind : uint8_t { Storm, Warmer, Cooler, Wetter, Drier };
    enum class InfluenceFalloff : uint8_t { None, Linear, Smooth };
    enum class InfluenceRejectReason : uint8_t {
        None, NotReady, BadKind, BadStrength, BadLifetime, EmptyRegion, CellOutOfRange,
        RadiusTooLarge, FalloffOnCellSet, RegionStorageFull, ActiveCapReached
    };
    enum class InfluenceState : uint8_t { Pending, InFlight, Active };

    using InfluenceId = uint32_t;                       // 0 = none; ids start at 1

    struct InfluenceSource { StringCRC system; uint32_t entity = 0; };

    struct InfluenceRegion {
        bool                  isCircle = true;
        uint32_t              centre = 0;               // circle
        uint16_t              radius = 0;               // hex steps
        DynamicArrayC<uint32_t> cells;                  // cell set; sorted and de-duplicated on Add
    };

    struct InfluenceDesc {
        InfluenceKind     kind;
        InfluenceSource   source;
        InfluenceRegion   region;
        float             strength;
        uint16_t          lifetimeWeeks;
        InfluenceFalloff  falloff = InfluenceFalloff::Smooth;
    };

    struct InfluenceResult { InfluenceId id; InfluenceRejectReason reason; bool Ok() const; };

    struct InfluenceInfo {
        InfluenceId id; InfluenceDesc desc; uint32_t addedDay;
        uint16_t weeksApplied; uint16_t weeksRemaining; InfluenceState state;
    };

    // Pure helpers. Feature 9 and the tests use them.
    float InfluenceWeight(const InfluenceDesc& desc, uint32_t cell, const HexGridTables& grid);
    const char* InfluenceRejectReasonName(InfluenceRejectReason);

    // Pure, SimPU-only authoritative list (not touched by Run).
    class InfluenceBook {
    public:
        InfluenceResult Add(const InfluenceDesc& desc, uint32_t addedDay,
                            const InfluenceRules& rules, uint32_t cellCount);
        bool            Remove(InfluenceId id);
        uint32_t        RemoveBySource(StringCRC system);          // returns how many
        InfluenceSnapshot Latch();                                 // Capture: mark in flight, copy
        void            Age();                                     // Commit
        void            Clear();
        void            List(DynamicArrayC<InfluenceInfo>& out) const;
        uint32_t        Revision() const;                          // bumps on any change; Feature 9 polls it
    };
}

// On WeatherModule (SimPU; reached by ModuleRef)
class WeatherModule {
public:
    InfluenceResult AddInfluence(const InfluenceDesc& desc);       // id, or a reason; never throws, never drops silently
    bool            RemoveInfluence(InfluenceId id);               // false if unknown
    void            ListInfluences(DynamicArrayC<InfluenceInfo>& out) const;   // pending, in flight and active
    uint32_t        GetInfluenceRevision() const;
};

// Additions to WeatherSimulation (Feature 2's class)
//   Capture(..., const InfluenceSnapshot&)      latches the list; empty snapshot = pass 0 skipped
//   WeatherHandoff::influenceWater              signed per-cell water weight, A5 reads in the same Run
```

A Dia dependency is not needed. `HexGridTables` is Feature 2's precomputed grid.

## Acceptance Criteria

### Record validation
1. `AddInfluence` with valid input returns a non-zero id, and `ListInfluences` shows it as `Pending` with `weeksRemaining == lifetimeWeeks`.
2. Ids are strictly increasing, start at 1 in a world, and are never reused after removal or expiry.
3. Each rejection reason fires on its own trigger: unknown kind, strength `0`, negative, NaN, infinite or `> max_strength`, lifetime `0` or `> max_lifetime_weeks`, an empty cell set, a cell index or centre `>= cell_count`, a radius `> max_radius`, a falloff other than `None` on a cell set, a cell set past `max_set_cells`, and no initialised world.
4. A rejected call changes nothing: the list, the id counter and the metrics other than the reject counter are unchanged.
5. A cell set is stored sorted and de-duplicated. A repeated cell does not double its weight.
6. `RemoveInfluence` of an unknown or already-removed id returns false and changes nothing.

### Evaluation and falloff
7. A `Circle` influence affects exactly the cells within `radius` hex steps of the centre, on both odd and even columns, and nothing outside.
8. Falloff `None` gives weight `strength` everywhere in range. `Linear` and `Smooth` give `strength` at the centre, decrease monotonically with distance, and give a small positive weight at distance `radius`.
9. A `Circle` that overlaps the world edge affects only the in-grid cells, with no wrap.
10. Radius 0 affects exactly the centre cell.
11. A `CellSet` influence affects exactly its member cells at weight `strength`.

### Storm impulse
12. In its first step a storm lowers pressure and adds moisture in its region, by `storm_pressure` and `storm_moisture` times the weight, subject to the caps.
13. A storm seeds nothing in later steps, though it stays listed until its lifetime ends.
14. After the first step, both the added moisture and the `pressureAnomaly` move downwind by the normal transport gather (on uniform terrain with a steady wind, the anomaly's centre shifts at the moisture transport rate), and the storm region is not re-seeded.
14a. Each week the anomaly decays by exactly `pressure_anomaly_decay`, and pass 2's pressure equals the computed value plus the anomaly, clamped. A planted storm keeps a cell below the `find storms` threshold for more than one week (provisional constants: at least 2).
14b. `pressureAnomaly` stays within ±`anomaly_clamp`. It is saved and restored bit-identically, and it is 0 everywhere on a world that never had a storm, so that world's weather is byte-identical to the pre-Feature-7 pipeline.
15. With a hold forcing on the same cell, the hold's value wins.

### Sustained kinds
16. A warmer influence raises `temperature` and a cooler one lowers it, every step of the lifetime, at the same cells.
17. A wetter influence raises `moisture` and a drier one lowers it, every step, at the same cells.
18. The delta is applied at the documented hook (temperature after pass 1, moisture after pass 4, pressure after pass 2). A test proves the temperature delta is not erased by pass 1 relaxation and the pressure drop is visible to pass 3.
19. Fields are clamped to their ranges after the delta (a cooler on a 0.0 cell leaves 0.0; a wetter on a saturated cell stays at 1.0).
20. The influence ends after exactly `lifetimeWeeks` applied steps: it acts in each of them and in no later one.

### Overlap sum and cap
21. Two overlapping influences of the same kind sum their weights per cell. The same pair listed in either call order gives the same result, because evaluation order is ascending id, not call order of equal ids.
22. Warmer and cooler on the same cell sum with opposite signs. Wetter and drier likewise.
23. `|dTemperature|`, `|dMoisture|`, the pressure drop and `|waterWeight|` per cell per week never exceed `cap_temperature`, `cap_moisture`, `cap_pressure` and `cap_water`, however many influences overlap (a test stacks 64 on one cell).
24. With an empty snapshot, pass 0 is skipped and fields, totals and profile are bit-identical to a run without the feature.

### Active cap
25. The `max_active` + 1 request returns `ActiveCapReached`. Pending, in-flight and active entries all count. Nothing is dropped from the book.
26. Expiry and `RemoveInfluence` free a slot immediately for the next call.
27. If a parameter reload lowers `max_active` below the current count, existing influences stay and new ones are rejected until the count falls below it.

### Timing
28. An influence added between kicks is `Pending` until the next `Capture`, affects that step's `Run`, and is visible in committed state only after that step's `Commit` (apply key N+k).
29. An influence added while a step is in flight does not change that step. It affects the next one.
30. Removing an influence while a step is in flight leaves that step unchanged (the removed influence still acted once) and affects no later step.
31. A discarded step ages nothing and applies nothing (Feature 3 discard path).
32. A new world revision, a preset load and a state import with other dimensions clear the book, with one Info log each. A state import with matching dimensions leaves the book alone.
33. `GetInfluenceRevision()` changes on add, remove, latch, age and clear, and on nothing else.

### Determinism
34. Fields, totals and `influenceWater` are bit-identical at `SerialExecutor` and at 1, 3 and 17 ranges, with ranges run in reverse order, over 52 weeks with a mix of all five kinds, circle and cell-set regions and overlaps.
35. A deferred run (Capture at N, apply at N+k) equals a synchronous `Step` run bit for bit over 52 weeks, including influences added between kicks.
36. Poisoning the book, the descriptor and the cell-set source after `Capture` does not change `Run` (DFS-002).
37. No RNG is used. The same call sequence on the same world gives the same ids and fields, run twice.
38. `Run` allocates nothing in pass 0 (scratch and the snapshot pool are sized at `Initialise`).

### Save round trip
39. Save, load and continue gives committed fields, the book and `influenceWater` bit-identical to a run that never saved, at N = 1, 13 and 52 weeks, with pending, in-flight and active entries all present at the save.
40. A save taken mid-step (after `Run`, before `Commit`) restores the in-flight entries as in flight and ages them at the apply-time `Commit`. The result equals the uninterrupted run.
41. `Validate` rejects each defect listed under Save and load and mutates nothing.
42. A dimension mismatch discards the book with the rest of the weather state, with the existing Warning.
43. `next_id` is restored, so ids after a load continue the sequence.

### A5 hand-off
44. `influenceWater` is positive where only wetter acts, negative where only drier acts, and the signed sum where they overlap, capped at `cap_water`.
45. With a fake A5 provider (`present == true`), the influence list round-trips into a fake `WaterInput.extraWater`/sink contribution of the same sign and size, and A5 reads it from the working copy inside the same `Run`.
46. With `present == false`, the module is not Degraded, A4 still applies moisture, `influenceWater` is computed and ignored, and the fields equal the with-A5 run's A4 fields (A4 never reads A5's reply to an influence).

### Commands
47. `cow.weather.influence` registers with non-empty help and takes one name from the sim-hosted budget, and registration stays all-or-nothing with the other weather names.
48. `add`, `remove`, `list` and `clear` behave as in the table. `add` replies with the id and the "next weekly step" wording, or the rejection reason. A bad field returns an error naming it and queues nothing.
49. `clear` with `source` removes only that source's influences.
50. `cow.weather.force` with `action: "storm"` creates a Storm influence with source `"debug.force"`, centre `cell`, strength from `intensity`, and the default radius. It no longer writes a pressure or moisture hold.
51. Direct API calls and commands give identical results for identical requests (one execution path).

### Metrics and logs
52. `cow.weather.influences_active` and `cow.weather.influences_rejected` register with `MetricInfo` and are written only on SimPU, once per applied step.
53. Each rejection logs one Warning naming the reason and the source. Add, remove and expiry log Info.

## Files Touched

| File | Change |
|---|---|
| `Cluiche/CoW/Shared/Weather/WeatherInfluence.h/.cpp` | New: enums, `InfluenceDesc`, `InfluenceBook`, `InfluenceSnapshot`, `InfluenceWeight`, validation |
| `Cluiche/CoW/Shared/Weather/WeatherInfluencePass.cpp` | New: pass 0 evaluation, caps, `influenceWater` |
| `Cluiche/CoW/Shared/Weather/WeatherInfluenceCodec.h/.cpp` | New: book to and from `Json::Value`, `Validate` rules |
| `Cluiche/CoW/Modules/WeatherInfluenceCommands.h/.cpp` | New: `cow.weather.influence` over the API |
| `Cluiche/CoW/Shared/Weather/WeatherSimulation.h/.cpp` | Edited: `Capture` takes the snapshot; pass 0; hook points; `influenceWater` |
| `Cluiche/CoW/Shared/Weather/WeatherParameters.h/.cpp` | Edited: `influence` block, defaults, validation |
| `Cluiche/CoW/Modules/WeatherModule.h/.cpp`, `WeatherStage.cpp` | Edited: owns the book, public API, `Latch` in `Capture`, `Age` in `Commit`, clear on world change |
| `Assets/CoW/Global/Weather/weather_parameters.json`, `weather_schema.json` | Edited: `influence` block; metric schema |
| `Tests/GoogleTests/CoW/Weather/TestWeatherInfluence{Book,Eval,Timing,Determinism,Save,Commands,Handoff}.cpp` | New |
| `CoW.vcxproj` (+ filters), `GoogleTests.vcxproj` (+ filters) | Via `dia docs vcxproj-add` |

**No Dia files are added or edited, so no AD-001 sign-off is needed.** If `HexTopology` lacks a hex distance, A4 adds its own pure helper rather than editing Dia. Flag it then if the user prefers the Dia route.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-002 | Module architecture; no singletons | The API lives on `WeatherModule`, reached by `ModuleRef`. The book is module-owned. No `GetStatic` |
| PD-004 | No STL in public APIs | `DynamicArrayC`, POD records, `StringCRC` |
| AD-002 | Four-tier model | Book, evaluation and codec are CoW-Shared pure logic. The module and commands are in `Modules/` |
| AD-003 | GDD owns design intent | Draft until the GDD re-locks A4. Differences are listed below |
| DFS-002 | `Run` reads only the captured working copy | The book is outside `Run`. `Capture` latches a snapshot. AC 36 poisons the sources |
| DFS-003 | A late step stalls; the key never slips | An influence never retimes a step. It lands at the key of the step that latched it |
| DFS-005 | Saves drain and store pending results | The book is saved with the committed record. In-flight state restores through the pending blob rule (AC 40) |
| SD-PERF-7 | Telemetry cost budgets | Two metrics, written once per applied step on SimPU. No per-cell telemetry |
| Sim-hosted name cap | 192 per PU in code | One new name, registered with the other seven all-or-nothing |

## GDD differences

For the GDD owner. The GDD draft is not locked, and user decisions of 2026-11-17 win.

1. **Storm is an impulse, not sustained.** GDD §18 applies a storm's pressure drop and moisture add every week of its life. The user decision applies it once, then lets the CA carry it.
2. **Overlap rule.** GDD sums weights per kind and clamps the **weight** at `INFLUENCE_MAX_STRENGTH`. This spec sums per field and clamps the **resulting delta** per cell per week (`cap_*`). `max_strength` still bounds one influence.
3. **Application point.** GDD says "applied in step 0", before pass 1. Pass 2 recomputes pressure from temperature and pass 1 relaxes temperature, so a step-0 delta would be erased. Deltas land at hooks after the pass that owns each field (see Hooks). Pass 0 only evaluates.
4. **Order.** GDD orders by `(start_day, id)`. Ids are strictly increasing, so ascending id is the same order. `start_day` is kept as informational `addedDay`.
5. **Falloff curve.** GDD uses `distance / radius`, which gives the edge cells zero weight under Linear and Smooth. This spec uses `distance / (radius + 1)`, so every in-range cell is affected.
6. **Cap semantics and parameters.** The per-field caps, `max_radius` and `max_set_cells` are new. The GDD has only the strength cap.
7. **Result type and reasons.** The GDD says "rejects new ones with a logged reason". This spec returns the reason to the caller as a typed result and also logs it.
8. **Lifetime for storms.** The GDD gives storms a weekly effect over their lifetime. Here the lifetime only keeps the record listed. The carried anomaly gives the storm its duration.
9. **How the storm's low travels.** Pass 2 recomputes pressure from temperature, so a plain pressure drop would last one step. The user chose a carried `pressureAnomaly` field (transported and decaying). This was sent to the GDD session on 2026-11-17 for §18, with provisional PRESSURE_ANOMALY_DECAY 0.7, STORM_PRESSURE_SEED −0.3 and an anomaly clamp of ±0.5.
10. **A5 share.** Same split as the GDD. The water weight is a dimensionless signed value, and the conversion to ground-water units is A5's parameter.

## Amendments owed elsewhere

Not made by this spec. The owner edits these files.

- **F2 `ca-pipeline.md`:** pass 2 adds `pressureAnomaly`; the transport gather carries it like moisture; a decay step follows; new pass 0 (Influence) and `kPassCount` plus one; influence hooks after passes 1, 2 and 4; `Capture` takes an `InfluenceSnapshot`; `WeatherHandoff` gains `influenceWater`; hold-wins rule; scratch arrays and pool sized at `Initialise`. Open Design Question 2's "one-shot set value" is superseded by the storm impulse.
- **F1 `weather-data.md`:** the `pressureAnomaly` field (saved), the constants `pressure_anomaly_decay` (0.7), `storm_pressure_seed` (−0.3) and `anomaly_clamp` (0.5), all provisional; the `influence` parameter block, defaults, ranges and validation (the 20-constant count is unchanged; these are a separate block).
- **F3 `weather-module.md`:** the module owns `InfluenceBook`; `Capture` latches it and `Commit` ages it; public `AddInfluence`/`RemoveInfluence`/`ListInfluences`; clear on a new world revision. Its mutation queue is not used for influences, because the call must return an id at once.
- **F4 `weather-save.md`:** the `pressure_anomaly` array; the `influences` object in the record and its `Validate` rules; `RestoreCommitted` restores the book; the pending-blob text notes that in-flight state comes from the book.
- **F5 `weather-debug.md`:** 8 names, not 7 (`cow.weather.influence`); `force` `storm` routes through `AddInfluence` and no longer holds pressure and moisture (criteria 13 and 14 and the table row change, `weeks` for storm is bounded by `max_lifetime_weeks`); `info memory` reports the influence count; `find storms` detects pressure below the storm threshold, which a planted storm now shows for several weeks, because the anomaly carries the low.
- **F6 `presets-and-regression.md`:** `ApplyPreset` clears the influence book (its "same state whatever ran before" criterion 7). Its `coastal_storm` preset keeps using `PressureHold` and is otherwise unaffected.
- **A5 `water-pipeline.md` / A5 Feature 3:** read `influenceWater` from A4's working copy and translate it (wetter → `extraWater`, drier → `WaterSinks.draw`).
- **Parent `a4-weather.md`:** Features table row 7 points to this spec; Update pipeline lists pass 0.

## Test Plan

TDD per `.claude/skills/tdd.md`. Pure logic on hand-built hex terrain (`MakeCoastRange`), no S-1 code needed. Order:

1. `InfluenceBook` and validation (all reasons, ids, caps, sorting)
2. `InfluenceWeight` and falloff, hex distance on odd and even columns
3. Pass 0 and caps
4. The hook placement and storm impulse against the Feature 2 pipeline
5. Overlap and hold precedence
6. Determinism (range counts, reverse order, deferred vs synchronous, poison)
7. Save codec, `Validate`, the mid-step save
8. A5 hand-off with a fake provider
9. Commands and metrics through the fake sim-host

Scoped `--filter="*WeatherInfluence*"` while iterating, one full `--shards 2` run at the end. No mutation checks. Gap audit and edge pass is the last plan task. Observation scan at verify.

## Open Design Questions

1. ~~**What is a storm impulse, really, in this pipeline?**~~ **Resolved 2026-11-17 (user): carried pressure anomaly** (see Storm impulse). The original question follows for the record. Pressure is recomputed from temperature in pass 2 every week, so a one-step pressure drop only shapes the wind of that step. The moisture seed is what travels. Two things follow. First, `find storms` (pressure below 0.3) will see a storm for one week only. Second, the user decisions give storms a lifetime but make their effect one step, so the lifetime means nothing beyond how long the record stays listed and counts against the cap. This spec keeps the uniform record (a storm stays listed for its lifetime) and flags it. Alternatives: (a) the storm also seeds a temperature rise in its region, so pressure stays low for a few weeks by the normal rule and the low-pressure region really does travel with the heat transport; (b) a storm's lifetime is forced to 1 and the record disappears after its step. Decide after the first S-2.5 run with a planted storm. Affects AC 12–14 and the `find storms` expectations.
2. **What do strength units mean, and should an impulse scale with region size?** `strength` is dimensionless, and one unit of weight means a per-week delta set by `temp`, `moisture`, `storm_pressure` and `storm_moisture`. A larger region means more total moisture in the world, not a weaker storm per cell. Caps stop a stack of influences from blowing a cell up, but not a huge region of mild ones. The numbers are the GDD's untuned defaults. Tune against S-2.5 and consider whether dragon abilities want "total energy" semantics instead.
3. **Who may do what, later?** The API has no ownership rules: any caller can remove any id, and one system could fill the 64 slots. The record carries `source`, so per-source quotas, owner-only removal, or a cooldown can be added without changing the record. Dragon ability design should decide which. Deliberately not decided now (no dragon content).
4. **Should a regenerated world drop the influences?** This spec clears the book on a new world revision, because the cell indices belong to the old grid. A same-dimension regeneration (S-1 live editing) arguably should keep them, but they would then point at different terrain. Clearing is the safe choice. Revisit if S-1 editing and influences are ever used together.

## Status

**Status:** Draft (2026-11-17). Not Approvable until the GDD session re-locks A4 (AD-003).

User decisions of 2026-11-17 encoded here:
- Public, deterministic, CoW-only influence API with five kinds and the six fields (source, region as cell set or centre + radius, strength, lifetime, falloff).
- Storm is an impulse that the CA carries. Its low travels as a `pressureAnomaly` field that is transported with the wind and decays. The other four kinds are sustained at a fixed region.
- Overlaps sum, with a per-field cap per cell per week and a hard active cap (provisional 64). Excess requests are rejected with a reason.
- Captured at `Capture` (DFS-002), landing at N+k. Feature 9 reads the list for the preview and Feature 7 renders nothing.
- Saved with the game through Feature 4's participant.
- A5 applies the ground-water part. Feature 7 defines the record and per-cell water weight.
- C++ API on `WeatherModule` by `ModuleRef`, plus `cow.weather.influence` with help. `force storm` becomes a thin call.
- No RNG. Ascending id order. Bit-identical at any range count and deferred vs synchronous.
- No dragon content. See the backlog review row.

**Plan:** none yet. Not written until the spec is Approved.
