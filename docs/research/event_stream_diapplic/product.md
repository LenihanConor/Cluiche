# Product Perspective — DiaApplicationFlow Event Stream

**Date:** 2026-05-17
**Companion to:** [audit.md](audit.md), [scaling.md](scaling.md).
**Question this answers:** *Beyond "is the code OK" and "will it scale" — what does this mean for the platform as a product? What capabilities does it gate, what's the developer experience cost, what's the brand/risk posture?*

## Bottom line

The event stream is **engineering-shaped, not product-shaped.** It's a working primitive that nobody outside the C++ engine team can see, author, observe, or extend. The product implications are mostly things you can't do *yet* — and they cluster around four absent capabilities: **observability, authorability, replay, and reach**. Two of them (observability, authorability) are the natural job of a real editor; the other two (replay, reach) gate features players and partners eventually expect.

## Who are the customers of this API?

Today, exactly one customer: a Cluiche engineer writing a `Module` subclass. Every other stakeholder is locked out:

| Stakeholder | Can they use streams today? | Why it matters |
|-------------|---------------------------|----------------|
| **C++ module author** | Yes — declare a typed handle, call `Connect` in `OnConnectStreams`. | Current happy path. |
| **Game designer / content author** | No — events are C++ types, wired in C++, with no manifest representation. | Designers can't author event-driven content (quests, cutscenes, triggers) without programmer time. |
| **Editor / tools dev** | Partial — `IApplicationInspectable::GetStreamInfo` exposes ID + kind, but no live event view, no graph, no inject. | A "real engine" editor shows live dataflow. CluicheEditor can't, today. |
| **Script / mod author (DiaPython, future Lua)** | No — typed templates are inaccessible from a non-C++ runtime. | Closes off a whole class of community/extensibility features. |
| **Live-ops / analytics** | No — the bus can't be tapped from outside the engine without a recompile. | Shipping a game without a telemetry tap is a known regret. |
| **QA / replay / esports** | No — consume-and-forget, no journal, no sequence numbers. | Bug repro is "play it again and pray." |
| **Networking / multiplayer module (future)** | No — no determinism, no replication semantics, no envelope metadata. | If multiplayer is ever a goal, the event layer is the natural unit and isn't ready. |

Six of seven personas are blocked. That's the product gap.

## Capabilities that are gated by today's design

Not "things the bus is missing" but "things the **product** can't do because of how the bus is shaped."

1. **Visual dataflow in the editor.** No way to render "module A → SimToUI → module B" because the topology only exists in code. Closes the door on the kind of node-graph view that's table-stakes in modern engines.
2. **Authoring event-driven content.** A designer wiring "when player picks up coin → play SFX → show HUD" must wait for a programmer to declare types and wire writers/readers. With manifest-declared streams + a small set of generic events, designers could do this in data.
3. **Live event inspector.** "Show me every event on stream X for the last 5 seconds, with sender + payload" — currently impossible without bespoke per-stream debug code in the writer/reader modules. A general inspector requires type-erased iteration which the bus does not expose.
4. **Inject / spoof events from the editor.** "Trigger this event to test the consumer" — not possible. Manual tests require running the actual producer module.
5. **Record + replay a session.** Q5/Q6 in the audit. Without a journal, sequence numbers, and timestamps in the envelope, replay is not building on the bus — it's a parallel system.
6. **Telemetry tap for shipped games.** No place to subscribe a "ship to backend" listener without modifying every producer.
7. **Cross-game reuse of an event taxonomy.** No shared baseline of "lifecycle events every Cluiche game emits" because lifecycle isn't on the bus at all (it's polled via `IApplicationInspectable`). Each new game would invent its own stream names.
8. **Determinism / multiplayer.** No ordering guarantees across streams; envelope has no sequence number; can't reproduce "what happened in what order."

## Developer experience (DX) — what's it like to add an event today?

For a C++ module author the path is clean but verbose. For anyone else, it's blocked.

**Adding a new event today:**
1. Define the C++ struct (`Types/MyEvent.h`).
2. Pick a StringCRC ID — by hand, no central registry to consult, no collision check.
3. Declare `EventStreamWriter<MyEvent>` on the producer module.
4. Declare `EventStreamReader<MyEvent>` on every consumer module.
5. Override `OnConnectStreams` on each, call `Connect(app)`.
6. Hope no one else picked the same StringCRC.
7. Hope every reader has a writer (no startup check).
8. Hope `T` matches across writer and readers (no runtime check).
9. Add the include path to every module's vcxproj.
10. Recompile.

