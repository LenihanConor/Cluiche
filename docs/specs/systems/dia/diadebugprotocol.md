# System Spec: DiaDebugProtocol

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaDebugProtocol is a shared, header-only protocol definition used by both DiaEditor (client) and DiaDebugServer (server). It defines message types, serialization helpers, data type constants, and JSON message formats for the debug connection between editors and running games. By centralizing the protocol in one place, both sides stay in sync and protocol changes are made once rather than duplicated.

**Location:** `Dia/DiaDebugProtocol/` - header-only, no .cpp files, no Module base class.

## Responsibilities

- **Message Type Enum** - Define all valid message types for editor-game communication
- **Message Structs** - C++ structs for typed message construction and parsing
- **Serialization Helpers** - Inline functions to serialize/deserialize messages to/from JSON
- **Data Type Constants** - StringCRC constants for subscription data types
- **Protocol Versioning** - Version field in handshake for forward compatibility

## Non-Responsibilities

- **Transport** - DiaWebSocket provides WebSocket client/server
- **Connection Management** - DiaEditor's GameConnectionManager and DiaDebugServer's DebugServerModule own connection lifecycle
- **Application Logic** - Protocol is a wire format, not business logic
- **State Serialization** - DiaDebugServer's StateSerializer handles game-specific serialization

## Public Interfaces

### Message Types

```cpp
namespace Dia::DebugProtocol {
    static constexpr int kProtocolVersion = 1;

    enum class MessageType {
        // Connection
        kHandshake,       // Bidirectional: version negotiation on connect

        // Subscription
        kSubscribe,       // Editor -> Game: subscribe to data type
        kUnsubscribe,     // Editor -> Game: unsubscribe from data type

        // Data flow
        kCoreMetrics,     // Game -> Editor: FPS, memory, frame time (broadcast, no subscription needed)
        kDataUpdate,      // Game -> Editor: subscribed data changed
        kEvent,           // Game -> Editor: one-shot event (e.g., phase transition)

        // Commands
        kCommandRequest,  // Editor -> Game: execute DiaAPI command
        kCommandResponse, // Game -> Editor: command result

        // Errors
        kError            // Either direction: error occurred
    };
}
```

### Message Structs

```cpp
// Parse output structs — const char* pointers reference jsoncpp's internal buffer
// and are valid only within the parse callback scope. Copy if persistence needed.
namespace Dia::DebugProtocol {
    struct MessageHeader {
        MessageType type;
        uint64_t timestamp;
    };

    struct SubscribeMessage {
        StringCRC dataType;
        Json::Value filter;  // Optional filter (e.g., { "pu_id": "MainProcessingUnit" })
    };

    struct CoreMetricsPayload {
        float fps;
        float frameTimeMs;
        float memoryUsedMb;
        float memoryAvailableMb;
    };

    struct DataUpdateMessage {
        StringCRC dataType;
        Json::Value payload;
    };

    struct EventMessage {
        StringCRC eventType;
        Json::Value payload;
    };

    struct CommandRequestMessage {
        StringCRC command;
        Json::Value payload;
    };

    struct CommandResponseMessage {
        StringCRC command;
        bool success;
        const char* message;
        Json::Value payload;  // Optional result data
    };

    struct HandshakeRequest {
        int protocolVersion;
        const char* clientName;
        const char* clientVersion;
    };

    struct HandshakeResponse {
        int protocolVersion;
        bool accepted;
        const char* serverName;
        const char* serverVersion;
    };

    struct ErrorMessage {
        const char* errorCode;
        const char* message;
    };
}
```

### Serialization Helpers (Inline)

