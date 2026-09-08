# Scaling Audit — DiaApplicationFlow Event Stream

**Date:** 2026-05-17
**Question behind the question:** *Will this design scale from 2 streams (today) to a real game's worth (30–100 streams across many PUs, modules, and event types)?*
**Companion to:** [audit.md](audit.md) — that audit answered "is it OK as-is?"; this one answers "where does it break as it grows?"

## Bottom line

**Yes for current scale (2 streams, 1 reader each, low frequency). No for a production game.**
The first walls are not memory or rewind — they are **hard caps, topology invisibility, and silent type aliasing**. These start hurting in the 10–30 stream range and become blocking in the 30+ range.

## What you didn't ask

Your seven questions covered: functional adequacy, configurability, debug, memory, rewind, disk, modularity. They didn't cover the dimensions that determine whether the design carries to production:

| # | Unasked dimension | Why it matters at scale |
|---|------------------|-------------------------|
| A | **Hard caps** (`kMaxStreams=16`, `kMaxReaders=8`) | A real game easily declares 30+ streams. `kMaxStreams=16` is the **first hard wall** — `Application.h:107`. |
| B | **Topology visibility** | At 30+ streams, "who writes/reads what" stops fitting in anyone's head. There is no manifest entry, no dataflow graph, no startup validation that every reader has a writer. |
| C | **Type-aliasing risk × stream count** | `static_cast<EventStreamStore<T>*>` is unchecked (`EventStreamWriter.h:70`). At N=2 streams, collision is theoretical. At N=30+, "two devs picked the same StringCRC" or "T changed shape and one TU didn't recompile" becomes inevitable. |
| D | **Throughput / lock contention** | `Send` holds a per-store mutex while iterating every reader (`EventStreamStore.h:127`). High-frequency streams (physics contacts, particles, animation events) serialize all producers behind the slowest reader's buffer churn. |
| E | **Compile-time scaling** | Every distinct `T` instantiates a new `EventStreamStore<T>` in every TU that includes the headers (header-only templates). 30+ event types × N including TUs = compile-time tax that grows superlinearly. |
| F | **Stage / lifetime scoping** | All stream stores live for the lifetime of `Application`. There is no "stream scoped to a stage." As stages multiply, dead per-stage streams accumulate alongside live ones. |
| G | **Determinism & ordering** | No sequence numbers, no cross-stream ordering, no per-event timestamp in the envelope. At scale you cannot reproduce "what arrived in what order" — relevant for replay, networking, and bug repro. |
| H | **Backpressure signalling** | Drop-oldest is the only policy and it logs a warning. At scale, log spam under burst becomes the *de facto* monitoring channel. There is no signal back to the producer. |
| I | **Failure-mode propagation** | If a writer's PU dies or stalls, readers see silence — no "stream stale" or "writer unhealthy" semantic. With 30+ streams, silent failures are very hard to attribute. |
| J | **Topology evolution friction** | Adding a stream = code edit (declare handle, edit `OnConnectStreams`). Manifest doesn't model streams, so dataflow changes don't show up in spec/review/diff. At scale this fights you on every feature. |

## Where it breaks first — ranked

1. **`kMaxStreams = 16`** — `Application.h:107`. Hard cap, asserts in debug, returns null in release. **First wall.** A modest game (input, UI, audio, network ingress/egress, save events, achievement events, AI signals, animation events, particle events, scene-load events) is already at 10–15.
2. **Topology invisibility** — no manifest entry, no graph, no validation. Becomes painful around 10 streams; becomes blocking around 30.
3. **Type-aliasing UB** (`static_cast` without runtime type check) — probability rises with stream count and team size. One miss = silent UB, no log, no assert.
4. **`kMaxReaders = 8`** — `EventStreamStore.h:24`. Hits when you have a "broadcast" stream (e.g. lifecycle events, input). Returns `-1` and `IsConnected()` quietly returns false — silent.
5. **Lock contention on high-frequency streams** — physics contacts, particle systems, animation events at 60 Hz × 100s of events/frame = `Send` mutex becomes the hot lock.
6. **Compile-time scaling of `EventStreamStore<T>`** — header-only inline templates instantiate per TU. At 30+ event types, build times degrade.
7. **Memory under high-cardinality types** — flagged in audit Q4. Not first to break, but accelerates if events embed full `StringCRC` (68 B each), names, or paths.
8. **Debug observability** — already flagged as a Fix in audit Q3. Becomes blocking when you can no longer hold the dataflow in your head.
9. **Stage scoping & determinism** — quality-of-life issues that compound but don't crash; matter for replay and networking.

## What scales fine

- The **core ring-buffer + per-reader fan-out model** itself is sound — Disruptor-class designs work this way.
- **StringCRC identity** is fine *if* the wiring is checked (manifest validation / startup graph check).
- **Connect-at-startup** is the right choice — runtime subscribe under threaded PUs is a worse problem.
- **Drop-oldest with warning** is fine for input-class streams; it's the wrong default for lifecycle/correctness streams, but that's per-stream policy, not a model issue.

## Implication for spec direction

The audit's priority list (debug streams, type-tag check, overflow policy, replay, extraction) is still right but the **ordering changes** when scaling is the real driver:

| Scaling-driven priority | Audit item | Reason |
|------------------------|------------|--------|
| 1 | **Type-tag check + StringCRC collision detection** | Eliminates the silent-UB class. Cheap. Required before stream count grows. |
| 2 | **Manifest-declared topology + startup validation** | Adds a new dimension *not in the audit* — make `.diaapp` declare streams (id, T, kind, writers, readers, capacity, policy). Validator rejects orphans, type mismatches, cycles. **This is the single biggest scaling lever.** |
| 3 | **Raise / parameterize hard caps** | Trivial fix; do it alongside (2). Should derive from manifest, not be a magic constant. |
| 4 | **Per-stream overflow policy + envelope metadata** | Audit item 3. Promoted because it unblocks correctness streams (lifecycle, network) that can't tolerate silent drop. |
| 5 | **Debug streams (lifecycle events + tap)** | Audit item 1. Important but downstream of (2) — easier to build a tap once topology is declared. |
| 6 | **Replay / rewind** | Audit item 4. Same — easier once events carry sequence + timestamp from (4) and topology is declared from (2). |
| 7 | **Extraction to `DiaStreams`** | Audit item 5. Still last; only worth it if a non-AppFlow consumer materializes. |

## One-paragraph answer to the high-level question

The current event stream is a **good kernel** wrapped in **fixed-size scaffolding**. The kernel (typed per-reader rings, connect-at-startup, StringCRC IDs) will carry to a production game; the scaffolding (`kMaxStreams=16`, no topology declaration, no type tags, no envelope metadata, no per-stream policy, parallel debug infra) won't. The cheapest, highest-leverage scaling move is **declaring stream topology in the manifest and validating it at startup** — that one change unlocks lifting the caps, catching type mismatches, generating debug taps automatically, and making replay tractable. Without it, every other improvement gets re-implemented per-feature.
