---
schema: dia.module.v1
module_id: dia.dia.diadebugserver
name: DiaDebugServer
owner_team: TBD
layer: platform
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
  - Command dispatching (protocol commands and DiaAPI command gateway)
  - Server self-monitoring and performance tracking

non_responsibilities:
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
  namespaces:
    - Dia::DebugServer
  entry_points:
    - DebugServerModule   # exposes NotifySubscribers (broadcast-to-all shim, DebugLayerManager migration pending)
    - StateSerializer
    - CommandDispatcher

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
