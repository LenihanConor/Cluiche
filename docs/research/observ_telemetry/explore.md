# Research: Explore — Engine Observability / Telemetry

**Session date:** 2026-05-17
**Folder:** docs/research/observ_telemetry/

## Problem Space Overview

"Engine observability" is the set of capabilities that let you understand, while the engine is running, *what it is doing, how fast, and why*. It is the union of three traditionally separate disciplines: **logging** (discrete events with severity and channels), **metrics** (numeric counters and gauges sampled over time), and **tracing** (durations and causal chains of work, usually scoped). In a game engine these are not luxuries — they are the substrate that frame-time bugs, hitch hunts, memory regressions, threading anomalies, and "why did the asset not load?" investigations all run on top of. Without it, every regression is a manual binary search in a debugger.

Cluiche today has a respectable but uneven set of building blocks. `DiaLogger` is a fully designed logging spine (per-thread buffers, sinks, channels, level filters, async dispatch). `DiaApplicationFlow::MetricsCollectorModule` collects per-PU FPS/frame-time and a coarse memory gauge. `DiaDebugServer` exposes a structured WebSocket protocol for subscriptions, queries, and commands, and already broadcasts `ServerStats` and core metrics. `DiaStateMachine::StateMachineTracer` traces individual state machines. What is missing is the **glue and the contracts**: there is no engine-wide event taxonomy, no timing/scope primitive (no `ScopedTimer`, `ZONE`, or trace span), no counters/gauges API beyond hand-rolled fields, no per-frame snapshot a sink can subscribe to, no central place where "the engine produces signals X, Y, Z" is declared. The visible result is that observability today exists in fragments — each subsystem grows its own status struct (see `ServerStats`, `MetricsSnapshot`, `StateMachineTracer`, `MessagesDropped` counters scattered through DebugServer), and consumers (editor panels, log files, the WebSocket UI) each adapt to the fragments individually.

The strategic question is not "should we have telemetry?" — we already do, partially. It is **whether to standardise the contracts now** (a single instrumentation API every module uses, a typed event/metric stream, a frame snapshot model) so that future modules ship with consistent observability for free, or to keep growing the per-module pattern and pay the integration tax repeatedly. The decision rhymes with the event-stream research conclusion: the kernel (logger, metrics module, debug server) is sound; the scaffolding (declarative taxonomy, scope primitives, snapshot model, sink-side aggregation) is what unlocks the next ten subsystems.

## Existing Approaches

