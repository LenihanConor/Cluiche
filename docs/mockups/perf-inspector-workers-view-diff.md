# Perf Inspector Workers view: mockup vs built

Mockup: `docs/mockups/perf-inspector-workers-mockup.html` (approved 2026-11-12). Built: `Dia/DiaPerfInspector/UI/src/components/workers/` (`WorkersView`, `StepDetail`, `CanvasRow`) with the draw functions in `src/model/drawWorkers.ts` and the model in `src/model/jobs.ts`.

**This comparison was made from the mockup HTML/CSS and the component source only. Nobody has looked at the rendered Workers tab. The user's visual sign-off is still required** (spec task 14 acceptance gate). Pixel-level claims (spacing, colours as rendered) are not made here.

Data note: the live game sends `steps: []` today (DiaDeferredStep not built) and publishes the pool counters `cap_use`, `peak_queue_depth`, `time_at_cap` as unavailable. Every step element below was exercised only against the synthetic fixture `src/model/__fixtures__/jobs.fixture.json`.

Status key: **built**, **differs** (built, but not as drawn), **not built**.

| Mockup element | Status | Notes |
|---|---|---|
| Tab switch `Frames` / `Workers` in the toolbar | differs | Built as a real tab strip under the connection banner (`role=tablist`), not inside the mockup toolbar. Frames content is unchanged. |
| Toolbar: Connected / app / ws url text | not built | The existing connection banner is reused. |
| Toolbar: Zoom 10 s / 2 s / 200 ms | built | Zoom 2 s and 200 ms fetch raw records (tier 3). 10 s is the aggregate view. |
| Toolbar: `Fit step` button | not built | Selecting a step already loads and shows its kick to apply range; no separate fit button. |
| Toolbar: Pause | not built on this tab | The Frames Pause drives the shared view state; the Workers heat strip follows the same domain (including pause) when lanes exist. No second Pause button. |
| Toolbar: Export Perfetto | not built here | Export lives on the Frames tab; it does not include jobs yet. |
| Timeline: time axis with key ticks | not built | No axis ruler. The heat strip and step rows have no tick labels. Needs a clock reference on the wire for keys. |
| Timeline: Sim lane inside the Workers tab | not built | The Sim frame lane stays on the Frames tab. The Sim stall of a late step is not drawn on a Sim lane here (AC12 half is E2E). |
| Worker rows W0..W7 with per-worker utilisation % | differs | Rows by worker are built from raw records (tier 3) at zoom of 2 s or less or when a step is selected. Utilisation % per worker is not shown. Row labels are `W<n>` only. |
| Worker slices: owner colour, label text inside wide slices | differs | Owner colour (same stable colour index as modules): built. Text labels inside slices: not built. Hatched in-frame slices: not built (no in-frame flag decoded yet; `flags` is read but not styled). |
| Worker slice click selects a job and shows Job detail card | not built | Slices are not clickable. No job detail card. |
| Selected step highlights its ranges on worker rows, dims other deferred jobs | differs | Slices of the selected step get a 1 px white top edge; others are not dimmed. |
| 10-60 s view without slices: owner heat strip | built | One stacked owner-coloured bar per 200 ms bucket per unit, bar height relative to the tallest visible bucket. Spec Goal 9 replacement for worker rows at 10 s; the mockup itself has no heat strip. Aligned to the lane axis (t = frame / hz). |
| Cap row: concurrency step graph with cap M line and waiting-for-cap shading | differs | Built as a status row only. The game publishes `capUse` as unavailable, and there is no time series, so the row is empty with the message "cap use unavailable ...". When `capUse` becomes available it prints the number, not a graph. |
| Deferred-step rows: kick to apply outline, run fill, red stall, green ready tick | built | One canvas row per channel; click selects a step. Row label text inside bars ("key 4 to 6, succession event") not built. |
| Deferred-step rows share one time axis with the worker rows | differs | Step rows use the steady-clock ns axis of the step summaries; the heat strip uses the lane axis. The wire has no mapping between them, so they are not aligned with each other. |
| Legend row (owners, Sim modules, stall) | differs | Owner swatch legend under the heat strip. No Sim modules or stall swatches. |
| Left panel: Deferred-step channels table (run p95, window p95, late count, every/k/ranges/working copy) | not built | Replaced by one button per step instance (`step-item`) plus the channel rows. Per-channel p95 stats, k, ranges and working copy size are not on the wire. |
| Left panel: Gameplay jobs table (jobs, p50, p95 queue wait) | differs | Built in Pool health as "Queue wait p95 by owner" (all owners, p95 only, worst first). p50 and job count are in the model but not shown. |
| Left panel: Cap M what-if select and show/hide toggles for gameplay / in-frame jobs | not built | Cap is read-only (Open Question 5); the what-if re-simulation is client-only and was not built. Show/hide toggles not built. |
| Detail: step Gantt (capture, ranges per worker with grey worker wait and dark red cap wait, apply, stall) | not built | Replaced by text rows. Wait is shown as two numbers (cap wait, worker wait), not as bars on a Gantt. |
| Detail: Cost card (critical path, run span, CPU, parallelism, imbalance, cap wait, worker wait, window used) | built | `StepDetail`. Critical path reads "unavailable" when the summary has no `captureMs` / `applyMs`; imbalance reads "unavailable" without raw records. Inline fallbacks and working copy rows: not built (not per step on the wire). |
| Detail: "Inside this step" sections | not built | Shown as "unavailable (needs profiling on; not on the wire yet)". |
| Detail: "Workers during this step" owner share | built | Owner share of worker time while the step ran, from raw records, integer percentages sum to 100. Shown as a list; the stacked bar is only on the window share. The mockup's "this step" white segment is not separated. |
| Detail: diagnosis banner (what-if M x 2, cap vs pool verdict) | not built | Needs the what-if. Cap vs worker wait numbers are shown instead. |
| Detail: Job and Sim frame detail cards | not built | |
| Right: Late results list with stall, jump to step | built | Click selects the step (and loads raw records). Pill styling and the "n in window" counter text are simplified. Today the list is always empty against the live game. |
| Right: Pool health Healthy / Degraded pill | not built | |
| Right: Where worker time went (stacked bar plus rows) | built | Window share from tier 2 aggregates for the visible domain; partial buckets count proportionally. |
| Right: Worker utilisation meters per worker | not built | |
| Right: Queue wait p95 by kind (deferred, gameplay, in-frame) | differs | By owner (what tier 1 carries), not by kind. |
| Right: Cap and queue (time at cap, peak queue depth, inline fallbacks, jobs completed) | differs | Time at cap, peak depth, inline fallbacks and inbox drops built; each reads "unavailable" when the game lists it in `unavailable`. "Jobs completed" not built. |
| Tooltip on hover | not built | |
| Honest empty states (not in the mockup) | built | "no deferred-step data from the game yet (DiaDeferredStep not built)", "per-job recording off: aggregates only", "truncated" notice, per-metric "unavailable". |

## Why some things are not built

- No clock reference on the wire: lane time is frame / hz, steps and raw records are steady-clock ns. Zooming therefore first finds the newest record time through `perf.jobs` (the game keeps the oldest records when it truncates, so the store repeats the query from the newest end seen, at most 4 times), then fetches the window ending there. That anchor is not aligned with the lanes' time axis.
- Cap, worker utilisation, in-frame hatching, sections and what-if all need data or design that is outside this task (game sampler, profiling sections, Open Question 5).
