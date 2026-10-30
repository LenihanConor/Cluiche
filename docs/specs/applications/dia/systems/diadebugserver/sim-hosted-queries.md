# Feature Spec: Sim-Hosted Queries

## Parent System
@docs/specs/applications/dia/systems/diadebugserver/diadebugserver.md

## Summary

Lets a query or DiaAPI command that must read or change SimPU-owned state be answered **on SimPU**, with the reply returned to the editor client asynchronously. The DebugServer (Main) posts a request event over an `EventStream`, a SimPU-side host runs the handler in its own update, and the reply comes back over a second `EventStream`. No state is shared between PUs and no mutex guards domain state. Replies are small and fixed-size (16 KB cap); large data is paged by the client.

## Problem Statement

`DebugServer::HandleCommand` runs on Main and replies in the same call. Handlers registered by SimPU modules (for example the DiaCellLayers mirror queries, `debug.domain_state`, viewer DiaAPI commands) therefore execute on Main against SimPU-owned objects. To make that safe, `CellLayerViewerDomain` holds a non-recursive mutex through its whole `Update` and exposes `StateMutex()` to outside readers. That breaks the rule that every PU boundary is crossed by streams (see memory `pu-boundary-streams`), stalls Main behind SimPU work, and deadlocks if a callback re-enters the domain. The DebugServer has no deferred-reply mechanism today: no request id, no pending-reply table.

## Goals

1. Let a SimPU module register a query or DiaAPI command as **sim-hosted**; its handler always runs on SimPU.
2. Route requests Main to Sim and replies Sim to Main over `EventStream`s with a request id; the client receives its reply on the same connection and command id as for Main-hosted handlers.
3. Bounded and safe: fixed-size events, fixed-size pending table, timeout, never block Main.
4. Wire format to the client is unchanged: a client cannot tell a sim-hosted handler from a Main-hosted one except by latency.
5. Prove the mechanism by moving DiaCellLayers onto it and deleting `CellLayerViewerDomain`'s mutex (see Follow-on).

## Non-Goals

- Migrating other domains (`UtilityAIDebugDomain` atomics, `MessageBusDebugDomain`, DomainRegistry maps). Log as backlog debt.
- Large payloads through the stream; clients page.
- Ordered multi-chunk server push (chunk streaming). Possible later optimisation.
- Changing how Main-hosted queries or commands work.

## API Design

```cpp
namespace Dia::DebugServer {
    // Fixed-size, trivially copyable events. Payload is inline JSON text.
    static constexpr uint32_t kSimPayloadBytes = 16384;
    struct SimRequestEvent  { uint32_t requestId; uint32_t nameCrc; uint32_t kind;  /* Query | Command */ char json[kSimPayloadBytes]; };
    struct SimReplyEvent    { uint32_t requestId; uint32_t status;                  /* Ok | HandlerFailed | TooLarge | UnknownName */ char json[kSimPayloadBytes]; };

    // Main side: DebugServer talks to this; it stays stream-agnostic.
    class ISimRequestSink {
    public:
        virtual ~ISimRequestSink() {}
        virtual bool PostRequest(const SimRequestEvent& request) = 0;   // false: stream full
    };

    // Registered on Main (names only), so HandleCommand knows to defer.
    // DebugServer::SetSimRequestSink(ISimRequestSink*), DebugServer::RegisterSimHosted(StringCRC name, SimKind kind),
    // DebugServer::CompleteSimReply(const SimReplyEvent&)  // Main thread only, called when the reply event is drained

    // SimPU side: owns the handlers, drains requests once per frame, posts replies.
    class SimQueryHost {
    public:
        using QueryHandler = std::function<Json::Value(const Json::Value& args)>;   // same as QueryRegistry
        bool RegisterQuery(StringCRC name, QueryHandler handler);                   // SimPU only
        bool RegisterCommand(StringCRC name, QueryHandler handler);
        void Update();                                                              // drain requests, run handlers, post replies
    };
}
```

`DebugServerHostModule` (Main) and a SimPU host module own the two `EventStream`s declared in the `.diaapp` manifest and adapt them to `ISimRequestSink` and `CompleteSimReply`.

## Acceptance Criteria