- **Tracy / Optick / Pix** — sampling + scoped instrumentation profilers. `ZoneScoped`/`ZONE_NAMED` macros around blocks of code, async-safe rings, GPU markers, network protocol to an external viewer. Industry-standard for AAA frame analysis.
- **Telegraf / StatsD / OpenTelemetry counters** — counters, gauges, histograms emitted at fixed intervals; aggregation lives in the receiver (Prometheus, InfluxDB). The C++ side is just `counter.Inc()` / `gauge.Set()`.
- **OpenTelemetry tracing (spans)** — explicit `Span` start/end with parent linkage, emitted to a collector. Same model is what game engines call "frame markers" or "zones" but with parent IDs.
- **chrome://tracing JSON / Perfetto** — `{ "ph": "B" }` / `{ "ph": "E" }` event log dumped to JSON, viewed in a browser flamegraph. Cheap to produce, viewers are free.
- **In-game stat overlays (id Tech `condump`, Source `cl_showfps`, Unreal `stat unit` / `stat scenerendering`)** — toggleable HUD layers that show counters and timings. Pulled from a single registry the engine populates.
- **Per-frame ring of structured events** — engine writes a typed event log into a ring per frame, debug UI reads it out. (Closest match to what `DiaApplicationFlow` event streams already are — the question is whether *every* notable engine event flows through the same channel.)
- **Sinks-based logging (spdlog, Boost.Log)** — log entries fan out to multiple sinks with per-sink filters. `DiaLogger` already implements this.
- **Counter registry pattern** — every counter registers itself by name with a singleton registry, sinks iterate the registry per tick. Simple, ships in dozens of engines.
- **Causal tracing / "request flow"** — assign a context ID at the entry point (frame number, asset request ID, input event ID), thread it through every layer, log/trace events under that ID. Lets you reconstruct "what happened to *this* asset request" across modules.
- **Black-box / flight recorder** — the last N seconds of telemetry in a fixed-size ring; on crash or assert, the ring is dumped. Always-on, low-overhead.
- **Probe / introspection bus** — the engine exposes a typed query surface ("give me the current stage", "list active modules", "snapshot of active jobs"); a debugger client polls or subscribes. `DiaDebugServer::IDebugStateProvider` + `QueryRegistry` is exactly this pattern.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| Push vs pull | Push (subsystem emits events) / Pull (sink polls registry) / Hybrid | Logger is push, MetricsCollector is push-then-snapshot, DebugServer is poll-then-broadcast. Hybrid is the realistic answer. |
| Instrumentation primitive | Logs only / Logs + Counters / Logs + Counters + Spans / All four (logs, counters, gauges, spans) | Each level adds API surface but solves a distinct class of question. |
| Counter storage | Per-thread + reduce / Atomic shared / Per-PU + snapshot | Game engines usually prefer per-thread + reduce on snapshot to avoid contention on hot paths. |
| Scope/zone API | Macro-based (`ZONE("Render")`) / RAII object / Manual Begin/End | Macros win on call-site brevity and conditional compile-out. |
| Event taxonomy | Free-form strings / Registered StringCRC IDs / Typed structs per event | Cluiche's Stringtype already pushes us toward StringCRC; typed structs add safety but cost API surface. |
| Snapshot model | Push to subscribers / Sink polls per tick / Both | Determines whether "no subscribers" means "no work done". |
| Storage model | In-memory ring only / Ring + flush to disk / Ring + WebSocket push only | DebugServer already supplies WebSocket; disk flight-recorder is a separate concern. |
| Aggregation site | In-process (engine pre-aggregates) / In-sink (sink reduces) / Out-of-process (collector) | In-process aggregation is cheaper but less flexible. |
| Per-frame vs continuous | Per-frame snapshot (one record per tick) / Continuous stream (ungrouped events) / Both | Per-frame is cheaper to render; continuous is needed for sub-frame timing. |
| Compile-time gating | Always on / `#ifdef` debug / Per-channel / Severity threshold | Logger has channel + level threshold today. Spans usually need a separate macro for hot paths. |
| Module consumption | Each module owns its own metrics / Shared registry / Both | Today each module owns its own. Shared registry is the unlock. |
| Editor integration | Custom protocol per module / Generic stream over DebugServer / Generic stream + typed schema | DebugServer already exists; the question is whether telemetry flows through it generically. |

## Known Tradeoffs

- **Always-on overhead vs after-the-fact insight.** A flight recorder needs continuous capture; a profiler can be opt-in. Always-on capture costs cycles every frame whether or not anyone is looking.
- **Push fidelity vs sink survival.** A high-frequency producer (per-particle, per-physics-contact) will overwhelm any naive sink. Either the producer rate-limits, the channel has overflow policy, or sinks are coarsened to aggregates.
- **Typed structs vs schemaless.** Typed events catch mistakes at compile time and document the engine's behaviour. Schemaless events are cheap to add but require every consumer to know the keys.
- **Per-thread aggregation vs single shared atomic.** Per-thread is faster but reads are stale until reduce; atomics are simpler but hot-path contention scales with thread count.
- **In-process pretty output vs raw firehose.** Pretty output (formatted strings, JSON) is what humans want; raw firehose (ints, IDs, deltas) is what tooling wants. You usually need both.
- **Engine-defined taxonomy vs free-form.** Free-form scales as fast as developers add events; engine-defined taxonomy gives consumers a contract but is a bottleneck for adding new instrumentation.
- **Time source coherence.** A trace span across PUs is meaningless if each PU uses its own clock. Choosing one monotonic time source (e.g. `std::chrono::steady_clock::now()` everywhere) is non-trivial when modules pre-date the standard.
- **Ring size vs hitch survival.** A small ring is cheap but loses data during long pauses; a big ring buys survival but eats RAM. Per-PU rings are the usual middle ground.

## Known Pitfalls (C++ / game engine context)