```cpp
namespace Dia::DebugProtocol {
    // Serialize to JSON (for sending)
    inline Json::Value Serialize(const MessageHeader& header, const Json::Value& payload);
    inline Json::Value SerializeSubscribe(const StringCRC& dataType, const Json::Value& filter);
    inline Json::Value SerializeUnsubscribe(const StringCRC& dataType);
    inline Json::Value SerializeCoreMetrics(const CoreMetricsPayload& metrics);
    inline Json::Value SerializeDataUpdate(const StringCRC& dataType, const Json::Value& payload);
    inline Json::Value SerializeEvent(const StringCRC& eventType, const Json::Value& payload);
    inline Json::Value SerializeCommandRequest(const StringCRC& command, const Json::Value& payload);
    inline Json::Value SerializeCommandResponse(const StringCRC& command, bool success, const char* message, const Json::Value& payload);
    inline Json::Value SerializeHandshakeRequest(int protocolVersion, const char* clientName, const char* clientVersion);
    inline Json::Value SerializeHandshakeResponse(int protocolVersion, bool accepted, const char* serverName, const char* serverVersion);
    inline Json::Value SerializeError(const char* errorCode, const char* message);

    // Deserialize from JSON (for receiving)
    inline MessageType GetMessageType(const Json::Value& message);
    inline bool ParseHandshakeRequest(const Json::Value& message, HandshakeRequest& out);
    inline bool ParseHandshakeResponse(const Json::Value& message, HandshakeResponse& out);
    inline bool ParseSubscribe(const Json::Value& message, SubscribeMessage& out);
    inline bool ParseCoreMetrics(const Json::Value& message, CoreMetricsPayload& out);
    inline bool ParseDataUpdate(const Json::Value& message, DataUpdateMessage& out);
    inline bool ParseEvent(const Json::Value& message, EventMessage& out);
    inline bool ParseCommandRequest(const Json::Value& message, CommandRequestMessage& out);
    inline bool ParseCommandResponse(const Json::Value& message, CommandResponseMessage& out);
    inline bool ParseError(const Json::Value& message, ErrorMessage& out);
}
```

### Data Type Constants

Core data types defined here. Systems export their own domain-specific data types as **typed StringCRC constants in a dedicated header** (e.g., `DiaApplication/DebugDataTypes.h`). Callers reference the constant — never raw strings — so a typo is a compile error.

```cpp
namespace Dia::DebugProtocol {
    // Core data types (framework-level, used by DiaDebugServer)
    namespace DataType {
        static const StringCRC kCoreMetrics("core_metrics");
    }
}

// System-specific data types live in their own headers:
// DiaApplication/DebugDataTypes.h
namespace Dia::Application::DebugDataType {
    static const StringCRC kProcessingUnitState("processing_unit_state");
    static const StringCRC kPhaseTransition("phase_transition");
    static const StringCRC kModuleState("module_state");
    static const StringCRC kMessageBus("message_bus");
    static const StringCRC kPerformanceBreakdown("performance_breakdown");
}

// Consumers use the constant — typo = compile error:
// manager->SubscribeToData(Dia::Application::DebugDataType::kPhaseTransition, callback);
```

## Protocol Wire Format

All messages are JSON with this base structure:
```json
{
  "version": 1,
  "type": "message_type",
  "timestamp": 1234567890,
  "payload": { }
}
```

### Handshake (on connect)

Client (editor) sends handshake first after WebSocket connect. Server (game) responds with accepted/rejected.

**Editor -> Game: Handshake Request**
```json
{
  "type": "handshake",
  "timestamp": 1234567890,
  "payload": {
    "protocol_version": 1,
    "client_name": "CluicheEditor",
    "client_version": "1.0.0"
  }
}
```

**Game -> Editor: Handshake Response**
```json
{
  "type": "handshake",
  "timestamp": 1234567890,
  "payload": {
    "protocol_version": 1,
    "accepted": true,
    "server_name": "DiaDebugServer",
    "server_version": "1.0.0"
  }
}
```

If the server does not support the client's protocol version, it responds with `"accepted": false` and an error message, then closes the connection. No other messages may be sent before the handshake completes.

