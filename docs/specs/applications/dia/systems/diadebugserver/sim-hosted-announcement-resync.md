# Feature Spec: Sim-Hosted Announcement Resync

## Parent System
@docs/specs/applications/dia/systems/diadebugserver/diadebugserver.md

**Prerequisite:** @docs/specs/applications/dia/systems/diadebugserver/sim-hosted-queries.md (Done).
**Composes with:** @docs/specs/applications/dia/systems/diadebugserver/sim-hosted-command-help.md (Approved, not built). Resync is built first; Command Help later adds its fields to the register message and the replay carries them automatically (replay re-sends whatever the host stored per name).
**Origin:** DiaCellLayers Streams Migration, task 7 (`docs/specs/applications/dia/systems/diacelllayers/streams-migration.plan.md`) and the backlog row "Sim-hosted announcements can be lost silently past stream capacity (2026-11-02)". Pinned today by the disabled test `CellLayersIntegrationStreams.DISABLED_AnnouncementChurnPastStreamCapacityNeverLeavesAHostedNameUnroutedAtMain`.
**AD-001 sign-off:** NOT yet given. Needed for the DiaDebugServer changes (`SimHostedTypes.h`, `DebugServer`, `SimQueryHost`) and the CluicheGameBaseline changes (`SimRegistrationMessage.h`, `SimQueryHostModule`, `DebugServerHostModule`). Asked at the approval gate.
**Design chosen by the user:** 2026-11-02, "sequence numbers plus resync"; interview answers: heartbeat digest for tail loss, keep routing while out of sync, resync built before Command Help.

## Summary

Makes the Sim to Main name announcements self-healing. Every announcement carries a sequence number; a small digest is sent about once a second; Main detects a gap or a digest mismatch, asks the SimPU host to resync over the existing request stream, and the host replays its full name set, which Main installs atomically. A name the SimPU serves can no longer stay unroutable at Main because an announcement was dropped.

## Problem Statement

Names registered on `SimQueryHost` are announced to the Main-side `DebugServer` as `requestId 0` replies on the `SimQueryReply` `EventStream`, which holds 48 events and drops the oldest when full. Register/unregister churn between two Main drains can exceed 48 events (the cap of 32 names per module does not bound churn). The oldest announcements are lost: Main never learns a name (or never learns it went away) while the Sim host keeps serving it, so clients get "unknown command" for a name that exists. `SimQueryHostModule::PostReply` treats `kDroppedOldest` as success, so nothing is logged or counted.

## Goals

1. Detect any lost announcement, including the **last** one of a burst, within about one second.
2. Repair it automatically: Main requests a resync, the host replays its full name set, Main installs it atomically.
3. Keep routing during the repair: known names keep working, there is no window where the table is empty or partial.
4. Never block Main or SimPU; all cross-PU traffic stays on the two existing `EventStream`s; no mutex or atomic in domain state.
5. Make drops visible: a counter and a log line when the announce path overflows.
6. The wire change is one compatible set of messages that Command Help can extend later without touching the resync logic.

## Non-Goals

- More than one SimPU host. `SimReplyEvent` carries no source id, so one `SimQueryHostModule` per application is assumed and enforced (a second host logs an Error and its announcements are ignored). A source id is a follow-up if a second SimPU ever exists.
- Raising the `SimQueryReply` capacity (48) or the 32-name cap.
- Making replies (non-announcements) lossless. Replies already have a timeout and a "sim busy" path.
- Help metadata in the register message (Sim-Hosted Command Help owns that).
- Resync of anything other than the name table.

## Protocol

All messages are `SimReplyEvent`s with `requestId 0` (the existing registration side channel) except the resync request, which is a `SimRequestEvent`.

```
register   {"register":{"name":"sim.echo","kind":"query"},"seq":7}
unregister {"unregister":{"name":"sim.echo"},"seq":8}
digest     {"digest":{"seq":8,"count":12,"hash":"0x1a2b3c4d"}}              sent about once a second, only when no announce was sent that frame
resync_begin {"resync_begin":{"seq":8,"count":12}}                          answer to a resync request
replay     {"register":{"name":"sim.echo","kind":"query"},"replay":true}    exactly count of these follow resync_begin; carry no seq
```