- **`printf`-style format calls that allocate** — most engines forbid runtime allocation in hot logging paths. `DiaLogger::Log(level, channel, fmt, ...)` formats into a fixed `char message[1024]`; this is correct but worth preserving.
- **Counters that drift between threads.** Per-thread counters reduced once per frame are correct *only if* the reduce is synchronised against the producer. A naïve `Sum()` without a fence reads stale values.
- **Macros that aren't `do { } while (0)`** — common bug: `if (cond) ZONE("x");` works in release but not when `ZONE` expands to multiple statements with no scope.
- **Scoped zones across `co_await`** — RAII zones break across coroutines. C++20 enables this; the engine doesn't use coroutines today, but the design should not be hostile to them.
- **Recording a string in the trace.** Always record a `StringCRC` or interned ID, never the raw `const char*` — pointers may dangle after the originating module unloads.
- **Time source choice.** `GetTickCount`, `QueryPerformanceCounter`, `std::chrono::steady_clock` differ in monotonicity guarantees. Using `system_clock` is wrong (jumps on NTP).
- **Telemetry that disappears in release.** Compile-time gating that strips *all* observability from release builds is what produces "we can't repro it on the QA build" stories. Keep at least metrics + warnings on in release.
- **Mutex-protected counter increments.** A spin-locked counter on a 10kHz hot path is the textbook 30% frame-time regression.
- **Sink panics that block the producer.** A WebSocket sink that blocks on a slow client must not back-pressure the producer thread. Per-thread buffer with drop-on-overflow is the correct shape (Logger has this; tracing should too).
- **Cycle counting vs wall-clock.** TSC-based timing is fast but unreliable across cores on older CPUs; wall-clock is reliable but expensive. Game engines usually use QPC (QueryPerformanceCounter on Windows) which is what `std::chrono::steady_clock` resolves to.
- **Telemetry before the logger is up.** Static-init counters have no logger to flush to; a deferred-startup phase needs to capture and replay.
- **Span IDs as 64-bit integers.** Generating monotonically increasing IDs on multiple threads needs an atomic. A per-thread id-prefix + per-thread counter avoids contention but requires care.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaLogger | The logging half is done — channels, sinks, per-thread buffers, level filters. Telemetry can ride on the same sink/channel pattern. |
| DiaApplicationFlow::MetricsCollectorModule | The skeleton of "the engine collects per-PU metrics every tick". Currently fps + frame-time + memory; could become the registry pull point. |
| DiaApplicationFlow event streams | Already the canonical typed multi-reader bus inside an Application. Telemetry events (counter updates, span open/close) could flow through it. |
| DiaDebugServer | The transport. `ServerStats`, `BroadcastCoreMetrics`, `QueryRegistry`, `SubscriptionManager` already exist. Telemetry is its primary product. |
| DiaCore::Architecture::Observer | In-process pub/sub if telemetry needs same-thread fan-out. |
| DiaCore::CRC::StringCRC | Required for any registered counter/span/event ID. PD-001 makes this non-negotiable. |
| DiaCore::Containers::DynamicArrayC | Fixed-cap array used everywhere; the right primitive for per-PU metric snapshots. |
| DiaStateMachine::StateMachineTracer | A worked example of "observability primitive owned by the module being observed". Validates the pattern. |
| DiaCore (no profiler/timer subsystem yet) | Confirmed via glob: no `Profiler`, `Trace`, `ScopedTimer`, or `Telemetry` directory exists. Greenfield. |
| DiaWebSocket | The pipe out of the process; already used by DebugServer. |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC for IDs | Counter names, span names, event IDs, channel IDs must all be StringCRC. No raw string maps in public API. |
| PD-002 ProcessingUnit/Phase/Module | Telemetry is a Module (or several). A per-PU `TelemetryModule` that owns the per-PU ring is the natural shape. |
| PD-003 Component-based entities | Mostly orthogonal — telemetry is engine-side, not entity-side — but per-component counters could exist later. |
| PD-004 No STL in public APIs | Public counter/span/event API uses `Dia::Core::Containers::*`, not `std::vector`, `std::unordered_map`, `std::string`. Internals (ring buffers, atomics) can use `<atomic>`, `<chrono>`. |
| PD-005 x64 Windows only | QPC / `std::chrono::steady_clock` are fine; no need to abstract platform timer. |
| PD-006 .vcxproj source of truth | A new `DiaTelemetry` module needs its own vcxproj + filters and entries in the registry. |
| PD-007 C++20 | `std::span` for snapshot views, `std::atomic_ref` for unaligned counters, `std::source_location` for span origin tagging — all fair game. |
| PD-008 Directory.Build.props owns OutDir | Free for new module to inherit. |
| PD-009 out/ for non-binary output | If telemetry writes a flight-recorder dump file, it goes under `Cluiche/out/<AppName>/telemetry/`. |
| PD-010 .diagame is project root | If telemetry topology (channels, sample rates, sinks) is declared, it goes in `.diagame` config — not hard-coded. |

