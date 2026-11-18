# Plan: A4 Smoothing (A4 Weather Feature 9)

> **Re-plan required (2026-11-18, world-sim review):** tasks below were written before the amendment in weather-smoothing.md and are stale where they touch preview kinds and the parameter source. Re-plan before /implement.

**Spec:** @docs/specs/applications/cow/systems/a4-weather/weather-smoothing.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.plan.md (row 6c)

**Starts after A4 Feature 1 (`weather-data.plan.md`) tasks 1 to 14 are Done** (`WeatherFields`, `PrecipitationType`
helpers). Tasks 2 to 6 are pure logic on hand-built `WeatherFields` pairs and can start then. Task 7 (the preview
interface header) needs `weather-influence.plan.md` task 2 (`InfluenceKind`, `kNotLanded`). Tasks 8 to 11 (the influence
preview) need task 7 only. Tasks 12 to 16 need pieces of Features 2 to 5 and are marked "Blocked until ... task N".
**Nothing in this plan is built today** (Features 1 to 6 are all unbuilt); every Blocked mark is a start condition.

**Division of work with the sibling plans (no duplication).** This plan owns the pure blend maths, `SmoothedWeather`,
`InfluencePreview` and the interface they read. Already planned elsewhere and **not repeated here**:
- the module's `GetSmoothed()` and preview-source wiring is a line in `weather-module.plan.md` task 10b; this plan's
  task 13 adds the Feature 9 behaviour tests on top of that wiring;
- `weather-influence.plan.md` task 18 builds the book-side implementation of the interface (`InfluenceBookPreviewSource`)
  and stamps `landedDay` in `InfluenceBook::Age` (its task 5); this plan only defines the interface (task 7);
- panel rows (`commitDay`, `t`, preview count) and the raw-or-smoothed wind toggle are `weather-debug.plan.md` task 20's
  card; this plan exposes the stats struct (task 16) and flags that task 20's text does not name those rows (open item 6);
- nothing here touches saves. `weather-save.plan.md` task 7 (`RestoreCommitted`) must set `previous` equal to `committed`;
  task 12 here adds the member and a test, task 12's notes name the edit in that row.

**Order:** task 12 changes Feature 2's `Commit` from a swap into a three-buffer rotation. Do it after ca-pipeline task 5
is Done and its lifecycle tests exist, because they are the regression net (task 12 re-runs `WeatherLifecycle*`).

All code is CoW (application) under `Cluiche/CoW/Shared/Weather/` and `Cluiche/CoW/Modules/`. No Dia files are added or
edited (AD-001). Where a task could need more than sonnet it says "ask before escalating" (feedback_confirm_opus).

## Implementation Patterns

### Files (Task 1)
- `Shared/Weather/SmoothedWeather.h/.cpp`: `SmoothedSample`, `kCalmSpeed`, `kPrecipEpsilon`, `kSnowCoverThreshold` (the
  0.1 constant, shared with `WeatherSnapshot`: reuse its definition if it exists, else define once here and have the
  snapshot include it; record in task 0), the free helpers, `SmoothedWeather`.
- `Shared/Weather/InfluencePreview.h/.cpp`: `PreviewWeight`, `InfluencePreviewInfo`, `IInfluencePreviewSource`,
  `InfluencePreview`.
- `Shared/Weather/Testing/SmoothingTestStates.h`: hand-built `WeatherFields` pairs, a `FakeInfluenceSource`
  implementing `IInfluencePreviewSource` (tests ship with libraries).
- Tests: `Tests/GoogleTests/CoW/Weather/TestSmoothedWeather.cpp`, `TestInfluencePreview.cpp`. Suite prefixes `Smooth*`
  and `InfluencePreview*` (the spec's scoped filter is `*Smooth*:*InfluencePreview*`).

### Blend helpers (Tasks 2 to 4)
- `float WeekFraction(uint64_t day, float dayFraction, uint64_t commitDay)`: `clamp(((day - commitDay) + dayFraction) / 7, 0, 1)`
  with the subtraction done in **signed** 64-bit so a day before the commit clamps to 0 instead of wrapping. `7` is
  `kDaysPerWeek` from `GameSpeedController.h`, never a literal.
- `float BlendAngle(prev, curr, t)`: angles in [0, 2pi). `delta = curr - prev` wrapped into (-pi, pi]; at exactly
  `|delta| == pi` the arc is positive (counter-clockwise). Result `prev + t*delta` wrapped back into [0, 2pi). Wrap uses
  `fmodf`-free arithmetic (add or subtract `2pi` once or twice) so it stays bit-stable and shader-reproducible. Tests give
  a `1e-6` tolerance for angle outputs but exact equality for the endpoints `t = 0` and `t = 1`.
- Calm rule is in the **sample**, not in `BlendAngle`: if either end's speed is below `kCalmSpeed`, the sample uses the
  other end's direction for every `t`; if **both** are calm, use `curr` (decision recorded in the test; the spec leaves
  the both-calm case open, see open item 4).
