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
  accepting editor connections, broadcasting core metrics, managing data subscriptions,
  dispatching commands, and forwarding MessageBus events to connected editors.

responsibilities:
  - WebSocket server lifecycle management (start/stop/update)
  - Core metrics broadcasting (FPS, frame time, memory) every 500ms
  - Subscription-based data streaming to editors
  - Command dispatching (protocol commands and DiaAPI command gateway)
  - MessageBus event forwarding (phase transitions, module state changes)
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
    - Dia/DiaDebugServer/SubscriptionManager.h
    - Dia/DiaDebugServer/StateSerializer.h
    - Dia/DiaDebugServer/CommandDispatcher.h
  namespaces:
    - Dia::DebugServer
  entry_points:
    - DebugServerModule
    - SubscriptionManager
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