1. A handler registered through `SimQueryHost` runs only on the thread that calls `SimQueryHost::Update()` (SimPU); a Main-thread test proves no handler invocation happens on the DebugServer thread.
2. For a sim-hosted name, `HandleCommand` posts a `SimRequestEvent` with a fresh `requestId`, records `{requestId, connectionId, commandId, deadline}` in a fixed-size pending table (64 entries), and returns without sending a reply.
3. When the matching `SimReplyEvent` is drained on Main, `CompleteSimReply` sends the standard `COMMAND_RESPONSE` to the recorded connection and removes the entry.
4. Wire format is identical to a Main-hosted handler's response (same fields, same payload shape).
5. A pending entry not completed within 2000 ms is removed and the client receives `{success:false, error:"sim timeout"}`; a late reply for a removed id is dropped and logged once.
6. Pending table full, or `PostRequest` returns false: client receives `{success:false, error:"sim busy"}` immediately.
7. A reply whose JSON exceeds `kSimPayloadBytes` is replaced by `{success:false, error:"reply too large"}` (status `TooLarge`); a request whose args exceed it is rejected on Main with the same error.
8. A handler that throws or returns a non-object yields `{success:false, error:"handler failed"}`, and `Update()` continues with the next request.
9. Connection closed while a request is pending: the entry is dropped, no send is attempted, no leak.
10. Unknown or unregistered sim name falls through to the existing dispatch order (QueryRegistry, then DiaAPI fallback); a name registered in both is rejected at registration with an Error log.
11. No `std::mutex`, `Core::Mutex`, or atomic is introduced in domain state for this feature; the only synchronisation is inside the framework `EventStream`s. Existing `PublishTopic` is unchanged.
12. Metrics and logs: `dia.debugserver.sim.requests`, `.replies`, `.timeouts`, `.busy`, `.too_large`, `.handler_failed`; Info log on sink attach, Warning on timeout.
13. Release builds exclude all of it (`DIA_DEBUG`), like the rest of the DebugServer.

## Follow-on (separate specs, not this feature)

- **DiaCellLayers onto streams:** mirror queries and the viewer's DiaAPI commands become sim-hosted; `debug.domain_state` for the viewer reads a `FrameStream` snapshot published each frame; `CellLayersPointerQueue` becomes an `EventStream`; `StateMutex()` and `mMutex` are deleted. Feature 5 limits drop to about 1,000 cells per `cells.layer.values` call and about 1,000 indices per `cells.select.get` call (amends ACs 7 and 9 and closes its open item 3). Features 1 and 5 specs and plans are amended in place.
- Fix `CLAUDE.md` Thread Safety line, which still recommends `std::mutex` for cross-PU state.

## Tasks

| # | Task | Size |
|---|------|------|
| 1 | `SimRequestEvent` / `SimReplyEvent` structs and `ISimRequestSink` in DiaDebugServer; request-id allocator and fixed pending table with deadline; verify `CommandRegistry` thread-safety (notes only) | M |
| 2 | `DebugServer::RegisterSimHosted`, `HandleCommand` deferral, `CompleteSimReply`, timeout sweep in Tick, busy and too-large paths | M |
| 3 | `SimQueryHost` (registry, Update drain, exception wrapper, reply cap) | M |
| 4 | Host wiring: manifest streams, Main-side adapter in `DebugServerHostModule`, SimPU host module | M |
| 5 | Metrics, logs, module doc and registry update | S |
| 6 | Tests: dispatch thread, request/reply round trip, timeout, busy, too large, handler failure, connection close, duplicate registration; loopback E2E with a sim-hosted echo query | L |

## Binding Decisions

| Source | ID | Decision | Compliance |
|--------|----|----------|------------|
| Platform | PD-004 / Dia AD-002 | No STL in public APIs | `std::function` for handlers matches the QueryRegistry precedent; events use fixed char arrays |
| System | DDS-001 | Generic system | The mechanism is domain-agnostic; CellLayers is only the first user |
| System | DDS-005, DDS-010 | JSON over WebSocket | Payloads are JSON text; client wire format unchanged |
| System | DDS-008 | Commands forwarded to DiaAPI CommandRegistry | Sim-hosted commands are an additional route; unmatched names still fall through to the DiaAPI fallback |

**Conflict to note:** the system spec's Thread Safety note says communication with the game thread uses `Mutex` and queues. This feature narrows that for domain state: streams only. The system spec note should be amended on approval.

## Resolved Design Questions

1. **DiaAPI commands on SimPU (resolved).** Sim-hosted commands call the SimPU handler directly and do not go through the `CommandRegistry` on SimPU. Task 1 verifies `CommandRegistry` thread-safety; CLI parity for sim-hosted commands is a follow-up only if the registry proves safe.
2. **Event size (resolved).** 16 KB per request and reply (`kSimPayloadBytes = 16384`). Oversize still returns the "reply too large" error. Ring memory is fixed: slot size times ring depth times readers.
3. **Request ordering (resolved).** No per-frame time budget in `SimQueryHost::Update` for now; handlers are already bounded (20-layer stats about 4 ms). Revisit if one slow handler starves other clients.

## Status

`Approved`

**Plan:** @docs/specs/applications/dia/systems/diadebugserver/sim-hosted-queries.plan.md
