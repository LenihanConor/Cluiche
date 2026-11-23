# Feature Spec: V-1 Checks and Captures

## Parent System
@docs/specs/applications/cow/systems/v1-tilerendering/v1-tilerendering.md (Feature 4)

**Gameplay Domains:** rendering, debug
**Depends on:** V-1 Features 1–3; `cow.map.capture` (Strategic Map Feature 4); DiaAutomation (checkpoints, reports)
**Design source:** GDD `10_PrototypeStrategy/V1_TileRendering.md` (Tests 1–9, locked)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/tools/shot.sh` (headless capture with `#key=value` state), `lookdev/shots/`

## Status
`Draft` (2026-11-20). Drafted from the GDD test list and the approved prototype; awaiting user review.

## Summary

The GDD's Tests 1 to 9b as repeatable checks, plus a fixed **capture set** for the user's taste review. The checks are mostly **data and measurement** (feedback: cell data over pixels); pixels are used only where the thing under test *is* the rendered colour, and then on small synthetic patch worlds rather than on screenshots of real worlds.

- **Correctness checks (Tests 1–6)** run in Correctness mode on the curated Standard world.
- **Performance checks (Tests 7–9b)** run in Performance mode on Large and record frame, bake and update timing.
- **Capture set**: one image per channel and preset at a fixed world, camera and seed, written to `out/CoW/v1/captures/` for review, never as a pixel gate.

## Problem

- The GDD tests are manual inspections ("can you identify each biome at a glance?"). Without measurable proxies they depend on whoever looks, and silently stop being run.
- Some of the GDD's wording no longer fits the chosen approach (180 tile variants; "static, no animation"). Each test needs an honest, measurable restatement and the deviations recorded.
- Perf checks need a reproducible harness at Large with everything on.

## Goals

1. **Tests 1–6 as measurements**, listed in the table below, each with a numeric pass condition and the GDD wording it replaces.
2. **Tests 7–9b as timed runs** with recorded numbers and a pass line (30 fps at Large, full zoom range, update sync).
3. **Capture set**: a fixed list (name, mode, world, camera, preset, seed, day) rendered through `cow.map.capture`.
4. **Report**: one JSON report (`cow.v1_report`) with every check's result, measured values and the capture file names; produced headless through DiaAutomation.
5. **Runner**: GoogleTests for the pure measurements, an E2E scenario for the rendered ones and the timed runs (`dia test e2e`).
6. **GDD deviations noted in the report** so they feed the GDD fold-back.

## Non-Goals

- Pixel-exact image regression. Captures are for the user's eyes.
- Judging art quality (the user does that on the capture set).
- A GPU benchmark rig (feedback: quick and dirty perf, no Release benchmarking or rigs).
- Checking A1/A4/A5 correctness.

## Design

### The checks

| GDD test | Restated check (measured) | Pass condition |
|---|---|---|
| 1 Biome visual distinction | For every pair of registry land biomes, the colour distance (CIE ΔE76) between their rendered flat patches at strategic zoom, in Hunt season, on a synthetic patch world | ΔE ≥ 15 for every pair; the closest pair is reported |
| 1b Readability at extreme zoom-out | Same patches rendered with each cell 4 px wide, then downsampled | ΔE ≥ 10 for every pair; failures listed (a Phase 3 constraint per the GDD) |
| 2 Ecology state distinction | For each biome, rendered patches at health 0.9 / 0.55 / 0.25 / 0.05 | adjacent bands differ by ΔE ≥ 8 and saturation falls monotonically; prop density falls monotonically (data) |
| 3 Seasonal cycle | For each biome, patches in each of the four seasons | seasons differ by ΔE ≥ 5 from Hunt (the grade alone guarantees it); no biome unchanged across seasons; the cross-fade has no jump (adjacent-day ΔE ≤ 2) |
| 4 Weather rendering | With the weather channel set from known cells: rain/snow particles exist only where precip > 0 (data); cloud shadow darkens a patch where cloud > 0.3 by 5–30%; wet darkening exists under rain; snow cover appears where snow pack and temperature say | counts and measured darkening in range |
| 5 Succession progression | A cell's condition path Depleted → Recovering → Stable | monotone, distinct bands (as Test 2); a class change fades over the configured weeks without a single-frame jump (adjacent-frame ΔE ≤ 3) |
| 5b Biome change | Class change in the snapshot | new palette reached at the end of the fade; fade length equals the configured value |
| 6 Continuous zoom | 24 zoom levels from overview to a single cell on a fixed world | no NaN/black/blank frames; ink width within 1 px of target at every level; mean luminance within 15% of the median; no more than one visible LOD switch of hatch spacing per octave of zoom |
| 7 Full-scale rendering | Large with everything on, 600 frames | median frame ≤ 33.3 ms (30 fps); p95 recorded |
| 8 Full-scale zoom | Zoom sweep overview → regional → tactical on Large | median ≤ 33.3 ms at each stop; the tile cache stays within budget |
| 9 Full-scale simulation | One game year at Fast with the sources running (stand-ins or live A1/A3/A4); measure frames from a channel change to its visible effect | median ≤ 33.3 ms throughout; a condition or weather change is visible within 1 frame of its channel version changing; a class change within bake time + 1 frame; no frame over 100 ms during updates |
| 9b Weather particle performance | Weather channel set so 30% or more of the world has precipitation, overview camera | median ≤ 33.3 ms; live particles never exceed the budget; spawning is limited to the viewed rect (counter) |

The GDD deviations are explicit: Tests 1–5 judge field-driven rendering, not 180 tile variants; Test 4's cloud darkening is a local shadow (5–30%) not a uniform 40% multiply; Test 3's "season transitions instantaneous" is replaced by a cross-fade.

