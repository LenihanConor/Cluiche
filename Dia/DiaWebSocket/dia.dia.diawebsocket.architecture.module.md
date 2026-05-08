---
schema: dia.module.v1
module_id: dia.dia.diawebsocket
name: DiaWebSocket
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaWebSocket
language: cpp
parent_module_id: dia.root

summary: >
  WebSocket communication system wrapping websocketpp for real-time bidirectional messaging.

intent: >
  Provide Dia-friendly Server and Client abstractions for WebSocket communication,
  enabling remote debugging, editor-to-game connections, and future multiplayer networking.
  Hides websocketpp behind a Pimpl pattern and uses DiaCore threading primitives for
  thread-safe message queuing with main-thread callbacks via Update().

responsibilities:
  - WebSocket server accepting multiple client connections
  - WebSocket client with auto-reconnect support
  - Text and binary message transport
  - Thread-safe message queuing between worker and main threads
  - Connection lifecycle management

non_responsibilities:
  - Protocol definition (JSON, Protobuf) — user responsibility
  - Message serialization — user serializes before Send()
  - TLS/SSL encryption — future enhancement
  - Authentication — user implements via message protocol
  - High-level networking (RPC, sync/replication)

dependent_modules: []

public_api:
  headers:
    - Dia/DiaWebSocket/Error.h
    - Dia/DiaWebSocket/Message.h
    - Dia/DiaWebSocket/Server.h
    - Dia/DiaWebSocket/Client.h
  namespaces:
    - Dia::WebSocket
  entry_points:
    - Server
    - Client
    - Message
    - MessageType
    - ConnectionState
    - Error
    - ErrorCode
    - ErrorSeverity

dependencies:
  required:
    - dia.core.containers.arrays
    - dia.core.core
    - dia.core.threading
  forbidden: []
---