- `seq` is a per-host `uint32_t`, starts at 1, +1 per register/unregister, wraps. A register or unregister **without** `seq`, and not marked `replay`, is Malformed (hard cutover: host and Main always ship together).
- `hash` is an order-independent combination (sum) of the CRC of every announced name and its kind, so equal sets give equal hashes regardless of registration order.
- Resync request: `SimRequestEvent` with `kind = SimKind::Resync` (new enumerator, value 2), `nameCrc = 0`, empty json. `SimQueryHost::Update` hands it to a callback set by `SimQueryHostModule` instead of looking up a handler.
- Replay is paced: at most 16 replay messages per `Update`, so the 48-slot stream also carries ordinary replies. `resync_begin` is sent first, never paced out of order.

**Main side state** (in `DebugServerHostModule`, plain members, Main thread only): `lastSeq`, `inSync`, `lastResyncRequestMs`, and a staging set `{expected count, received names, startedMs}` while a replay is in progress.

**Main behaviour**
- `register`/`unregister` with `seq == lastSeq + 1`: apply as today, `lastSeq = seq`.
- `seq` outside `lastSeq + 1` (gap, duplicate or reorder): count `announce_gaps`, set `inSync = false`, request a resync. The announcement itself is still applied (it is newer information).
- `digest`: mismatch if `seq != lastSeq`, or `count`/`hash` differ from Main's own name table. On mismatch count `digest_mismatches`, set `inSync = false`, request a resync.
- A resync request is sent at most once per 500 ms while out of sync (`PostRequest` false or a lost request is retried by the next digest or timer).
- `resync_begin`: open a staging set; names arriving as `replay` go into staging. When `count` names have arrived, replace the DebugServer's sim-hosted table with the staging set in one step, set `lastSeq = seq`, `inSync = true`, count `resyncs_completed`. Until then the live table is untouched, so routing never has a hole.
- A staging set not completed within 1000 ms is discarded (counter `resync_timeouts`), and the next digest mismatch requests again.