### Capture set

| Name | Mode | World | Camera | Preset | Notes |
|---|---|---|---|---|---|
| `overview` | Look-dev | v1_default | overview | strategic | |
| `mountains` | Look-dev | v1_mountains | regional, tilt 58° | strategic | |
| `close` | Look-dev | v1_default | tactical | strategic | |
| `freeze` | Look-dev | v1_mountains | overview | strategic, day in Freeze | snow |
| `dry-fire` | Look-dev | v1_all_biomes | regional | strategic, Dry, ignite | fire and smoke |
| `storm` | Look-dev | v1_mountains | regional | strategic, 4 storms | rain and snow |
| `collapsed` / `thriving` | Look-dev | v1_default | regional | strategic, health 0.1 / 0.95 | |
| `cinematic` | Look-dev | v1_default | overview, low | cinematic | |
| `debug-flat` | Correctness | v1_all_biomes | overview | debug-flat | class colours + grid |
| `large` | Performance | v1_large | overview | strategic | |

### Report and runner

`cow.v1_report`: per check `{id, restatement, measured, pass, notes}`, the deviations list, timings, and capture paths. Pure measurements are GoogleTests (`V1Checks_*`, run with the usual scoped filters). Rendered checks and timed runs are one E2E scenario (`dia test e2e`); the report is written through DiaAutomation. Slow checks use the `SLOW_` prefix convention.

## Acceptance Criteria

1. **Each check implemented.** Every row in the table has a measurement function, a unit test with a known-good and a known-bad input (for example a palette with two identical biomes fails Test 1), and appears in the report.
2. **Test 1 and 1b.** The shipped palettes pass; a modified table with two near-identical biomes fails and names the pair.
3. **Test 2.** Condition bands separate by the stated ΔE on the shipped style; collapsing the saturation bands makes the check fail.
4. **Test 3.** All four seasons differ from Hunt for every biome; cross-fade adjacency holds for all 364 days.
5. **Test 4.** Particle counts follow the weather channel exactly (zero where precip is zero) and shadow darkening is in range.
6. **Tests 5 and 5b.** The condition path and class change meet the stated bounds with a succession source switched in.
7. **Test 6.** All 24 zoom levels pass the stated conditions.
8. **Tests 7–9b.** The timed runs record median and p95 and the pass line; the report states the machine and configuration. A miss queues a perf task and is not hidden.
9. **Capture set.** All captures are produced with the fixed arguments; two runs give byte-identical PNGs on the same machine; the report lists them.
10. **Report.** `cow.v1_report` validates against its sidecar, and the deviations list is present.
11. **Headless.** The whole suite runs with no window input through DiaAutomation (`dia test e2e`); a failing check fails the scenario with the check id.
12. **Data over pixels.** Only Tests 1, 1b, 2, 3, 5 and 6 render pixels, and only synthetic patch worlds and fixed zoom sweeps; every other check asserts on data and counters.
13. **User review.** The user reviews the capture set once per feature and signs off the V-1 close-out (taste checks stay human).
14. **Observability.** Log channel `V1`; one info line per check with its measured value; metric `v1.checks_failed`; health Degraded after a failed run.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| V1-003 | Correctness is asserted on data; captures are the user's taste check | AC 12, 13 |
| Feedback: cell data over pixels | Verify world-sim via data and stats, not screenshots | Same rule applied here; pixels only for colour distinctness |
| Feedback: quick and dirty perf | No Release benchmarking or rigs | Debug-only runs, ballpark, recorded; no rig |
| Feedback: exhaustive test pass at the end | Gap audit, edge tests | Each check has a known-bad case (AC 1) |
| Feedback: no mutation checks | Dropped from close-out | None planned |
| GDD (V1_TileRendering) | Tests 1–9 locked | Restated with deviations recorded (AC 10) |
| AD-003 | GDD owns intent | The deviations list feeds the fold-back to the GDD session |

## Open Design Questions

1. **ΔE thresholds** (15, 10, 8, 5) are my starting numbers, not the GDD's (the GDD says "readable"). Calibrate against the approved prototype palettes first and set thresholds just under the worst shipped pair, so the check catches regressions rather than failing on day one.
2. **Rendering a patch world** needs a way to build a synthetic world without S-1. A small test helper in V-1 (not S-1) builds uniform-biome patches; confirm that does not need an S-1 change.
3. **Machine variance for Tests 7–9b.** Frame time depends on the GPU. The pass line is for the dev reference machine; the S-3.5 `reference_machine.json` convention (used by R5) may apply; reuse it if S-3.5 lands first.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Colour measurement helpers (ΔE, saturation, luminance) and patch-world builder | unit tests | Not Started | sonnet | |
| 2 | Tests 1, 1b, 2, 3 | AC 2, 3, 4 | Not Started | sonnet | threshold calibration (ODQ 1) |
| 3 | Test 4 | AC 5 | Not Started | sonnet | |
| 4 | Tests 5, 5b | AC 6 | Not Started | sonnet | needs succession source |
| 5 | Test 6 zoom sweep | AC 7 | Not Started | sonnet | |
| 6 | Tests 7–9b timed runs | AC 8 | Not Started | sonnet | Debug-only |
| 7 | Capture set | AC 9 | Not Started | sonnet | |
| 8 | Report, headless E2E scenario, observability | AC 10, 11, 14 | Not Started | sonnet | |
| 9 | Gap audit and edge cases; user close-out review | AC 1, 12, 13 | Not Started | sonnet | |
