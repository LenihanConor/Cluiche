# Feature Spec: Perf Live Control (A/B switches, event markers, latency probes)

**Parent:** @docs/specs/applications/dia/systems/diaobservation/diaobservation.md
**Depends on:** @profiling-frame-instrumentation.md (Amendment 2026-11-09, A1-A3). Consumed by @perf-inspector.md
**Mockup:** @docs/mockups/perf-inspector-mockup.html (approved 2026-11-09)
**Status:** `Approved` — 2026-11-09. **Revised 2026-11-10** after the plan audit: pause ownership and delivery decided (Open Questions 4, 5); trace-linked stream writes (Goal 4 linking, AC10, task 6) deferred to a follow-up feature on the backlog.

## Summary

Three additions that turn measurements into answers: (1) **A/B switches** to pause a module or a debug domain in the running game and watch the numbers change; (2) **event markers** (stage entered, world regenerated, slider drag, preset loaded) recorded on the same time axis as the frames; (3) **end-to-end latency probes** that follow one action across threads (slider drag to value applied to visible). All three are cheap and rate-limited, and the switches are Debug-only.

## Problem

- Isolating the S-1 card meant rebuilding the game with and without it (A/B), about 20 minutes.
- A spike on a timeline is a mystery without knowing what happened just before it.
- The S-1 plan requires "live slider to visible change in under 100 ms" and nothing can measure it. Cost that spans two threads (Sim builds, Main parses and pushes) never appears as one thing.

## Goals

