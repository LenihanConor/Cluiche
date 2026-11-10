# Feature Spec: Perf Inspector

**Parent:** @docs/specs/applications/dia/systems/diaobservation/diaobservation.md
**Amends / depends on:** @profiling-frame-instrumentation.md (Amendment 2026-11-09, A1-A4); @perf-live-control.md (switches, markers, latency)
**Mockup:** @docs/mockups/perf-inspector-mockup.html (approved 2026-11-09)
**Status:** `Approved` — 2026-11-09

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

## Non-Goals

- In-game rendering of any of this (an in-game card costs frame time; see the S-1 incident).
- GPU timing, memory or allocation tracking (profiling spec v2 hook; later).
- Cross-run baseline diffing and budgets in CI (separate feature).
- Editing profile categories at runtime beyond on/off.

## Architecture

- **Plugin:** `DiaPerfInspector` (Dia-level Inspector plugin, same pattern as DiaApplicationFlowInspector: C++ plugin + `UI/` TypeScript). It lives in Dia because any game can use it (layer rule).
- **Bridge (DiaDebugServer amendment):** the ObservationBridge gains these channels. All are pull plus aggregated push, never per-frame push:
  - `perf.window {seconds}` (query): per unit, per frame: tick ms and per-module ms (fixed point 0.01 ms, arrays), event markers, trips. Served from the in-process rings. Target response under 120 KB for 60 s at 30 Hz.
  - `perf.timing` (push, 10 Hz): latest per-unit and per-module EWMA/p95 so the table stays live without re-pulling the window.
  - `perf.trip` (push, event): the trip record when written.
  - `perf.frame {unit, frame}` (query): the section breakdown for one frame from the unit's ring. Sections are only recorded while profiling is on; if off, the response says so and the UI offers "enable profiling".
- **Export:** the plugin builds the Chrome-trace JSON client-side from `perf.window` plus `perf.frame` results; no engine change.

## Acceptance Criteria

| ID | Criterion | Verification |
|----|-----------|--------------|
| AC1 | Connected to a running CoW S-1 stage, the lanes show all three units with the budget line at the unit's target | UI test on a recorded fixture + E2E smoke |
| AC2 | Module bars on a lane sum to the unit's tick ms within 1 ms (remainder shown as "unattributed") | Unit test on the lane model |
| AC3 | A trip record written by the engine appears in the trips list within 1 s and clicking it selects its first frame | E2E (force a trip with a sleeping test module) |
| AC4 | Selected-frame detail lists modules and sections in schedule order with ms; when profiling is off it says sections are unavailable | UI test |
| AC5 | The cross-thread chain shows the S-1 card path (GetJSONState, pack, parse, push) with stream bytes and data age, and its total as a share of one budget | UI test on fixture; E2E reads the same chain from the live game |
| AC6 | Viewing a 60 s window adds under 0.3 ms per frame to the game's Main tick (bridge cost, plugin attached vs detached) | Perf test |
| AC7 | `perf.window` for 60 s at 30 Hz with 3 units and 13 modules is under 120 KB | Test |
| AC8 | Exported Chrome-trace JSON loads in Perfetto and shows per-unit tracks and module slices | Manual check recorded in notes + schema test |
| AC9 | Disconnecting the editor leaves the game's behaviour unchanged and removes any A/B pauses (see @perf-live-control.md) | E2E |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Bridge: `perf.window`, `perf.frame`, `perf.timing` push, `perf.trip` push in DiaDebugServer | AC6, AC7 | Planned | sonnet | Dia; needs profiling amendment A1-A3 |
| 2 | Plugin shell + lane model (pure TS) + timeline lanes | AC1, AC2 | Planned | sonnet | Follow DiaApplicationFlowInspector |
| 3 | Module table, selected-frame detail, trips list | AC3, AC4 | Planned | sonnet | |
| 4 | Cross-thread chain + stream edges | AC5 | Planned | sonnet | Needs trace-linked stream writes from @perf-live-control.md |
| 5 | Latency + self-cost panels | - | Planned | sonnet | Needs live-control latency probes |
| 6 | Chrome-trace export | AC8 | Planned | haiku | |
| 7 | E2E + gap audit + attach/detach cost | AC9, AC6 | Planned | sonnet | Final verification task |

## Binding Decisions

| ID | Decision | How it applies |
|----|----------|----------------|
| SD-PERF-2 | Telemetry never writes at tick rate | The bridge pushes aggregated timing at 10 Hz; everything else is pull or event |
| SD-PERF-4 | The view lives in CluicheEditor (Inspector tier), never in-game | Rendering cost stays out of the measured process |
| (tool tiers) | Editor = offline files, Inspector = live game via DiaDebugServer, Visual Debugger = in-game overlay | This is an Inspector |

## Open Design Questions

1. **Frame retention.** The unit ring holds 128 frames (profiling amendment A2), about 4 s at 30 Hz, but the mockup shows 10-60 s. Option: a longer coarse ring (per-frame unit ms + module ms only, 2 bytes each; 60 s at 30 Hz is about 50 KB per unit) alongside the detailed 128-frame ring. Proposal: yes, a long coarse ring.
2. **Sections when profiling is off.** Show only module-level data (always on) and offer one click to turn the ApplicationFlow category on, or make sections part of the always-on table? Proposal: module level only until enabled.
3. **Unequal frame rates.** RenderPU ticks at 60 Hz in the mockup; the lane model must use a time-aligned x axis, not a frame-aligned one.
