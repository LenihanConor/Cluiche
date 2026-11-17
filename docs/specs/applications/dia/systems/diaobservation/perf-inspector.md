# Feature Spec: Perf Inspector

**Parent:** @docs/specs/applications/dia/systems/diaobservation/diaobservation.md
**Amends / depends on:** @profiling-frame-instrumentation.md (Amendment 2026-11-09, A1-A4); @perf-live-control.md (switches, markers, latency)
**Mockup:** @docs/mockups/perf-inspector-mockup.html (approved 2026-11-09); Workers view: @docs/mockups/perf-inspector-workers-mockup.html (approved 2026-11-12)
**Depends on (Revision 2026-11-12):** @docs/specs/applications/dia/systems/diathreading/job-observation.md (job context and timing records); @profiling-frame-instrumentation.md Amendment 2026-11-12 (worker scopes, `dia perf convert`)
**Status:** `Done` — 2026-11-17 (Approved 2026-11-09). Built per [perf-inspector.plan.md](perf-inspector.plan.md); visual sign-off given by the user 2026-11-17. **Accepted follow-ups at close (not built or not verified):** AC6 telemetry cost NOT measured (ballpark-only decision, no Release benchmarking); AC3, AC5 live and AC12 E2E not run; deferred-step summaries are empty until DiaDeferredStep exists (Workers step view runs on fixtures only); the `observation.perf.jobs` kill switch has no config seam (`SetPerJobRecording` only); pool counters `capUse`, `peakQueueDepth`, `timeAtCapMs` read 0 (no sampler); `marker_drops`, `suppressed_trips`, `suppressed_logs` unavailable; manual Perfetto load of an export not done; live chain needs tracing enabled in the observation config. Original status notes follow: **Revised 2026-11-10** after the plan audit: long coarse ring adopted (Open Question 1), `perf.timing` carries latency and self-cost, AC5's live chain deferred with trace-linked streams, `perf.window` size fallback. **Revision 2026-11-12 (Approved 2026-11-12):** Workers view (jobs and deferred steps), three wire tiers, telemetry cost budgets SD-PERF-7 (tightens AC6), no perf Editor for now.

## Summary

A CluicheEditor plugin (Inspector tier: the editor connected to the running game over DiaDebugServer) that shows where frame time goes: one lane per ProcessingUnit with module-coloured stacked bars against the unit's budget, trips, event markers, the selected frame's module and section breakdown, the cross-thread cost chain, end-to-end latency, and the telemetry's own cost. It renders in the editor process, so viewing costs the game nothing. A window can be exported as a Chrome-trace/Perfetto file.

## Problem

Diagnosing the S-1 panel over-budget problem took a subagent about 20 minutes of hand-written timers and A/B rebuilds. The data (per-module timing, trips) is added by the profiling amendment but still lands in files; there is nothing live, visual or comparable. An in-game card is the wrong tool: the S-1 card itself cost 30 ms per frame.

## Goals

1. Live per-unit lanes (Main, Sim, Render): stacked module bars per frame, budget line, trips shaded.
2. Module table per unit: p50 / p95 per module and sections, with the A/B switches from @perf-live-control.md.
3. Selected-frame detail: module and section bars; the cross-thread chain (trace-linked) with stream edges (bytes, rate, data age, write cost).
4. Trips list (from `trips.jsonl` records) with jump-to-frame; event markers on a row beneath the lanes.
5. Latency panel for the actions in @perf-live-control.md, against their targets.
6. Telemetry self-cost panel (`perf.self_ms`, suppressed log lines).
7. Export the visible window as Chrome-trace JSON (Perfetto-compatible).
8. Window 10 / 30 / 60 s; Pause freezes the view only (the game keeps recording).
9. **Workers view (2026-11-12).** Job-worker rows coloured by owner; a cap-usage row; one row per deferred-step channel (kick → apply window, ready point, stall); worker time split by owner for the visible window and for a selected step (for example "is the pool's time mostly weather or ecology?").
10. **Step detail (2026-11-12).** For a selected deferred step: ranges per worker, queue wait split into *waiting for a cap slot* and *waiting for a free worker*, Sim critical-path cost (Capture + Apply + stall), parallelism achieved, range imbalance, share of the kick → apply window used, and sections inside `Run` when profiling is on.
11. **Job-pool health (2026-11-12).** Queue-wait p95 per kind of job (deferred range, gameplay, in-frame where Sim waits), time at cap, peak queue depth, inline fallbacks, and a late-results list with jump-to-step.

