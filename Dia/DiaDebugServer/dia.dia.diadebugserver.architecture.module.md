---
schema: dia.module.v1
module_id: dia.dia.diadebugserver
name: DiaDebugServer
owner_team: TBD
layer: foundation/services
status: active
maturity: dev

path: Dia/DiaDebugServer
language: cpp
parent_module_id: dia

summary: >
  WebSocket-based debug server that runs inside games to enable remote debugging by editors.

intent: >
  Provides a WebSocket server module that integrates into the Dia application framework,
  accepting editor connections, broadcasting core metrics, dispatching commands, and
  forwarding stream events to connected editors via tap-based subscriptions.

responsibilities:
  - WebSocket server lifecycle management (start/stop/update)
  - Core metrics broadcasting (FPS, frame time, memory) every 500ms
  - Tap-based data streaming to editors: on Subscribe message, calls IDebugStateProvider::FindStream
    then AttachTap on the stream; events are pushed to connected clients on delivery
  - Stage transitions arrive via tap on the $lifecycle stream (not polled or broadcast separately)
  - NotifySubscribers: broadcast-to-all shim retained during DebugLayerManager migration (pending removal)
  - PublishTopic(topic, payload): any-thread publish of a virtual topic; latest payload per topic is retained,
    delivered by Tick (Main, latest wins) and replayed to a late subscriber after its subscribe ack
  - GetTopicSubscriberCount(topic): any-thread count of distinct connections subscribed to a topic (mutex-guarded table,
    rebuilt on Main from the subscription list after every subscribe/unsubscribe/disconnect/Stop)
  - Command dispatching (protocol commands and DiaAPI command gateway)
  - Server self-monitoring and performance tracking
  - Sim-hosted queries (Main side): DebugServer::RegisterSimHosted/UnregisterSimHosted mark a name as handled on SimPU;
    HandleCommand posts a SimRequestEvent through ISimRequestSink and defers the COMMAND_RESPONSE until
    CompleteSimReply (reply event drained on Main) or a 2000 ms timeout swept in Tick (SimPendingTable tracks pending ids)
  - Sim-hosted queries (SimPU side): SimQueryHost drains an ISimRequestSource, runs registered Query/Command handlers
    and posts SimReplyEvents to an ISimReplySink; fixed-size payloads (SimHostedTypes.h)
  - Sim-hosted observability: counters dia.debugserver.sim.{requests,replies,timeouts,busy,too_large,handler_failed},
    gauge dia.debugserver.sim.pending, Info logs on sink attach/detach and name register/unregister, Warning on timeout
  - Test seams: DebugServer::SetNowMsFunction (clock) and SetSendHook (outgoing messages, no socket)

non_responsibilities:
  - Cross-PU access to domain state: done via streams only (EventStream request/reply for sim-hosted queries, FrameStream
    snapshots); no shared mutexes between Main and SimPU for query data
  - Editor-side UI or connection management
  - Game-specific data serialization beyond core metrics
  - WebSocket protocol implementation (delegated to DiaWebSocket)
  - Debug protocol message format definition (delegated to DiaDebugProtocol)

dependent_modules: []

public_api:
  headers:
    - Dia/DiaDebugServer/DebugServerModule.h
    - Dia/DiaDebugServer/StateSerializer.h
    - Dia/DiaDebugServer/CommandDispatcher.h
    - Dia/DiaDebugServer/QueryRegistry.h      # Register/Unregister/Has/Execute; SetRegisterVeto (refuses names that are sim-hosted)
    - Dia/DiaDebugServer/DebugServer.h        # RegisterSimHosted, UnregisterSimHosted, SetSimRequestSink, CompleteSimReply, SetNowMsFunction, SetSendHook
    - Dia/DiaDebugServer/SimHostedTypes.h     # SimKind, SimReplyStatus, SimRequestEvent, SimReplyEvent, kSimPayloadBytes
    - Dia/DiaDebugServer/ISimRequestSink.h
    - Dia/DiaDebugServer/SimPendingTable.h
    - Dia/DiaDebugServer/SimQueryHost.h       # SimQueryHost, ISimRequestSource, ISimReplySink
  namespaces:
    - Dia::DebugServer
  entry_points:
    - DebugServerModule   # exposes NotifySubscribers (broadcast-to-all shim, DebugLayerManager migration pending)
    - StateSerializer
    - CommandDispatcher
    - SimQueryHost
    - SimPendingTable

dependencies:
  required:
    - dia.core.crc
    - dia.core.json
    - dia.core.containers
    - dia.core.threading
    - dia.core.time
    - dia.application
    - dia.websocket
    - dia.api
    - dia.dia.diadebugprotocol
  forbidden: []
---