### Core Metrics (broadcast every 500ms, no subscription needed)

**Game -> Editor:**
```json
{
  "type": "core_metrics",
  "timestamp": 1234567890,
  "payload": {
    "fps": 60.0,
    "frame_time_ms": 16.67,
    "memory_used_mb": 512,
    "memory_available_mb": 2048
  }
}
```

### Subscription Messages

**Editor -> Game: Subscribe**
```json
{
  "type": "subscribe",
  "data_type": "processing_unit_state",
  "filter": {
    "pu_id": "MainProcessingUnit"
  }
}
```

**Editor -> Game: Unsubscribe**
```json
{
  "type": "unsubscribe",
  "data_type": "processing_unit_state"
}
```

**Game -> Editor: Data Update (after subscription)**
```json
{
  "type": "data_update",
  "data_type": "processing_unit_state",
  "timestamp": 1234567890,
  "payload": {
    "pu_id": "MainProcessingUnit",
    "current_phase": "UpdatePhase",
    "active_modules": ["RenderModule", "PhysicsModule", "InputModule"],
    "state": "kRunning"
  }
}
```

### Event Broadcasting

**Game -> Editor: Event (e.g., phase transition)**
```json
{
  "type": "event",
  "event_type": "phase_transition",
  "timestamp": 1234567890,
  "payload": {
    "pu_id": "MainProcessingUnit",
    "from_phase": "UpdatePhase",
    "to_phase": "RenderPhase"
  }
}
```

### Command Execution

**Editor -> Game: Execute Command**
```json
{
  "type": "command",
  "command": "hot_reload",
  "payload": {
    "manifest_path": "C:/path/to/example.diaapp"
  }
}
```

**Game -> Editor: Command Response**
```json
{
  "type": "command_response",
  "command": "hot_reload",
  "success": true,
  "message": "Hot reload completed successfully",
  "timestamp": 1234567890
}
```

### Protocol Commands (debug server specific)

**get_state** - Request current game state:
```json
{
  "type": "command",
  "command": "get_state",
  "payload": { "state_type": "processing_units" }
}
```

### Error Messages

```json
{
  "type": "error",
  "error_code": "INVALID_COMMAND",
  "message": "Unknown command: 'invalid_cmd'",
  "timestamp": 1234567890
}
```

### Example Data Types (for subscription)

Data types are StringCRC values defined by each system. The protocol does not enumerate them — any StringCRC is valid as a subscription data type. Examples from DiaApplication:

| Data Type | Defined By | Description |
|-----------|------------|-------------|
| `processing_unit_state` | DiaApplication | PU/Phase/Module runtime state |
| `phase_transition` | DiaApplication | Phase transition events |
| `module_state` | DiaApplication | Individual module state details |
| `message_bus` | DiaApplication | MessageBus messages (inter-module) |
| `performance_breakdown` | DiaApplication | Detailed timing per module/phase |

## Dependencies

### Required Systems (from Dia)
- **DiaCore** - StringCRC for data type constants, JSON parsing (jsoncpp wrapper)

### No Other Dependencies
DiaDebugProtocol is intentionally dependency-light. It does not depend on DiaApplication, DiaWebSocket, or any transport layer.

## Related Systems

| System | Relationship | Interface |
|--------|--------------|-----------|
| DiaEditor | Consumer (client-side) | GameConnectionManager uses protocol types to send/receive messages |
| DiaDebugServer | Consumer (server-side) | DebugServerModule uses protocol types to send/receive messages |
| DiaWebSocket | Transport layer | Carries serialized protocol messages; protocol is transport-agnostic |

## Inherited Binding Decisions