## Non-Goals

- In-game rendering of any of this (an in-game card costs frame time; see the S-1 incident).
- GPU timing, memory or allocation tracking (profiling spec v2 hook; later).
- Cross-run baseline diffing and budgets in CI (separate feature).
- Editing profile categories at runtime beyond on/off.
- A perf Editor (offline capture viewer) plugin. Offline deep analysis uses `dia perf convert` and Perfetto (@profiling-frame-instrumentation.md B3). Revisit with baseline diffing and CI budgets. The lane model stays independent of its data source, so a file loader can be added later without touching the UI (decision 2026-11-12).

## Architecture

- **Plugin:** `DiaPerfInspector` (Dia-level Inspector plugin, same pattern as DiaApplicationFlowInspector: C++ plugin + `UI/` TypeScript). It lives in Dia because any game can use it (layer rule).
- **Bridge (DiaDebugServer amendment):** the ObservationBridge gains these channels. All are pull plus aggregated push, never per-frame push:
  - `perf.window {seconds}` (query): per unit, per frame: tick ms and per-module ms (fixed point 0.01 ms, arrays), event markers, trips. Served on Main from the long coarse ring (below). Target response under 120 KB for 60 s at 30 Hz. If the measured size misses the target, the window defaults to 30 s and the plugin fetches 60 s as two requests; the wire format does not change.
  - `perf.timing` (push, 10 Hz): latest per-unit and per-module EWMA/p95 so the table stays live without re-pulling the window, plus latency p50/p95 per action (from @perf-live-control.md probes), `perf.self_ms`, marker drops and suppressed trip/log counts. No separate latency or self-cost channel.
- **Long coarse ring (DiaObservation, 2026-11-10):** per unit, per entry (per fixed step on Sim, as the profiling amendment A2), the unit ms and each module's ms as 2-byte 0.01 ms fixed point (values over 655.35 ms clamp and are flagged), 60 s at the unit's rate (about 50 KB per unit at 30 Hz). Filled beside `UnitTimingTable` on the unit's thread and published to Main by `FrameStream` snapshot, so Main never reads another PU's memory. The 128-entry detailed ring stays the source for `perf.frame`.
  - `perf.trip` (push, event): the trip record when written.
  - `perf.frame {unit, frame}` (query): the section breakdown for one frame from the unit's ring. Sections are only recorded while profiling is on; if off, the response says so and the UI offers "enable profiling".
- **Jobs and workers (Revision 2026-11-12).** Job timing records (@docs/specs/applications/dia/systems/diathreading/job-observation.md) reach the submitting PU through its `JobInbox`. Each PU drains its inbox once per `Update()` into a `JobTimingTable` (new, pure, `Dia/DiaObservation/Profile/JobTimingTable.h`) and publishes it to Main in its existing snapshot. Data reaches the editor in three tiers, so detail only travels when it is being looked at:
  - **Tier 1, always (on `perf.timing`, 10 Hz):** per owner: busy ms, job count, queue-wait p50/p95. Pool: cap use, time at cap, peak queue depth, inline fallbacks, inbox drops.
  - **Tier 2, window (on `perf.window`):** one summary per deferred step (channel, kick key, apply key, kick, first start, ready, apply, stall ms, CPU ms, cap-wait ms, worker-wait ms, ranges), recorded by `DeferredStepModule` (@docs/specs/applications/dia/systems/diadeferredstep/diadeferredstep.md Feature 4); and one aggregate per owner per frame for all other jobs (count, busy ms, worst wait). The 10-60 s view draws the worker rows from these aggregates as an owner-coloured heat strip, not individual slices.
  - **Tier 3, zoom (new `perf.jobs {unit, fromNs, toNs}` query):** raw per-job records (worker, start, end, owner, tag, range) from a short raw ring (about 5 s) per PU, capped at 2,000 records per reply (the reply says if it was truncated). Fetched only when the view is zoomed to 2 s or less, or when a step is selected. Sections inside a step need profiling on.