1. **Switches.** `perf.pause {kind: "module"|"domain", id}`, `perf.resume {...}` and `perf.resume_all`. A paused module skips `DoUpdate` (lifecycle, streams and registrations stay, so dependents do not hang); a paused debug domain is excluded from snapshot packing and from `GetJSONState`. All pauses are released when the DiaDebugServer client that set them disconnects, and are listed in the app report.
2. **Markers.** `DIA_PERF_MARK(name)` / `DIA_PERF_MARK_VALUE(name, u64)` write to a per-thread lock-free ring (no allocation, no I/O). Automatic markers: stage transitions, module start/stop, trips. S-1 adds markers for slider drag start/end, preset load, regenerate and size change.
3. **Latency probes.** `LatencyProbe::Begin(actionCrc)` returns a probe id; `Stamp(id, hopCrc)` records a steady-clock timestamp per hop; `End(id)` closes it. Ids travel where payloads already exist (panel command args carry `_lat`; snapshot slots already carry `simFrame`, so the visible hop is matched by "first snapshot with simFrame at or after the frame that applied the command"). A small ring of recent completions per action gives p50/p95 over the last 40.
4. **Stream edge diagnostics.** For streams on a traced chain, report bytes per write, rate, data age at read and write/read cost, reusing `FrameStreamDiagnostics`. A stream write records the active scope's trace id so the reading scope can be linked as its child (trace pillar parent/child; no new id scheme). **Deferred 2026-11-10:** the trace linking (and AC10) moves to a follow-up feature, because it may touch every stream. Task 6 becomes a design spike that records the decision (Open Question 1); the build is a backlog row.
5. **Pause ownership and delivery (2026-11-10).** Main owns the pause set. Each pause has an owner: a DebugServer client (released on that client's disconnect, through a new public `DebugServer` client-disconnect callback) or a local owner for in-process callers such as tests (released on stage exit, on loss of the editor connection, and at shutdown). The in-game panel does not pause anything in this feature. Main publishes the set as a small `FrameStream` snapshot (bitset by module index plus a generation); each PU reads it once per `Update()`, and Sim reads the same snapshot for debug-domain pauses. No mutex crosses a PU boundary.

## Non-Goals

- Pausing arbitrary modules in Release (compiled out).
- Pausing modules that own a unit's frame clock or the debug server itself (denylist: Kernel, DebugServerHost, SimQueryHost; `perf.pause` on them returns an error).
- Persisting pauses across sessions.
- Automatic regression gating (separate feature).

## Acceptance Criteria

| ID | Criterion | Verification |
|----|-----------|--------------|
| AC1 | `perf.pause` on a module stops its `DoUpdate` within one frame; its streams stay connected and dependents still start; `perf.resume` restores it | Unit + E2E (S-1: pause WorldGenerationModule, revision stops rising; resume, it rises) |
| AC2 | Pausing the WorldGen domain removes it from `debug.domain_state` and lowers the Sim tick by at least the measured GetJSONState + pack cost | E2E |
| AC3 | Denylisted modules cannot be paused (`success:false`) | Unit test |
| AC4 | Disconnecting the client that paused a module resumes it | E2E |
| AC5 | In a Release build the pause commands are not registered | Build check |
| AC6 | `DIA_PERF_MARK` costs under 50 ns and allocates nothing; a ring of 1,024 markers per thread drops the oldest on overflow and counts drops | Unit + benchmark (reported) |
| AC7 | Stage transition, module start/stop and trips appear as markers without call-site changes | Unit test |
| AC8 | A probe from `Begin` to `End` reports each hop's delta; 40 probes give p50 and p95 | Unit test with a fake clock |
| AC9 | S-1: slider drag to visible has a measured p95 compared to the 100 ms target | E2E (reports; fails above 2x target) |
| ~~AC10~~ | ~~A traced stream write links the reader's scope as a child; the chain GetJSONState to PushDomainStatesToPanel is one trace~~ **Deferred 2026-11-10** to the trace-linked streams follow-up | - |
| AC11 | A pause set by a local owner is released on stage exit and at shutdown; a module paused on Main stops updating on Sim within one Sim `Update()` | Unit test |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `PerfMarkers` ring + `DIA_PERF_MARK` macros + auto markers | AC6, AC7 | Planned | sonnet | Dia (DiaObservation) |
| 2 | Pause/resume for modules in ApplicationFlow (Debug only) + client-disconnect release | AC1, AC3, AC4, AC5 | Planned | sonnet | Dia; touches the `Module.cpp` update path |
| 3 | Pause for debug domains (registry exclusion) | AC2 | Planned | sonnet | `VisualDebuggerModule` (Baseline) / DiaVisualDebugger |
| 4 | `perf.pause`, `perf.resume`, `perf.resume_all` commands + app report listing | AC1-AC4 | Planned | sonnet | DiaDebugServer |
| 5 | `LatencyProbe` + panel-command `_lat` + simFrame correlation | AC8 | Planned | sonnet | |
| 6 | Design spike for trace-linked stream writes (Open Question 1), recorded in this spec; build deferred to a backlog follow-up | - | Planned | sonnet | Was opus build of AC10; deferred 2026-11-10 |
| 7 | CoW S-1 markers and latency E2E | AC9 | Planned | sonnet | CoW |
| 8 | Gap audit, edge tests, overhead measurement | All | Planned | sonnet | Final verification |

## Binding Decisions

| ID | Decision | How it applies |
|----|----------|----------------|
| SD-PERF-2 | Telemetry never writes at tick rate | Markers go to a ring, not the log; probes keep a small ring of completions |
| SD-PERF-5 | A/B switches are Debug-only and session-scoped | Compiled out of Release; released on disconnect |
| SD-PERF-6 | Trace ids ride existing payloads; no new wire format on streams | `_lat` in command args, `simFrame` correlation for snapshots; stream trace linking uses the trace pillar's ids |
| (layer rule) | Reusable capability belongs in Dia | Markers, probes and switches are engine; S-1 marker call sites stay in CoW |

## Open Design Questions

1. **Trace-linked stream writes (task 6).** FrameStream slots are fixed payload structs; linking needs either a header word on every slot (touches all streams) or a side table keyed by (stream, write sequence). The side table avoids payload changes but adds a lookup per write. Needs a small design spike before dispatch.
2. **What does "paused" mean for a module that publishes a stream each frame?** Readers may log "no data" warnings. Proposal: a pause records a marker and silences those warnings for paused writers.
3. **Switch granularity for domains.** Pause the domain's snapshot only, or also its `OnCommand`? Proposal: snapshot only, so the panel stays usable.
4. ~~Who owns a pause set without a DebugServer client?~~ **Resolved 2026-11-10:** a local owner, released on stage exit, editor disconnect and shutdown (Goal 5).
5. ~~How does a pause set on Main reach modules on other PUs?~~ **Resolved 2026-11-10:** a `FrameStream` snapshot published by Main (Goal 5).