## User-Stated Scope and Constraints

These were given verbally during the explore stage and reshape the design space materially. Treat them as binding for ideation and evaluation.

- **Goal: an observation tooling stack covering four pillars** — logs, traces, metrics, health. Logging exists (`DiaLogger`); the other three are the work. "Captures" was used as the verb covering all four (i.e. *captures* logs/traces/metrics/health) — not a fifth pillar.
- **Time horizon: single session.** Within one run of the engine: reproduce, inspect, diagnose. Long-term aggregation, persistent dashboards, and historical trend analysis are explicitly out of scope for v1 — but the system must not foreclose them. Wire formats and event schemas should be portable so a future exporter can ship without reshaping the producer side.
- **Scale up but start simple.** A v1 that supports 5 counters, 10 spans, and 1 health probe at low overhead is more useful than a v1 that supports 1000 of each but is hard to onboard. Rooms to grow (per-PU rings, per-channel sinks, sidecar transport) should be reachable, not present on day one.
- **External tooling alignment, not adoption.** OpenTelemetry / OpenMetrics are reference data models. Match their wire format — `service.name`, `trace_id`, span `start_unix_nano`, OTLP-style metric records — so a future `OtlpExporter` is a sink we add, not a refactor. Do **not** pull `opentelemetry-cpp` as a dependency; it's heavy and bringing in external runtime allocators conflicts with the engine's container conventions.
- **Sidecar vs in-process is a real architectural axis.** The user identified that today's logger imposes a runtime cost in-process. The two responses (a) make the existing transport asynchronous via a different `ISink` implementation, or (b) move the consumer side out of the engine process entirely (named pipe, shared memory, or local TCP/WebSocket to a sidecar). Both belong in ideation; the choice has long-term consequences.
- **Config is a missing piece.** The logger supports per-sink level thresholds and channel filters, but there is no surface today that lets CluicheTest say "channel X at level Y for this run" without recompiling. Configuration of telemetry (log levels, enabled trace channels, metric sample rates, health probe intervals) should live in the `.diagame` project file under `config` per PD-010, or be a typed import that the project file references. This applies to all four pillars.

## Pillar Definitions (Clarified)

The word "trace" is ambiguous in the wild. Pinning down what each pillar means in Cluiche to avoid arguing about names later.

| Pillar | What it captures | Cardinality | Lifetime | Example |
|--------|------------------|-------------|----------|---------|
| **Log** | Discrete event, with severity, channel, formatted message | Low–medium (hundreds–thousands per second peak) | One entry per occurrence | `[Info][Asset] Loaded "hero.png" in 12ms` |
| **Trace (span)** | A *duration* of work — `Begin/End` pair around a code region, optionally with parent linkage to reconstruct flow | Medium–high (every frame for hot zones) | Open until `End`; emitted as a record on close | `Span("Render::Shadows", parent=Span("Render::Frame"), 0.4ms)` |
| **Metric** | A named numeric — counter (monotonic, e.g. `frames_rendered`), gauge (sampled, e.g. `memory_used_mb`), or histogram (bucketed, e.g. `frame_time_ms`) | Bounded — counters/gauges are registered once and updated in place | Lives for the session; sampled per tick or on demand | `counter("assets_loaded") = 342` |
| **Health** | A boolean or enumerated status of a named subsystem — "is X up, degraded, or failing?" | Very low (one per subsystem, polled ~once per second) | Lives for the session; updated by the subsystem itself | `health("DebugServer") = OK; uptime=42s; reason=""` |