- `PrecipitationType BlendPrecipType(prevType, prevIntensity, currType, currIntensity, t)`: the four rules in the spec in
  order; the smoothed-intensity-below-epsilon rule is applied by the caller (`Sample`) after the lerp because the helper
  does not see the lerped value. The helper never returns a value outside `None, Rain, Snow`.

### SmoothedWeather (Tasks 5, 6, 13)
- Holds two `const WeatherFields*` and a `commitDay`; owns no field data. Two bind forms:
  `BindFields(const WeatherFields& previous, const WeatherFields& current, float snowThreshold, uint64_t commitDay)`
  (pure, used by tests and available now) and `Bind(const WeatherSnapshot&, const WeatherSnapshot&, uint64_t commitDay)`
  (a thin wrapper over `GetFields()`, added in task 13 when `WeatherSnapshot` exists). `prev == curr` is valid.
- `Sample(cell, t)`: seven `lerp(prev, curr, t)` scalars (temperature, cloud cover, precipitation intensity, snow pack,
  `waterToGround`, wind speed, and wind direction through `BlendAngle` with the calm rule), the type through
  `BlendPrecipType` (type of each end derived the same way `WeatherSnapshot::GetPrecipitationType` does: none at zero
  intensity, else snow below `snow_threshold`, rain otherwise; task 0 confirms the snapshot's rule and the shared helper),
  `SnowCover()` from the **smoothed** snow pack against `kSnowCoverThreshold`. `moisture`, `pressure` and
  `pressureAnomaly` are never read.
- `lerp(a, b, t)` is written `a + t * (b - a)` with `t = 0` returning `a` exactly and `t = 1` returning `b` exactly (the
  endpoint tests compare bit for bit, so the form `a*(1-t) + b*t` is **not** used).
- `Sample` and `SampleRange` are `const`, leaf functions, with no allocation, no lock, no logging and no profile scope.
  Structural checks stand in for an allocation hook (task 6, open item 2).

### Buffer rotation (Task 12)
- `WeatherSimulation` holds three `WeatherFields` (previous, committed, working) and `commitDay`. `Commit(uint64_t
  commitDay)` rotates by index (`std::swap` of three indices or pointers, no `WeatherFields` copy): `previous <- committed`,
  `committed <- working`, `working <- old previous`. `GetPreviousSnapshot()` is const and valid until the next `Commit`.
- The old `Commit()` of ca-pipeline task 5 becomes `Commit(commitDay)`; `Step(...)` takes the day it commits at (the
  headless runner passes its own clock). Existing callers (ca-pipeline tests, `WeatherStage`) are updated in the same
  commit, and `WeatherLifecycle*` and `WeatherDeterminism*` are re-run to prove no value changed (open item 3).
- At `Initialise`, at the first commit and after any `RestoreCommitted`/`RestoreCompleted`, `previous` is a **copy** of
  `committed` (the only copies this feature makes). `previous` is never saved.
- **Hazard checked in task 0:** the rotation leaves `working` holding the *older* previous state, not the committed one.
  Task 0 confirms Feature 2's `Capture` copies committed into working before `Run` (so `Run` never relies on `working`
  being last week's state). If it does not, stop and flag it: the rotation would change results.

### InfluencePreview (Tasks 7 to 11)
- `struct PreviewWeight { uint32_t cell; float weight; }`.
  `struct InfluencePreviewInfo { InfluenceKind kind; InfluenceId id; uint32_t requestDay; uint32_t landedDay; DynamicArrayC<PreviewWeight> weights; }`
  (`id` is added to the spec's field list, open item 5). `IInfluencePreviewSource { GetCount(); Get(index); }` as in the
  spec. `InfluenceKind`, `InfluenceId` and `kNotLanded` come from `WeatherInfluence.h` (Feature 7).
- Strength `s` (a pure function `PreviewStrength(const InfluencePreviewInfo&, uint64_t day, float fadeInDays)`):
  pending: `clamp((day - requestDay) / fadeInDays, 0, 1)`; landed: `1 - clamp((day - landedDay) / 7, 0, 1)` (zero from
  seven days after landing). The `t` argument of `Update` and `dayFraction` are open item 1; this plan implements the
  per-day form and records the sub-day gap.
- Looks (constants provisional, visual only), per influence at cell weight `w` and strength `s`: storm cloud `+0.5*s*w`,
  precipitation `+0.3*s*w`; wetter `+0.25*s*w`, `+0.1*s*w`; drier cloud **scale** `1 - 0.5*s*w`; warmer temperature
  `+0.05*s*w`; cooler `-0.05*s*w`. Storage is four arrays sized N at `Reset(cellCount)` (`cloudAdd`, `precipAdd`,
  `tempAdd`, `cloudScale` initialised to 1; the spec says three arrays, drier needs a multiplier, open item 5). Read:
  `cloud = clamp(smoothedCloud * cloudScale + cloudAdd, 0, 1)`, precipitation and temperature `clamp(x + add, 0, 1)`.
- `Update(source, day, t)` runs at most once per game day (remembers `mLastUpdateDay`). With a zero live count it does no
  work. Otherwise it rebuilds the sparse list per influence id **only when the id is new**, clears the previously
  touched cells in the four arrays (cells listed in `mTouchedCells`, never a full-grid fill), recomputes the deltas for
  the touched cells and rebuilds `mTouchedCells` (sorted, de-duplicated). A test-visible counter
  `GetLastUpdateWorkCount()` returns the number of cell updates, used by AC 22 and 30.
- `fadeInDays` comes from `influence.preview_fade_days` (Feature 1 table row) through
  `InfluencePreview::SetFadeInDays(float)`, called by the module with the latched parameter (open item 1).
- The class includes only `SmoothedWeather.h`, `WeatherTypes.h` and the interface header; it never includes
  `WeatherSimulation.h`, so the isolation checks in task 14 are a header-include check plus `dia check deps`.

### Testing
- Hand-built state pairs: `MakePair(...)` helpers that fill the seven smoothed arrays with chosen values. Known answers are
  written as literals in the tests, not computed by calling the helper under test.
- Randomised range check (AC 7) uses a fixed-seed `std::mt19937` **inside the test only** (production code has no RNG).
- Determinism and isolation tests (task 14) use Feature 2's `SerialExecutor` and `FixedRangeExecutor`, never a second set.

### Verification cadence
- Per task: narrow `dia run googletest --filter="*Smooth*:*InfluencePreview*" --shards 2` (always `--shards 2`); add `WeatherLifecycle*` for task 12.
- **No full-suite run in this plan** (it is `worldstep.plan.md` Weather-stack close-out). No mutation checks. Sanitizer: on request only (user, 2026-11-17). The feature-wide filter runs only in the scoped-verification task; the full suite and the single Release build run once in worldstep.plan.md, Weather-stack close-out. No `SLOW_` suites (longest run is 104 weeks). (2026-11-17, speed-ups)
- One commit per task, with the plan row updated in the same commit through `dia docs plan`.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Prerequisite check, read only: record the build state of A4 Features 1 to 7 (all unbuilt at planning time) and which tasks wait. Confirm: where `WeatherSnapshot` and `PrecipitationType` are declared and **which plan builds `WeatherSnapshot`** (no plan row names it explicitly; weather-module 11 only tests an empty snapshot) and how `GetPrecipitationType` derives Rain from Snow (a shared helper or inline); whether `kSnowCoverThreshold` already exists; Feature 2's `Capture` copies committed into working before `Run` (the rotation hazard); how `weather-save.plan.md` 7 and 17a restore committed state; `weather-module.plan.md` 10b's wording on `GetSmoothed()`; the exact `Commit` signature in ca-pipeline task 5 | Findings recorded in notes | Not Started | sonnet | TDD-exempt investigation. If `Capture` does not copy committed into working, stop and report: task 12 would change results. If no plan builds `WeatherSnapshot`, add a one-line row to `weather-module.plan.md` only after the user agrees (docs edit outside this task's scope) |
| 1 | Create empty `SmoothedWeather.h/.cpp`, `InfluencePreview.h/.cpp`, `Testing/SmoothingTestStates.h` and the two test files; register each with `dia docs vcxproj-add CoW ...` and `dia docs vcxproj-add GoogleTests ...` (literal paths, one call per file, PowerShell) | Solution builds with the empty files | Not Started | haiku | Mechanical, TDD-exempt. Idempotent. Later tasks edit in place and do not touch the vcxproj again |
| 2 | RED then GREEN: `WeekFraction` | AC 1: 0 at the commit day; `3.5/7` at day +3 with fraction 0.5; clamps to 1 at day +7 and beyond; clamps to 0 before the commit (a day before it, and day 0 with a later commit, no unsigned wrap) | Not Started | sonnet | Prove RED with quoted output. Uses `kDaysPerWeek` from `GameSpeedController.h` |
| 3 | RED then GREEN: `BlendAngle` | AC 10, 11, 13 (helper half): 350 deg to 10 deg at `t = 0.5` gives 0 deg, not 180; an exact reversal goes counter-clockwise and gives the same bits on every call (tested at several `t`); results stay in [0, 2pi) including across the wrap; `t = 0` returns `prev` and `t = 1` returns `curr` exactly | Not Started | sonnet | Pure. Test the tie window as exactly pi and pi plus one ULP |
| 4 | RED then GREEN: `BlendPrecipType` | AC 15, 16, 17 (helper half), 18: neither end precipitating gives `None`; one end precipitating gives that end's type at every `t`; both with different types give `prev`'s type below `t = 0.5` and `curr`'s from 0.5; intensities at or below `kPrecipEpsilon` count as none; the output is always one of the three values over an exhaustive table of type pairs and `t` samples | Not Started | sonnet | Pure. The exhaustive table is one loop, not 27 hand-written cases. Commit tasks 2 to 4 together after the scoped filter passes |
| 5 | RED then GREEN: `SmoothedWeather::BindFields` and `Sample` | AC 4 (sample half: `prev == curr` gives the committed value at any `t`), 5 (`t = 0` equals previous, `t = 1` equals committed, bit for bit, for all seven smoothed fields), 6 (`t = 0.25` is exactly the lerp), 7 (no value leaves its field range over randomised in-range pairs), 8 (no jump at the commit: the sample at `t = 1` of one pair equals the sample at `t = 0` of the next pair built from it), 9 (no extrapolation: a field rising across three commits never exceeds the larger end at any `t`), 12 (calm rule at sample level: either end's speed below `kCalmSpeed` gives the other end's direction for every `t`), 13 (sample direction in [0, 2pi)), 14 (speed is the lerp of the two speeds whatever the directions), 17 and 19 (smoothed intensity at or below epsilon gives `None`; `SnowCover()` follows the **smoothed** snow pack against 0.1) | Not Started | sonnet | Uses `BindFields`, so no `WeatherSnapshot` is needed. `moisture`, `pressure`, `pressureAnomaly` are never read: a test fills them with NaN and confirms the sample is unchanged. Record the both-calm decision (use `curr`) |
| 6 | RED then GREEN: `SampleRange` and the cost and isolation structure | AC 20: `SampleRange(first, count, t, out)` equals a loop of `Sample` bit for bit; both are `const`; a guard-byte buffer around `out` shows nothing outside `[first, first + count)` is written; `SmoothedWeather` has no container or lock member (a `static_assert` on `sizeof` equal to two pointers plus the day plus the threshold, and a grep recorded in notes for `new`, `DynamicArrayC`, `mutex` in `SmoothedWeather.cpp`) | Not Started | sonnet | No global `operator new` hook is added (same decision as ca-pipeline amendment 5); open item 2 asks if the user wants a real counting hook for this one test. Commit tasks 5 and 6 together |
| 7 | RED then GREEN: preview interface and strength curve: `PreviewWeight`, `InfluencePreviewInfo`, `IInfluencePreviewSource`, `PreviewStrength(info, day, fadeInDays)` | AC 24, 25: a pending influence's strength rises from 0 to 1 over `fadeInDays` (tested at 0.25, 1 and 3) and holds at 1; at landing it is `1 - t` so the preview is full at `t = 0` and exactly 0 at `t = 1`; the preview plus the real blended-in change is continuous across the commit (hand-built pair with a known delta); a fake source implementing the interface compiles | Not Started | sonnet | **Needs `weather-influence.plan.md` task 2 Done** (`InfluenceKind`, `InfluenceId`, `kNotLanded`). This is the header `weather-influence.plan.md` task 18 waits for, so do it early. Pure; no `SmoothedWeather` needed |
| 8 | RED then GREEN: preview looks and clamps in `InfluencePreview::Update` and `SampleWithPreview` | AC 26, 27: storm and wetter raise cloud cover and precipitation only inside the influence's weight; drier lowers cloud cover through the scale and never below 0; warmer and cooler change temperature only; the delta scales with the cell weight; several overlapping influences add then clamp to each field's range | Not Started | sonnet | Uses `FakeInfluenceSource` from the testing header and `BindFields` states. Constants are provisional (spec); put them in one `kPreviewLook` table so a tuning pass is one edit |
| 9 | RED then GREEN: preview storage, once-per-day update, touched cells, no-work paths | AC 22 (a counting hook shows `Update` does no cell work when there is no active preview, and nothing runs per day with no reader), 30 (with no active influence `SampleWithPreview` equals `Sample` bit for bit), `Update` twice on one day does the work once, a removed influence's cells are cleared without a full-grid fill (the work count equals the touched cells) | Not Started | sonnet | Work counter is `GetLastUpdateWorkCount()`. Sparse lists are built once per new influence id |
| 10 | RED then GREEN: expiry, `Clear`, restart after load | AC 28 (an influence that expires while pending drops its preview at once and logs once), 29 (after `Clear()` or a load, pending previews appear at full strength with no fade-in) | Not Started | sonnet | The log is behind a small `IPreviewLog` seam so the test counts calls without the logging system. Wire the real `WTH` channel in task 16. Commit tasks 7 to 10 together |
| 11 | RED then GREEN: `InfluencePreview::SetFadeInDays` and parameter latch | AC 24 again with the latched value from the `influence.preview_fade_days` row: the default 1 and the range ends 0.25 and 3; a value outside the range is the table's concern (Feature 1 rejects it), the preview only clamps the divisor above 0 | Not Started | haiku | Small; the module's call site is task 13. Skip with a note if folded into task 7's tests |
| 12 | RED then GREEN: three-buffer rotation, `commitDay`, `GetPreviousSnapshot` | AC 2 (after two commits `previous` holds the first committed state and `committed` the second, bit for bit; the data pointers show a swap of ownership, no field copy), 3 (the previous and committed views stay unchanged until the next `Commit`), 4 (buffer half: at `Initialise`, the first commit and after `RestoreCommitted` `previous` equals `committed`), and a re-run of ca-pipeline's lifecycle and determinism tests to show no value changed | Not Started | sonnet | **Blocked until ca-pipeline task 5 (lifecycle) and task 20 (determinism).** Edits Feature 2's `WeatherSimulation` and callers in place (`Commit` gains `commitDay`). The `RestoreCommitted` edit lands in `weather-save.plan.md` task 7's file: if that row is already Done, change it here, else leave a one-line note on that row. Ask before escalating |
| 13 | RED then GREEN: `Bind(WeatherSnapshot, WeatherSnapshot, commitDay)` and module behaviour | AC 3 (module level: both snapshot views valid until the next `Commit`), 29 (module level: previews cleared on load and on a world change), the module binds at `Commit` and returns `GetSmoothed()` with `IsBound()` false before the first world | Not Started | sonnet | **Blocked until weather-module task 5 (`Commit`), 10b (the `GetSmoothed()` and source wiring) and the `WeatherSnapshot` class (task 0 finds its owner), and Task 12.** 10b only wires; this task proves the Feature 9 behaviours. Calls `SetFadeInDays` from the latched parameters |
| 14 | RED then GREEN: isolation from the simulation | AC 21 (committed fields after 104 weeks are bit-identical with a reader sampling every day, with no reader, and with `previous` never read), 31 (removing the preview changes no simulation output; `InfluencePreview.h` has no include of `WeatherSimulation.h` and takes only const references, shown by `dia check deps` and a compile-time signature test) | Not Started | sonnet | **Blocked until ca-pipeline task 20 and Task 12.** Run the 104-week cases once at the end of the task |
| 15 | RED then GREEN: nothing smoothed is saved | AC 23: a `WeatherSaveParticipant` round trip is byte-identical with previews active and with none | Not Started | sonnet | **Blocked until weather-save task 8 (participant `Serialize`) and 10.** Real JSON text compare, no mocks |
| 16 | RED then GREEN: instrumentation and stats | Gauge `cow.weather.preview_active` registered with `MetricInfo` and written once per applied step on SimPU; Warning once when `t` had to be clamped by more than a day (a late commit); Info when a preview is dropped because its influence expired while pending (the real `WTH` channel behind the task 10 seam); a `SmoothingStats { commitDay, t, previewCount }` accessor for the panel | Not Started | sonnet | **Blocked until weather-module task 15 (logs and health) and weather-debug task 19 (metric registration helpers).** No per-read telemetry (SD-PERF-7). The panel rows and raw-or-smoothed wind toggle are weather-debug task 20's (open item 6) |
| 17 | Exhaustive test pass: gap audit of AC 1 to 31 against test names, then edge cases | Gap list in notes. Edge cases: `t` exactly 0, 1, 0.5; `t` out of range passed by a caller (clamped or rejected, recorded); a 1-cell world; a world where every cell has speed under `kCalmSpeed`; both ends calm; reversal at exactly pi on both ends; precipitation type flipping at exactly `t = 0.5`; intensity exactly `kPrecipEpsilon`; a preview weight of 0 or 1; 64 live previews on one cell; an influence added and removed within one day; `Update` called with a day earlier than the last; `SampleRange` with count 0 and with the last cell | Not Started | sonnet | No mutation check. Add only tests the audit shows missing |
| 18 | Observation Opportunity Scan over every file touched across logs, traces, profiling, metrics, health | Suggestions in notes | Not Started | haiku | Non-blocking. Expected finds: `Update` duration, a reads-per-frame counter (rejected by SD-PERF-7, note only), a late-commit counter |
| 19 | Scoped verification: `dia run googletest --filter="*Smooth*:*InfluencePreview*" --shards 2`; `dia check deps`; `dia docs precommit` | Pass/fail and failing names only; AC 1 to 31 listed with the task that proved each, blocked ACs named | Not Started | haiku | **No full-suite run here** (it is `worldstep.plan.md` Weather-stack close-out). Report pass/fail only. ACs still Blocked at this point are reported as Blocked, not failed |
| 20 | Close-out: `dia docs plan` rows to Done with notes, then `dia docs spec-done docs/specs/applications/cow/systems/a4-weather/weather-smoothing.md` | Spec status Done, plan header updated | Not Started | haiku | **Do not run until every row above is Done.** Not to be run when this plan is written |

## Open items

1. **Update cadence versus the fade.** The spec says `Update(source, day, t)` runs at most once per game day, but the
   pending fade is 0.25 to 3 days (default 1) and uses `dayFraction`; `Update` has no `dayFraction`, and `t` is the week
   position. With a once-per-day update the default 1-day fade is a single jump from 0 to 1 at the next day. This plan
   builds the per-day form exactly as written and records the gap; if the user wants a smooth sub-day fade, `Update`
   gains a `dayFraction` argument (and the touched-cell recompute runs per frame). Needs a decision before task 8.
2. **AC 20 "allocation-counting test".** The spec asks for one; ca-pipeline decided (amendment 5) to add no global
   `operator new` hook. Task 6 uses structural checks instead (const signature, no container member, guard bytes, a
   grep). Confirm that is acceptable or say a real counting hook is wanted for this one test.
3. **`Commit` signature change.** The spec says the module passes the apply day to `Commit`; ca-pipeline task 5 builds
   `Commit()` with no argument and `Step` without a day. Task 12 changes both and updates callers. If Features 2 and 3
   are built by then, that is a visible edit across their files and tests; the plan keeps it to one commit with the
   lifecycle and determinism suites re-run. Still open (low risk, wide touch).
4. **Both ends calm.** The spec gives a rule when one end is calm only. Task 5 uses the committed end when both are
   calm and records it in the test name. Confirm, or name another rule.
5. **Preview data model gaps in the spec.** (a) `InfluencePreviewInfo` has no `id`, but the preview must tell a new
   influence from one it already holds; this plan adds `InfluenceId id`. (b) Drier scales cloud cover, which a pure
   additive delta array cannot hold; this plan adds a fourth array (`cloudScale`) beside the spec's three. Both are
   implementation details the user may veto; neither changes an acceptance criterion.
6. **Panel rows are not in weather-debug's plan.** `weather-debug.plan.md` task 20 builds the card but its text does not
   mention the `commitDay`, `t` and preview-count rows or the raw-or-smoothed wind toggle that this spec assigns to
   Feature 5. Task 16 provides the stats struct; the rows need a one-line addition to task 20 (docs edit, user
   agreement first).
7. **`WeatherSnapshot` ownership.** No plan row names the class explicitly (see task 0). It gates tasks 13 and 15 only;
   the pure tasks do not depend on it.
8. **Reader off SimPU (spec Open Design Questions 1 to 3).** Transport of the two states to V-1, shortest-arc versus
   vector blend for reversals and honesty of the preview strengths all stay open by design and are decided against the
   first V-1 render. Nothing in this plan pre-empts them.