- **Wire rules (SD-PERF-7, 2026-11-12):** nothing leaves the game per frame. Recording is always on and cheap; sending happens at 10 Hz or on request. Perf payloads are packed numeric arrays (fixed point), never JSON objects built per entry. With no client attached, nothing is packed. Main only copies the published snapshot and hands the buffer to the DebugServer thread, which encodes and sends it, so a window pull never shows up as a spike on Main. Rings are fixed size and drop the oldest entries, so a burst of jobs costs ring writes and nothing more downstream. Kill switch: `observation.perf.jobs = off` keeps only the tier 1 counters.
- **Export:** the plugin builds the Chrome-trace JSON client-side from `perf.window` plus `perf.frame` results; no engine change.

## Acceptance Criteria

| ID | Criterion | Verification |
|----|-----------|--------------|
| AC1 | Connected to a running CoW S-1 stage, the lanes show all three units with the budget line at the unit's target | UI test on a recorded fixture + E2E smoke |
| AC2 | Module bars on a lane sum to the unit's tick ms within 1 ms (remainder shown as "unattributed") | Unit test on the lane model |
| AC3 | A trip record written by the engine appears in the trips list within 1 s and clicking it selects its first frame | E2E (force a trip with a sleeping test module) |
| AC4 | Selected-frame detail lists modules and sections in schedule order with ms; when profiling is off it says sections are unavailable | UI test |
| AC5 | The cross-thread chain shows the S-1 card path (GetJSONState, pack, parse, push) with stream bytes and data age, and its total as a share of one budget | UI test on fixture. **The live E2E half is deferred 2026-11-10** with trace-linked streams; until then the panel shows "chain unavailable" against a live game |
| AC6 | **Revised 2026-11-12 (SD-PERF-7).** Telemetry cost per frame, average / worst single frame: detached (no client) Sim and Main < 0.02 / < 0.05 ms; attached and viewing a 60 s window live, Sim < 0.05 / < 0.1 ms, Main < 0.1 / < 0.25 ms. Measured on the CoW S-1 stage and on a synthetic 10,000 jobs/s flood | Perf test (attached vs detached vs telemetry off) |
| AC7 | `perf.window` for 60 s at 30 Hz with 3 units and 13 modules is under 120 KB | Test |
| AC8 | Exported Chrome-trace JSON loads in Perfetto and shows per-unit tracks and module slices | Manual check recorded in notes + schema test |
| AC9 | Disconnecting the editor leaves the game's behaviour unchanged and removes any A/B pauses (see @perf-live-control.md) | E2E |
| AC10 | Workers view: worker rows, cap row and deferred-step rows render from a recorded fixture; owner time share for the window and for a selected step sums to 100% of worker busy time | UI test on fixture |
| AC11 | Step detail shows cap wait and worker wait separately, and they match the step summary record | UI test on fixture + unit test on `JobTimingTable` |
| AC12 | A forced late step (a test step that sleeps past its apply key) appears in the late-results list within 1 s, and its stall appears on the Sim lane in the same frame | E2E (CluicheTest-side test step) |
| AC13 | `perf.jobs` returns at most 2,000 records and states truncation; tier 2 for 60 s with 2 channels and 5 owners is under 30 KB | Test |
| AC14 | With `observation.perf.jobs = off`, only tier 1 counters are produced and the per-job record path is not taken | Unit test |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Bridge: `perf.window`, `perf.frame`, `perf.timing` push, `perf.trip` push in DiaDebugServer | AC6, AC7 | Done | sonnet | Dia; needs profiling amendment A1-A3 |
| 2 | Plugin shell + lane model (pure TS) + timeline lanes | AC1, AC2 | Done | sonnet | Follow DiaApplicationFlowInspector |
| 3 | Module table, selected-frame detail, trips list | AC3, AC4 | Done | sonnet | |
| 4 | Cross-thread chain + stream edges | AC5 | Done | sonnet | Needs trace-linked stream writes from @perf-live-control.md |
| 5 | Latency + self-cost panels | - | Done | sonnet | Needs live-control latency probes |
| 6 | Chrome-trace export | AC8 | Done | haiku | |
| 8 | `JobTimingTable` (inbox drain, per-owner per-frame aggregates, deferred-step summaries, raw ring) + tier 1 on `perf.timing` + tier 2 on `perf.window` + `perf.jobs` query; encoding on the DebugServer thread | AC11 (table), AC13, AC14 | Done | sonnet | Dia; needs job-observation tasks 2-4. Revision 2026-11-12 |
| 9 | Workers view UI: worker heat strip and slices, cap row, deferred-step rows, step detail, owner share, pool health, late results | AC10, AC11, AC12 | Done | sonnet | Mockup check gate against perf-inspector-workers-mockup.html first |
| 7 | E2E + gap audit + attach/detach cost | AC9, AC6 (revised budgets), AC12 | Done | sonnet | Final verification task; runs after 8 and 9 |