Two operational uses of "trace" exist:
1. **Span-style tracing** — what the table above describes; matches OpenTelemetry's `Span` model.
2. **Verbose-channel tracing** — "turn on tracing for the asset system" meaning "raise the log channel level to `Trace`/`Verbose` for that subsystem". This is a *log feature*, not a separate pillar — it is achieved by the existing `Logger::SetLevelThreshold` + channel filter.

**Decision implied by user input:** "trace" as a pillar means **span-style tracing**. The verbose-channel sense is folded into the logger, not a separate system.

## Logger Perf — Findings After Code Read

The user flagged a runtime cost from logging. After reading `Logger.cpp`, `ThreadLogBuffer.cpp`, `StdOutSink.cpp`, `DebugOutputSink.cpp`, and the `LoggerModule` host:

**Producer side is cheap.** `Logger::Log` writes to a `thread_local ThreadLogBuffer` — `vsnprintf` into a stack-allocated `LogEntry`, then a memcpy into the ring. No mutex, no allocation, no I/O. This is correct and not the bottleneck.

**Consumer side is the bottleneck.** `LoggerModule::DoUpdate` calls `Logger::FlushBuffers` *every main-PU tick*. `FlushBuffers` (a) takes the registry mutex, (b) walks every thread's ring, (c) for each entry, dispatches synchronously to every sink. The default sinks do blocking I/O **on the main PU thread**:
- `StdOutSink::OnLogEntry` → `printf` + `fflush(stdout)` per entry — two syscalls per log line.
- `DebugOutputSink::OnLogEntry` → `String1024::Format` + `OutputDebugStringA` per entry — kernel call per entry.

At a few hundred log lines per frame this is a real hit. Each entry is also ~1 KB (1024-char `message`), so ring drain is a 1 KB memcpy per entry.

**Implications for ideation:**
- The cheap fix is *not* a sidecar process. It is **drain off the main thread**: a dedicated logger thread that consumes thread-local rings and feeds sinks. Producer side unchanged. ~1–2 day change.
- A **sidecar process** buys crash-survival of the log stream, schema versioning at the IPC boundary, and zero-engine-overhead sinks — at the cost of a second process, IPC plumbing, and lifecycle management. Save for when crash-survival matters.
- A **JSON-line / structured sink** is independent of either choice and unlocks deterministic AI consumption immediately.
- `LogEntry` currently has **no timestamp field**. Adding one is a v1 schema commitment that traces and metrics will also depend on; cheap to do now, expensive once the wire format is shipped.

These findings reshape Q1 — the question is no longer "in-process or sidecar?" but "which order do we add (a) async drain, (b) structured sink, (c) sidecar?".

## Quick Wins for AI Consumption

The user explicitly framed this work as "give AI more tools to solve issues." Most observability research optimises for human eyeballs (flamegraphs, dashboards). AI consumers want different things: deterministic parseability, complete artifacts on crash, per-run identity, stable timestamps, and a single-line summary that says "this run was good or bad and why." The candidates below are individually small, but collectively they convert telemetry from "human looks at a Visual Studio output window" to "AI ingests a directory of artifacts and reasons about them."

