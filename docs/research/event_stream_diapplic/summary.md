# Event Stream — Decision Summary

**Date:** 2026-05-17
**Folder:** docs/research/event_stream_diapplic/
**Inputs folded in:** [explore.md](explore.md), [audit.md](audit.md), [scaling.md](scaling.md), [product.md](product.md)

## One-paragraph picture

DiaApplicationFlow ships a typed event stream (`EventStreamStore<T>` + writer/reader handles) that works for today's two streams (`InputToSim`, `SimToUI`). The kernel is sound — typed channels, per-reader rings, connect-at-startup, StringCRC IDs. The scaffolding is not — fixed caps (16 streams, 8 readers), no manifest topology, no type-tag check, no envelope metadata, no per-stream policy, parallel debug infrastructure, six of seven product personas locked out. The cheapest, highest-yield move on every axis (engineering, scaling, product) is to **declare stream topology in the `.diaapp` manifest**; that one change unlocks lifting caps, catching type mismatches, generating editor views, enabling telemetry, and making replay tractable.

## Buckets

### SHOULD DO

Items where the cost is low or moderate and *not doing them* blocks downstream product/engineering work. Do these before stream count grows or before CluicheEditor takes on stream-related work.

| # | Item | Why now |
|---|------|---------|
| 1 | **Manifest-declared stream topology + startup validation** | Single biggest leverage point. Makes streams visible to editor, designer, telemetry, validator. Prerequisite for items 2, 4, and most of "Good to do." Drives `kMaxStreams` from data, not a constant. Top of product, top of scaling. |
| 2 | **Raise / parameterize hard caps (`kMaxStreams=16`, `kMaxReaders=8`)** | First wall a real game hits. Bundle with (1) — caps come from manifest. Trivial in isolation; pairs naturally with topology work. |
| 3 | **Type-tag check on `IStreamStore` lookup + StringCRC collision detection** | Removes a silent-UB class. Cheap. Risk rises with stream count and team size — fixing later costs the same but with a brand-damage incident attached. Prevents the "Cluiche is unstable" review. |
| 4 | **Lifecycle events on a well-known stream** | `ModuleStateChanged`, `StageTransitionStarted/Committed`, `RollbackAttempted`, `ShutdownRequested`. Turns observability from polling into subscription. Foundation for cross-game taxonomy, editor live view, telemetry, debug taps. Currently the biggest functional gap. |

### GOOD TO DO

Items with clear value that chain off "Should do" and unlock concrete capabilities, but aren't blocking today.

| # | Item | Depends on / Unlocks |
|---|------|---------------------|
| 5 | **Per-stream overflow policy** (drop-oldest / drop-newest / block / fail-loud) + **envelope metadata** (timestamp, sender StringCRC, sequence number) | Lets correctness streams (lifecycle, network, save) opt out of silent drop. Envelope is a prerequisite for replay, networking, and ordered debug view. Build with (1) so policy lives in manifest. |
| 6 | **Stream tap / type-erased iteration API** | Lets `DiaDebugServer`, an editor inspector, a telemetry sink, or a recorder iterate any stream without knowing `T`. Required for editor live event view and telemetry tap. Cheap once (1) exists. |
| 7 | **Delete v1 `MessageBus`** | Already superseded; carries no production traffic. Removing it is pure onboarding/DX hygiene — eliminates "which is the real one?" confusion for every new dev. |

### OK TO DO

Items worth doing eventually, but the case for *now* is weak. Revisit when a concrete user materializes.

| # | Item | When it earns priority |
|---|------|----------------------|
| 8 | **Replay / rewind subsystem** | When a player-facing or QA-facing feature requires it (esports clip, "share your bug", deterministic netcode). Builds on (5) and (6) — don't start before them. Disk persistence belongs here, not as a memory tactic. |
| 9 | **Extraction to a `DiaStreams` module** | When a non-DiaApplicationFlow consumer of streams shows up, or when (6) lands and the registry needs to be passed around without `Application&`. Until then, the coupling is load-bearing for thread safety, not incidental. |
| 10 | **Editor live event inspector + dataflow graph** | An obvious product win, but is an editor feature — needs an HTML mockup gate per project rule before any spec. Sequence after (1) and (6). |

### WOULD NOT RECOMMEND NOW

Items that look attractive but solve the wrong problem, or where the cost outweighs the benefit at current scale. Re-evaluate only if the underlying assumption changes.

| # | Item | Why not |
|---|------|---------|
| 11 | **Disk persistence to "save memory"** | Disk doesn't solve a memory problem (current footprint ~23 KB). The live ring needs to be in memory anyway. Disk is interesting for replay (item 8), not for memory. Asking for it now is a sign the memory question was a proxy for the replay question. |
| 12 | **Same-PU lightweight stream variant** | `Dia::Core::Architecture::Observer` already exists for in-process, single-thread fan-out. Adding a third mechanism splits the design space without solving a felt problem. |
| 13 | **Lock-contention / `Send` mutex optimization** | Not the bottleneck at any current or near-future stream frequency. Premature optimization until a high-frequency stream (physics contacts, particles) exists and measures hot. |
| 14 | **Compile-time / template-instantiation work** | Real concern at 30+ event types but not measured today. Worth a flag, not a fix. |
| 15 | **Runtime subscribe (mid-stage attach)** | DiaApplicationFlow v2 deliberately closed this door for thread-safety reasons. Reopening it costs a lot for a use case (debug attach mid-run) that taps + lifecycle events solve more cleanly. |
| 16 | **Reactive-stream / Rx-style operators** | Heavy, non-idiomatic for the engine, and unrelated to any current product need. |

## Suggested first move

If only one thing happens next, it should be **a feature spec for manifest-declared stream topology + cap parameterization + type-tag check** — items 1, 2, 3 bundled. They share scope, they share validator code, and together they convert the bus from a code-only artifact into a piece of `.diaapp` that the rest of the platform can build on. Lifecycle events (item 4) is a natural follow-up spec because once topology is declared, "the engine itself emits these" is a one-line manifest entry.

## What this answers vs. what it doesn't

**Answered:**
- Is it good enough today? (Yes, for two streams.)
- Will it scale? (No, walls at 10–30 streams.)
- What does this mean for the product? (Six personas locked out; manifest topology is the unlock.)
- What should we do next, in what order? (Items 1–4, then 5–7, then evaluate.)

**Still open (would need a spec to decide):**
- Exactly which lifecycle events ship in the v1 baseline taxonomy.
- Manifest schema for streams (single block? per-PU? typed import?).
- Where envelope metadata lives — in `EventStreamStore<T>` itself, or in a wrapper `Event<T>`.
- Whether type identity uses C++ `typeid`, a registered StringCRC type-id, or both.
- Whether to bundle items 1–3 in one spec or split.

These are spec-level decisions, not research-level — the next step is `/spec-feature` on the bundled SHOULD DO items.