**Compared to "what a designer wants":**
- Pick an event from a list.
- Wire producer to consumer in a UI or in data.
- Done.

The gap is roughly the entire content pipeline. Every dimension that the audit and scaling docs flagged as engineering hygiene (manifest topology, type tags, envelope metadata) doubles as DX work — they are the prerequisites for moving event authoring out of C++.

## Cross-game / platform positioning

Cluiche pitches itself as a **multi-game platform** (CluicheTest demo, future games). The event layer is a place where that positioning is either reinforced or contradicted:

- **Reinforced** if there's a shared baseline of lifecycle/system events every game inherits, declared once in the platform spec, and a generic editor view that works across games.
- **Contradicted** if every game reinvents its event taxonomy in C++ and each editor build is per-game work.

Today: contradicted. Each new game would re-pick stream names, re-declare event types, and re-hand-wire its dataflow.

## Risk register (product lens, not engineering)

| Risk | Probability | Brand / product impact |
|------|-------------|------------------------|
| **Silent type-aliasing UB ships in a player-facing build** | Low today, rises with team + stream count | Crash bugs that don't reproduce in dev = brand damage; "Cluiche is unstable." Cheap to prevent now (audit item 2). |
| **Editor team blocked on stream visualisation** | High once CluicheEditor takes on real work | Editor without live dataflow = "feels like a hobby engine" review-bait. |
| **Designer authoring blocked behind programmer time** | High whenever a content-driven feature lands | Slow content velocity = late milestones. |
| **No telemetry tap when a game ships** | Inevitable on first ship | Can't diagnose live issues. Forces an emergency tap retrofit. |
| **No replay for QA / community** | High once players exist | "Send us your save" instead of "share a clip." |
| **v1 `MessageBus` lingers in the tree** | Already true | Onboarding tax — every new dev asks "which is the real one?" Cheap to delete now that v2 carries production traffic. |

## Product-priority reordering

The audit and scaling docs ranked fixes by engineering yield. With a product lens the priorities shift slightly:

| Product priority | Item | Product yield (not engineering yield) |
|------------------|------|--------------------------------------|
| 1 | **Manifest-declared stream topology** | Unlocks editor visualisation, designer authoring, validation, telemetry, cross-game taxonomy. The single biggest product unlock from the audit. |
| 2 | **Lifecycle events on a well-known stream** | Turns "is the engine OK?" from a polling problem into a subscribable event. Foundation for cross-game baseline + editor live view + telemetry. |
| 3 | **Type-tag check on lookup** | Risk mitigation — prevents brand-damage UB. Cheap. |
| 4 | **Stream tap / type-erased iteration** | Required for editor inspector, telemetry, recorder. Worthless without (1) and (2). |
| 5 | **Envelope metadata (timestamp, seq, sender)** | Required for replay, networking, ordered debug view. |
| 6 | **Replay / rewind subsystem** | High-end product feature; depends on (1), (4), (5). |
| 7 | **Delete v1 MessageBus** | DX hygiene — onboarding clarity, no functional impact but reduces "two systems doing the same thing" confusion. |

## Editor angle — mockup gate applies

Any product move that surfaces streams in CluicheEditor (live event inspector, dataflow graph, event injector) is an **editor feature** and the project's standing rule is to mock it in HTML/CSS first and use the mockup as a visual acceptance gate. That is true even before a spec is written — the mockup is what the spec gets reviewed against.

(Memory: HTML mockups required for editor features; mockup is the visual acceptance gate.)

## One-paragraph product answer

The event stream today is a working engineering primitive with no product surface — only a C++ engineer can author, observe, or extend it. The single highest-product-yield change is **putting stream topology in the manifest**, because that one move turns the bus into something the editor can render, designers can author against, telemetry can tap, and validators can check at startup. Everything else (replay, multiplayer, modding, telemetry, lifecycle taxonomy) chains off that. Without it, every product capability that wants to ride the event layer ends up bypassing it.
