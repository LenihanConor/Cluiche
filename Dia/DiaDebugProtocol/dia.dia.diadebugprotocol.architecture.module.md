---
schema: dia.module.v1
module_id: dia.dia.diadebugprotocol
name: DiaDebugProtocol
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaDebugProtocol
language: cpp
parent_module_id: dia.root

summary: >
  Header-only shared protocol definitions for editor-game debug communication.

intent: >
  Centralize message types, serialization helpers, and data type constants used by
  both DiaDebugServer (server-side) and DiaEditor (client-side). Single source of
  truth prevents protocol drift between the two sides.

responsibilities:
  - Define MessageType enum for all valid debug message types
  - Define C++ structs for typed message construction and parsing
  - Provide inline serialization/deserialization helpers for JSON wire format
  - Define core data type StringCRC constants for subscriptions
  - Protocol versioning via version field in handshake

non_responsibilities:
  - Transport layer (DiaWebSocket provides WebSocket client/server)
  - Connection management (owned by DiaDebugServer and DiaEditor)
  - Application logic or game state serialization
  - State serialization (DiaDebugServer's StateSerializer handles that)

dependent_modules: []

public_api:
  headers:
    - Dia/DiaDebugProtocol/DiaDebugProtocol.h
    - Dia/DiaDebugProtocol/MessageTypes.h
    - Dia/DiaDebugProtocol/MessageStructs.h
    - Dia/DiaDebugProtocol/Serialization.h
    - Dia/DiaDebugProtocol/DataTypes.h
  namespaces:
    - Dia::DebugProtocol
  entry_points:
    - MessageType
    - MessageHeader
    - CoreMetricsPayload
    - HandshakeRequest
    - HandshakeResponse
    - SubscribeMessage
    - DataUpdateMessage
    - EventMessage
    - CommandRequestMessage
    - CommandResponseMessage
    - ErrorMessage

dependencies:
  required:
    - dia.core.crc
    - dia.core.json
    - dia.core.time
  forbidden:
    - dia.application
    - dia.dia.diawebsocket
    - dia.graphics
---