| Source | ID | Decision | Impact on DiaDebugProtocol |
|--------|----|----------|---------------------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | Data type constants and command IDs are StringCRC |
| Platform | PD-004 | No STL containers in public APIs | Structs use `const char*`, StringCRC — no std::string, std::vector |
| Platform | PD-006 | Visual Studio project files are source of truth | Header-only — included by consumer .vcxproj files, no standalone project needed |
| Dia | AD-001 | Module system with YAML frontmatter | Lightweight module doc at `dia.dia.diadebugprotocol.architecture.module.md` — appears in dependency graph, documents consumers |
| Dia | AD-002 | No STL in public APIs | Reinforces PD-004; use DiaCore types |
| Dia | AD-003 | Namespace convention: `Dia::<Module>::` | All types in `Dia::DebugProtocol::` namespace |
| Dia | AD-004 | ProcessingUnit/Phase/Module for app structure | N/A — protocol types are passive data, not runtime modules |
| Dia | AD-005 | Component-based entities | N/A — protocol structs are wire format, not entities |
| DiaEditor | SED-004 | JSON over WebSocket | Protocol serializes to JSON |
| DiaEditor | SED-010 | Use DiaDebugProtocol for all editor-game wire types | This IS DiaDebugProtocol — self-referential, compliant by definition |

## System-Specific Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| DDP-001 | Header-only, no .cpp files | Minimizes link-time coupling; both consumers just include headers | Proposed | Yes |
| DDP-002 | Protocol version field in handshake | Forward compatibility; client and server can negotiate capabilities | Proposed | Yes |
| DDP-003 | Single namespace `Dia::DebugProtocol` | Shared neutral ground; neither `Dia::Editor` nor `Dia::DebugServer` | Proposed | Yes |
| DDP-004 | Inline serialization helpers | Header-only constraint; no separate compilation unit needed | Proposed | Yes |
| DDP-005 | Parse functions return bool (not throw) | Consistent with Dia error handling; caller decides how to handle malformed messages | Proposed | Yes |
| DDP-006 | Systems export data types as typed StringCRC constants in a header; no raw strings at call sites | Protocol is type-agnostic (subscriptions are StringCRC); typed constants give compile-time safety against typos; each system owns its constants | Proposed | Yes |

## Features

DiaDebugProtocol is header-only shared types with no runtime behavior. No feature specs are needed — the protocol definition above IS the specification. Changes to the protocol are made by updating this system spec directly.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Scope | Why a separate system instead of duplicating types? | Protocol drift — DiaEditor and DiaDebugServer independently defining MessageType enums will diverge. Single source of truth. |
| 2 | Scope | Why not put this in DiaCore? | It's debug-specific, not foundation. DiaCore shouldn't know about debug protocols. |
| 3 | Versioning | How to handle protocol evolution? | Version field in handshake; new message types are additive; old fields never removed, only deprecated. |
| 4 | Dependencies | Should this depend on DiaWebSocket? | No. Protocol is transport-agnostic. Could theoretically run over pipes, shared memory, or HTTP. |
| 5 | Header-only | Any risk of ODR violations? | All functions are inline; structs have no non-trivial statics. Safe for multiple TU inclusion. |
| 6 | Structs | Who owns memory behind `const char*` in parse output structs? | Pointers reference jsoncpp's internal buffer. Structs are transient — valid only within the parse callback scope. Document at struct block. Callers who need persistence copy explicitly. |
| 7 | Module Doc | Does header-only DiaDebugProtocol need a module doc per AD-001? | Yes — lightweight module doc so it appears in `dia_modules.py` dependency graph and documents consumers. No Module base class needed. |
| 8 | Handshake | What is the handshake sequence on connect? | Client-first: editor sends handshake with protocol version, server responds accepted/rejected. No other messages before handshake completes. Server closes connection on version mismatch. |
| 9 | Extensibility | Where do system-specific data type constants live? | Each system exports typed StringCRC constants in a dedicated header (e.g., `DiaApplication/DebugDataTypes.h`). Callers use the constant, never raw strings — typo = compile error. Protocol is type-agnostic; subscriptions are just StringCRC values. |

## Status

`Done` - Implementation complete