**Sim side behaviour** (`SimQueryHostModule`): `mAnnounced` is the authority. On a resync request it queues `resync_begin` plus one `replay` per entry in `mAnnounced` and drains the queue at most 16 per `Update`. If a new register or unregister happens while a replay is queued, the queue is rebuilt from `mAnnounced` (the replay is always a snapshot of the latest set; Main's `lastSeq` is set from `resync_begin.seq`, so later announcements continue the sequence).

## API Design

```cpp
namespace Dia::DebugServer {
    enum class SimKind : uint32_t { Query = 0, Command = 1, Resync = 2 };   // Resync: control request, never a handler name
}

namespace Cluiche::AppFlow {      // SimRegistrationMessage.h
    enum class SimRegistrationOp : uint32_t {
        NotRegistration = 0, Register = 1, Unregister = 2, Malformed = 3,
        Digest = 4, ResyncBegin = 5, Replay = 6
    };
    struct SimRegistration {
        SimRegistrationOp op; char name[kSimRegistrationMaxName]; Dia::DebugServer::SimKind kind;
        uint32_t seq = 0; uint32_t count = 0; uint32_t hash = 0;
    };
    // BuildSimRegistrationReply gains the seq; new Build functions for digest / resync_begin / replay.
    // Pure functions, no state, unit tested without streams.

    // Pure, unit-tested state machine on the Main side (no streams, no DebugServer): feeds on parsed messages and a clock,
    // returns actions {Apply(register|unregister), RequestResync, InstallSet, DiscardStaging, None}.
    class SimAnnounceTracker { /* lastSeq, inSync, staging, timers */ };
}
```

`SimQueryHost` gains `void SetResyncHandler(std::function<void()>)`; `DebugServer` gains `ReplaceSimHosted(const SimHostedName* names, uint32_t count)` (atomic swap of the sim-hosted table) and `GetSimHostedDigest(uint32_t& count, uint32_t& hash)`. `SimQueryHostModule::PostReply` reports `kDroppedOldest` to a counter and a rate-limited Warning.

## Acceptance Criteria

1. Announce sequence: every register/unregister built by the host carries `seq` starting at 1 and increasing by 1; a message without `seq` that is not `replay` parses as Malformed.
2. Gap: Main receiving `seq` other than `lastSeq + 1` applies the announcement, sets out-of-sync and posts exactly one resync request; further messages within 500 ms post no second request.
3. Tail loss: after the last announcement of a burst is dropped, the next digest (at most about one second later) makes Main request a resync.
4. Digest match: `seq`, `count` and `hash` equal to Main's state change nothing and request nothing; a digest is not sent in a frame that already sent an announcement.
5. Resync: on a request the host sends `resync_begin` then one `replay` per announced name, at most 16 per `Update`; Main installs the set only when `count` replays have arrived, in one atomic swap, then is in sync with `lastSeq = resync_begin.seq`.
6. No hole: while a replay is in progress the live sim-hosted table is unchanged; a request for a known name is still routed (test: query a name between `resync_begin` and the last replay).
7. A lost replay message leaves the staging set incomplete; it is discarded after 1000 ms, and the next digest mismatch triggers a new resync that completes.
8. A name registered on Sim but missing on Main (lost register) is routable after the resync; a name unregistered on Sim but still on Main (lost unregister) is gone after the resync (a request then falls through as unknown).
9. The disabled churn test is enabled and passes: three rounds of 32 register/unregister with Main not draining, then Main drains: every one of the 32 names is hosted and answers.
10. Overflow visibility: a `kDroppedOldest` result on the announce path increments `dia.debugserver.sim.announce_overflow` and logs a Warning at most once per second.
11. Sequence wrap: `seq` rolling over `0xFFFFFFFF` to 0 then 1 is treated as consecutive (tested with a tracker seeded near the limit).
12. A second `SimQueryHostModule` logs an Error and its announcements are ignored; the first host is unaffected.
13. Metrics: `dia.debugserver.sim.announce_gaps`, `.digest_mismatches`, `.resyncs_requested`, `.resyncs_completed`, `.resync_timeouts`, `.announce_overflow`. Logs: Warning on gap or mismatch, Info on resync completion with the name count.
14. Wire format to clients is unchanged; no new client-visible command.
15. No `std::mutex`, `Core::Mutex` or atomic added; all cross-PU traffic is on the two existing streams. Release builds exclude it with the rest of the DebugServer (`DIA_DEBUG`).

## Binding Decisions

| Source | ID | Decision | Compliance |
|--------|----|----------|------------|
| Dia | AD-002 | No STL in public APIs | New public structs use fixed char arrays and `uint32_t`; `std::function` for the resync handler follows the `QueryHandler` precedent |
| System | DDS-001 | Generic system | The tracker and messages are domain-agnostic; CellLayers is only the case that found the bug |
| System | DDS-005, DDS-010 | JSON over WebSocket / JSON for all messages | Control messages are JSON text in the existing events; the client wire is untouched |
| Platform (CLAUDE.md) | Thread safety | PU boundaries are crossed by streams only | Detection, request and replay all use the existing `SimQueryReply` and `SimQueryRequest` streams |
| Dia | AD-001 | Module changes need sign-off | **Pending** (see header) |

## Open Design Questions

1. **Single-host assumption.** `SimReplyEvent` has no source id, so two SimPU hosts would interleave `seq` and trigger endless resyncs. The spec enforces one host (AC 12). If a second SimPU is planned, add a `source` field now (cheap) instead of later (wire change).
2. **Digest cost and interval.** About one 100-byte event per second per host is negligible against the 48-slot stream, but "about one second" counts SimPU frames (30 frames at 30 Hz), not wall time, so a stalled SimPU also stalls the digest. Revisit if a stall must also be detected (that would be a health reporter, not this feature).
3. **Replay during heavy churn.** If names churn faster than the replay drains, the queue is rebuilt each time and the resync may take several frames; it always converges once churn stops, and Main keeps routing meanwhile. If a game legitimately churns names every frame, this would never settle; no such caller exists today.

## Tasks

| # | Task | Size |
|---|------|------|
| 1 | `SimRegistrationMessage.h`: `seq`, digest, resync_begin, replay build/parse; `SimKind::Resync`; name-set hash; unit tests (ACs 1, 4, 11) | M |
| 2 | `SimAnnounceTracker` pure state machine and tests (ACs 2, 3, 5, 6, 7, 11) | M |
| 3 | `DebugServer::ReplaceSimHosted` and `GetSimHostedDigest`; `DebugServerHostModule` wiring (feeds the tracker, posts the resync request, installs the set) | M |
| 4 | `SimQueryHost` resync handler and `SimQueryHostModule`: seq counter, digest timer, paced replay queue, overflow counter and Warning | M |
| 5 | Enable and extend the churn integration test; add tail-loss, lost-replay, lost-unregister and no-hole integration tests (ACs 6 to 9) | L |
| 6 | Metrics, logs, second-host guard, module docs, registry, amend Sim-Hosted Command Help spec if needed, close the backlog row | S |
| 7 | Exhaustive test pass (gap audit, edge tests, mutation check, sanitizer) | M |

## Status

`Draft`