| # | Quick win | Effort | AI-value reason |
|---|-----------|--------|-----------------|
| QW-1 | **JSON-line sink** (`JsonStdOutSink` / `JsonFileSink`) — one JSON object per log entry per line | ~50 LoC | AI parses with `json.loads()` per line. Eliminates fragile regex on `[INFO][channel] msg` format. Foundation for traces/metrics/health — same sink class with a `record_type` field. |
| QW-2 | **Per-session ID** stamped on every entry — UUID or `Cluiche.exe`-PID + start time | ~10 LoC | When AI compares run N vs run N-1, it can disambiguate. Required for any future multi-run analysis. |
| QW-3 | **Stable timestamp** (`uint64_t timestampNs`) on every `LogEntry` — captured at producer side, monotonic | ~2 LoC + schema commit | Required for cross-thread ordering, span correlation, OTel wire-compat. Cheap now, breaking later. |
| QW-4 | **Auto-dump on assert/fatal** — last N log entries + counter snapshot + stage state to `Cluiche/out/<App>/crashes/<timestamp>.json` | ~100 LoC | AI gets artifacts even when the run dies before stdout is flushed. Pairs with existing `AssertSinkBridge`. |
| QW-5 | **Final session summary** — on normal exit, write a one-line `Cluiche/out/<App>/sessions/<id>.json` with `{ exit_code, errors, warnings, frames, first_error, duration }` | ~80 LoC | One artifact per run; AI reads it before deciding whether to dig into the full log. Directly answers "what does health mean beyond exit code?". |
| QW-6 | **CLI override for log levels** — `dia run cluichetest --log-level=trace --log-channel=asset` | ~30 LoC in CLI + Logger config hook | Iteration without recompile or config edit. AI agents can drive log verbosity per investigation. |
| QW-7 | **Test step / scenario tagging** — push/pop a "current scenario step" StringCRC, every entry while active carries the tag | ~40 LoC | AI greps by step name to verify "did step 3 actually run?" Maps directly to E2E health-state capture. |
| QW-8 | **Errors-and-warnings retention ring** — separate small never-drop ring for `Warning`/`Error`/`Fatal` entries | ~30 LoC | Guarantees critical entries survive a flood of trace-level output. AI never misses an error because logs were noisy. |
| QW-9 | **Process exit-reason capture** — distinguish `normal_exit / assert / unhandled_exception / forced_terminate` and write to session summary | ~50 LoC | "did the test pass" needs more than `exit_code == 0`. Folds into QW-5. |
| QW-10 | **Counter / gauge JSON dump per session** — if metrics ship in v1, dump final counter values into the session summary | trivial once metrics exist | Closes the loop — AI sees both the qualitative log and the quantitative metric in one artifact. |

These quick wins are **not all in scope for every candidate** — but several of them are zero-design-cost additions to whatever the chosen v1 ends up being, and ideation should treat them as cheap riders.

## User-Raised Questions to Resolve in Ideation/Evaluation

These are explicit questions the user surfaced. Each must have an answer before this research closes.

- **Q1.** Should the in-process logger transport stay in-process (and become async via a thread-backed sink), move to a sidecar process, or both as deployment options? *Affects all four pillars, not just logging — whichever wins is likely to be the model for traces and metrics too.*
- **Q2.** Where does telemetry configuration live? `.diagame` `config` block? A separate `.diatelemetry` typed import? Hard-coded module defaults plus runtime override commands via DebugServer? *Likely the same answer for all four pillars.*
- **Q3.** Is "trace" in Cluiche a log feature or a span-based system? *Tentatively settled above — span-based, but confirm during evaluation.*
- **Q4.** Do we adopt OpenTelemetry/OpenMetrics SDKs or only match their wire format for future export? *Tentatively settled above — wire-compat only — but exact schema commitments belong in evaluation.*
- **Q5.** What does "health" mean in Cluiche? Per-module status enum? Per-PU heartbeat? Both? Who reports? Who consumes? *Tentatively settled: health is the **session-state artifact** — warnings, errors, E2E test step states, exit reason. Not real-time liveness probes. The unit of consumption is "more than exit code 0/1" — answers questions like "did the asset module log warnings? did the test reach step 4? was shutdown clean?" See QW-5 / QW-7 / QW-9 above.*

## Decisions Locked from User Input (Pre-Ideation)

After review of the quick-win list, the user committed to the following — these are no longer open questions, they are constraints that ideation must respect:

- **Two distinct sink types, not a converted one.** The console sink stays human-readable (`[INFO][channel] msg`); a *new* "observation sink" is what emits JSON-line. Humans look at the console; AI reads the observation sink's output. Don't replace `StdOutSink` — add alongside it.
- **Per-session directory is the unit of capture.** Building on QW-2 (session ID) and QW-5 (session summary), the user asked for "a file that is some sort of binding session details — what ran, what files created, etc." The cleanest shape is **one directory per session** at `Cluiche/out/<AppName>/sessions/<sessionId>/`, containing:
  - `session.json` — the binding manifest: run identity, app name, build version, start/end timestamps, exit reason, what stage was loaded, modules that ran, counts of warnings/errors, *and a list of every other file in this directory*. This is the AI's entry point; it can read this one file and discover everything else.
  - `log.jsonl` — the JSON-line log stream from the observation sink.
  - `crashes/<n>.json` — auto-dump on assert/fatal (QW-4).
  - `metrics.json` — final counter/gauge dump (QW-10), if metrics ship.
  - Any other artifacts (screenshots, asset cache outputs, etc.) the engine produces during the run.