## Binding Decisions

| ID | Decision | How it applies |
|----|----------|----------------|
| SD-PERF-2 | Telemetry never writes at tick rate | The bridge pushes aggregated timing at 10 Hz; everything else is pull or event |
| SD-PERF-4 | The view lives in CluicheEditor (Inspector tier), never in-game | Rendering cost stays out of the measured process |
| SD-PERF-7 | Telemetry cost budgets and wire rules (2026-11-12): detached < 0.02 ms avg / < 0.05 ms worst per frame; attached Sim < 0.05 / < 0.1 ms, Main < 0.1 / < 0.25 ms; nothing sent per frame; packed arrays, no per-entry JSON; no packing when detached; encode off Main | AC6 (revised), Architecture "Wire rules"; also binds @docs/specs/applications/dia/systems/diathreading/job-observation.md |
| (tool tiers) | Editor = offline files, Inspector = live game via DiaDebugServer, Visual Debugger = in-game overlay | This is an Inspector |

## Open Design Questions

1. ~~**Frame retention.**~~ **Resolved 2026-11-10:** a long coarse ring (see Architecture).
2. **Sections when profiling is off.** Show only module-level data (always on) and offer one click to turn the ApplicationFlow category on, or make sections part of the always-on table? Proposal: module level only until enabled.
3. **Unequal frame rates.** RenderPU ticks at 60 Hz in the mockup; the lane model must use a time-aligned x axis, not a frame-aligned one. Workers have no frame rate at all, so the Workers view requires this.
4. **Encoding (2026-11-12).** DiaWebSocket supports binary frames (`SendBinary`), but the DebugServer protocol is JSON. Send perf arrays as binary frames referenced by a JSON envelope, or as base64 inside JSON? Proposal: decide in plan task 0 by measuring the encode cost against SD-PERF-7.
5. **Live cap changes.** Should the Inspector be able to change the deferred-step worker cap M live (as the mockup's what-if does), or only through `.diaapp`? Proposal: read-only for now; the what-if stays a client-side estimate.
6. **Sections per range.** Show sections summed over all ranges of a step (as the mockup does), or per range? Proposal: summed, with per-range on demand from tier 3.
7. **Placement.** A separate Workers tab (as the mockup does) or rows under the Frames lanes? Proposal: a tab, because a step's kick → apply span covers several keys and needs a longer time axis.
