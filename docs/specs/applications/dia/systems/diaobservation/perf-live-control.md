# Feature Spec: Perf Live Control (A/B switches, event markers, latency probes)

**Parent:** @docs/specs/applications/dia/systems/diaobservation/diaobservation.md
**Depends on:** @profiling-frame-instrumentation.md (Amendment 2026-11-09, A1-A3). Consumed by @perf-inspector.md
**Mockup:** @docs/mockups/perf-inspector-mockup.html (approved 2026-11-09)
**Status:** `Done` — 2026-11-16 (tasks 1-5, 7, 8 built; task 6 spike recorded, AC10 deferred; AC9 measures Sim receive to snapshot published only, Main/websocket hop not wired). Plan: @perf-live-control.plan.md. Approved 2026-11-09. **Revised 2026-11-10** after the plan audit: pause ownership and delivery decided (Open Questions 4, 5); trace-linked stream writes (Goal 4 linking, AC10, task 6) deferred to a follow-up feature on the backlog.

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
   **Spike result (2026-11-16, task 7, code reading only; write cost is estimated, not benchmarked).**
   - **The option framing was wrong.** `FrameStreamStore<T>` keeps per-slot metadata (`timestampUs`, `hasData`) in its own `Slot` struct, outside the payload `T`. A header word goes in that one template, so it touches no payload struct and none of the ~90 stream users. `Event<T>` already carries an envelope (`sequence`, `timestampUs`) the same way. SD-PERF-6 holds: ids ride the store envelope, not the payload.
   - **Option B (side table) is the worse choice.** It duplicates the 8-slot ring's bookkeeping and needs its own synchronisation and a lookup per write, when the slot already holds the write's metadata.
   - **Recommendation: Option A, in the store.** Add to `Slot`: `traceId`, `spanId` (the writer's open span), and `writeSteadyNs`. That is 24 bytes per slot, against payloads such as `DomainStateSnapshot` at ~128 KB (about 1 MB for 8 slots). Writing reads one `thread_local` and, when the tracer is started, one steady-clock read (~20-30 ns, estimated). With the tracer off the extra cost is a flag test. The write already copies the whole payload, which dominates.
   - **Reader side.** `FetchLatest()` returns `const T*`, so the link needs a second accessor (for example `FetchLatestLinked(StreamLink&)`, or `LastLink()` on `StreamReader`). Plain `FetchLatest()` stays unchanged.
   - **`Tracer` needs a small extension, needed under either option.** Trace state is `thread_local` (`tCurrentTraceId`, `tOpenStack`), and a span opened with an empty stack always becomes a new root. Recommendation: add `linkTraceId`/`linkSpanId` to `SpanRecord` (a flow link, not a parent), so each thread keeps its own span tree and `dia perf convert` can emit Perfetto flow events. Forcing the reader span to adopt the writer's trace as its parent would clash with the reader's own open stack (Main's module scopes).
   - **Today's S-1 chain has no trace zones.** `DomainStateSnapshotPublisher::Update` has only `DIA_PROFILE_SCOPE`, and `GetJSONState` and `PushDomainStatesToPanel` are not traced. The feature also adds `DIA_TRACE_ZONE`s there.
   - **Edge stats.** `FrameStreamDiagnostics` today is only the once-per-stream "no data" warning, so every edge stat (bytes, rate, data age, write and read cost) is new. Per-stream counters belong in the store (relaxed atomics).
   - **Scope.** FrameStream first (it is the S-1 chain). `EventStream` follows through its existing envelope. `ServiceStream` is not examined and stays out of scope until needed.
   - **Open for the feature spec:** cost under SD-PERF-7 once measured (the first task should be a write-path benchmark); behaviour when `Write` runs with no open span (link absent); whether `Debug`-only gating is wanted for the link fields.
2. **What does "paused" mean for a module that publishes a stream each frame?** Readers may log "no data" warnings. Proposal: a pause records a marker and silences those warnings for paused writers.
3. **Switch granularity for domains.** Pause the domain's snapshot only, or also its `OnCommand`? Proposal: snapshot only, so the panel stays usable.
4. ~~Who owns a pause set without a DebugServer client?~~ **Resolved 2026-11-10:** a local owner, released on stage exit, editor disconnect and shutdown (Goal 5).
5. ~~How does a pause set on Main reach modules on other PUs?~~ **Resolved 2026-11-10:** a `FrameStream` snapshot published by Main (Goal 5).