- **Stable timestamp on every record** (QW-3) — confirmed; v1 schema commitment.
- **Auto-dump on assert/fatal into the session directory** (QW-4) — confirmed.
- **Final session summary written into `session.json` on exit** (QW-5) — confirmed.
- **Config is in scope for v1** (QW-6 and broader). A telemetry config surface — log levels per channel, sink enable/disable, sink output paths, scenario tagging, metric sample rates — must ship as part of v1 so the system is usable without recompiling. The user will do a **second pass** on config later (likely as part of a broader cross-system configuration story), so the v1 config should be:
  - **Minimal but real** — the smallest config surface that lets a user change log level for a run without recompiling. Not a full schema for every imaginable knob.
  - **Forward-compatible** — slot into `.diagame` `config` per PD-010, or be a typed import (`.diatelemetry` file) that `.diagame` references. Either is consistent with how the platform already extends.
  - **Overridable via CLI** — `dia run cluichetest --log-level=trace --log-channel=asset` — so iteration doesn't require a file edit.
  - **Reloadable later, not now** — v1 reads config at startup; live reload is an obvious future extension and the format must not foreclose it (i.e. don't bake config values into compile-time templates).
  - The v2 pass will likely unify telemetry config with other system configs (renderer, window, input bindings) under one schema. v1 telemetry config should look like "the telemetry slice of that future schema" so v2 is a merge, not a rewrite.
- **Test step tagging** (QW-7) — confirmed.
- **Errors-and-warnings retention ring** (QW-8) — confirmed.
- **Process exit-reason capture** (QW-9) — confirmed; folds into `session.json`.
- **Counter/gauge JSON dump per session** (QW-10) — confirmed (conditional on metrics shipping).

These constraints reshape the candidate list materially. In particular:
- The "**v1 baseline**" is now a known shape: observation sink + session directory + session.json + timestamps + exit-reason capture + crash dump. Whatever main candidate wins must produce this baseline.
- The **JSON schema for `session.json` and `log.jsonl`** is itself a design artifact that ideation needs to produce, because once it ships it becomes a compatibility surface.
- **Config is in v1 but minimal**: minimum viable config schema (log level + channel filter at least), `.diagame` integration, CLI override. A future v2 pass will unify telemetry config with other system configs. Design must not foreclose live reload, even though v1 only does startup reload.

## Open Questions for Ideation

- Should telemetry be **one module** (`DiaTelemetry`) that owns logger + metrics + tracing, or **three loosely-coupled subsystems** that share only a snapshot model?
- Is the primitive a **typed event** (struct), a **named scope** (RAII), a **counter/gauge** (atomic), or all three?
- Is the snapshot **per-frame** (one struct per PU per tick) or **continuous** (ring-buffered timestamped events)?
- Does a **flight recorder** (last-N-seconds rolling capture, dumped on crash) belong in v1 or is it follow-up?
- Do we need a **chrome://tracing JSON exporter** (cheap viewer, free tooling), or is the DebugServer WebSocket enough?
- Should **scope timers** (`ZONE("RenderShadows")`) be macro-based or RAII-class-based?
- Is the **counter registry** central (one table) or per-PU (each PU owns its counters, snapshot reduces)?
- Should telemetry events ride the **existing DiaApplicationFlow event-stream system**, or use a dedicated channel (their volume profile is different — telemetry is high-frequency, lifecycle events are low-frequency)?
- How does this **interact with the planned event-stream lifecycle taxonomy** (`ModuleStateChanged`, `StageTransitionStarted`, etc.) decided in `event_stream_diapplic`?
- What is the **always-on baseline in release**? FPS + memory + warnings always on? Spans only when a flag is set?
- Does the system need to support **GPU markers** today (PIX / RenderDoc bridge) or is CPU-only fine for v1?
- Should there be a **counter-as-metric / gauge-as-metric** distinction (counter = monotonic, gauge = sampled), or do we keep a single "named float" abstraction?
- Where does the **time source** live — a `DiaCore::Time` utility, or a dedicated `DiaTelemetry::Clock`?
- Is **causal tracing** (frame ID, request ID propagation) in scope or out?
