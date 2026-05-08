# Feature Spec: Error Handling

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaWebSocket | @docs/specs/systems/dia/diawebsocket.md |
| Feature | **Error Handling** | (this document) |

## Problem Statement

Replaces raw `const char*` error strings with a structured, unified error reporting system using typed error codes and severity levels, giving consumers (DiaDebugServer, DiaEditor) consistent, programmatic error handling across both Server and Client.

## Acceptance Criteria

- [ ] Define `ErrorCode` enum with all error conditions shared by Server and Client
- [ ] Define `ErrorSeverity` enum (kWarning, kError, kFatal)
- [ ] Define `Error` struct containing: ErrorCode, ErrorSeverity, connectionId, and a human-readable message
- [ ] Server ErrorCallback signature changes from `(int, const char*)` to `(const Error&)`
- [ ] Client ErrorCallback signature changes from `(const char*)` to `(const Error&)`
- [ ] All existing error paths in Server.cpp produce the correct ErrorCode and severity
- [ ] All existing error paths in Client.cpp produce the correct ErrorCode and severity
- [ ] Queue overflow errors use severity kWarning (message dropped but system continues)
- [ ] Message too large errors use severity kError
- [ ] Connection rejected (server full) uses severity kWarning
- [ ] Connection failure uses severity kError
- [ ] Max reconnect attempts exhausted uses severity kError
- [ ] Error struct includes a `GetMessage()` method returning `const char*` for human-readable text
- [ ] Error struct includes a `GetCodeName()` method returning the enum value as a string for logging
- [ ] Existing DiaCore::Log calls remain alongside callback delivery (errors are both logged and dispatched)
- [ ] Update existing tests to use new Error struct in callbacks

## Design

### Error Types

```cpp
namespace Dia::WebSocket {

    enum class ErrorCode
    {
        kNone,

        // Connection errors
        kConnectionFailed,          // websocketpp connection failure
        kConnectionTimeout,         // Client connect timed out
        kConnectionRejected,        // Server full, connection refused
        kConnectionCloseFailed,     // Error during close handshake

        // Message errors
        kMessageTooLarge,           // Exceeds max message size
        kMessageQueueFull,          // Incoming or outgoing queue overflow
        kSendFailed,                // websocketpp send exception

        // Reconnection errors
        kReconnectFailed,           // Single reconnect attempt failed
        kReconnectExhausted,        // Max reconnect attempts reached

        // Internal errors
        kInternalError              // Catch-all for unexpected websocketpp exceptions
    };

    enum class ErrorSeverity
    {
        kWarning,   // Recoverable; operation skipped but system continues
        kError,     // Significant problem; may require user action
        kFatal      // Unrecoverable; server/client should be stopped
    };

    struct Error
    {
        ErrorCode code;
        ErrorSeverity severity;
        int connectionId;           // -1 if not connection-specific
        char message[256];          // Human-readable description

        const char* GetMessage() const { return message; }
        const char* GetCodeName() const;  // Returns enum name as string
    };
}
```

### Severity Mapping

| ErrorCode | Severity | Rationale |
|-----------|----------|-----------|
| kConnectionFailed | kError | Connection lost or could not be established |
| kConnectionTimeout | kError | Client blocked for full timeout duration |
| kConnectionRejected | kWarning | Server at capacity; client can retry later |
| kConnectionCloseFailed | kWarning | Close handshake issue; connection will be cleaned up |
| kMessageTooLarge | kError | Message violated configured limit |
| kMessageQueueFull | kWarning | Dropped message; system continues |
| kSendFailed | kError | websocketpp send threw exception |
| kReconnectFailed | kWarning | Single attempt failed; more attempts may follow |
| kReconnectExhausted | kError | Gave up reconnecting; user intervention needed |
| kInternalError | kError | Unexpected exception in worker thread |

### Callback Signature Changes

**Server (before):**
```cpp
using ErrorCallback = std::function<void(int connectionId, const char* error)>;
```

**Server (after):**
```cpp
using ErrorCallback = std::function<void(const Error& error)>;
```

**Client (before):**
```cpp
using ErrorCallback = std::function<void(const char* error)>;
```

**Client (after):**
```cpp
using ErrorCallback = std::function<void(const Error& error)>;
```

The `connectionId` moves into the `Error` struct, unifying both signatures.

### Error Construction Helper

```cpp
// Internal helper (in .cpp files)
namespace Dia::WebSocket::Internal {
    Error MakeError(ErrorCode code, ErrorSeverity severity, int connectionId, const char* format, ...);
}
```

This replaces scattered string formatting with a single construction point.

### QueuedEvent Changes

The existing `QueuedEvent::Type::kError` path changes to carry an `Error` struct instead of raw text in `dataBuffer`:

```cpp
struct QueuedEvent {
    // ... existing fields ...
    Error error;  // Used when eventType == kError
};
```

### Integration with DiaCore::Log

All errors continue to be logged via `Dia::Core::Log::OutputLine` / `OutputVaradicLine` in addition to being dispatched through the callback. The severity maps to log level:
- kWarning → `OutputLine` (informational)
- kError → `OutputVaradicLine` with error prefix
- kFatal → `OutputVaradicLine` with fatal prefix

## Implementation Files

- `Dia/DiaWebSocket/Error.h` - New file: ErrorCode, ErrorSeverity, Error struct
- `Dia/DiaWebSocket/Server.h` - Update ErrorCallback signature
- `Dia/DiaWebSocket/Client.h` - Update ErrorCallback signature
- `Dia/DiaWebSocket/Server.cpp` - Replace string errors with Error construction
- `Dia/DiaWebSocket/Client.cpp` - Replace string errors with Error construction
- `Dia/DiaWebSocket/Internal/WebSocketppWrapper.h` - Add Error field to QueuedEvent
- `Dia/DiaWebSocket/DiaWebSocket.vcxproj` - Add Error.h
- `Dia/DiaWebSocket/DiaWebSocket.vcxproj.filters` - Add Error.h

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | ✅ **Compliant** - Error codes are enums, not string IDs; StringCRC not applicable |
| Platform | PD-004 | No STL in public APIs | ✅ **Compliant** - Error struct uses `char[256]` not `std::string`; callback uses `std::function` (allowed per AD-002) |
| Platform | PD-006 | Visual Studio project files are source of truth | ✅ **Compliant** - New Error.h added to DiaWebSocket.vcxproj |
| Dia | AD-001 | Module system with YAML frontmatter | ✅ **Compliant** - DiaWebSocket module doc already exists |
| Dia | AD-002 | No STL containers in public APIs | ✅ **Compliant** - Error struct uses fixed-size char array, no STL types |
| Dia | AD-003 | Namespace convention: `Dia::<Module>::` | ✅ **Compliant** - All types in `Dia::WebSocket::` namespace |
| DiaWebSocket | DWS-001 | Wrap websocketpp, don't expose in public API | ✅ **Compliant** - Error codes abstract websocketpp failures; no websocketpp types exposed |
| DiaWebSocket | DWS-003 | Update() pattern for callbacks | ✅ **Compliant** - Error callbacks still fire on main thread during Update() |
| DiaWebSocket | DWS-005 | Message queue between threads | ✅ **Compliant** - Errors queued via QueuedEvent, delivered on main thread |
| DiaWebSocket | DWS-007 | websocketpp standalone mode (no Boost) | ✅ **Compliant** - No new dependencies |

**All binding decisions: COMPLIANT. No conflicts detected.**

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | API Breaking Change | ErrorCallback signature changes break existing consumers — how to handle? | This is pre-release code; update all call sites (tests, future DiaDebugServer) at implementation time | ✅ Pre-release code; update all call sites at implementation time |
| 2 | Message Buffer | Is char[256] sufficient for error messages? | Yes — error messages are short descriptive strings, not payloads; 256 is generous | ✅ Yes, 256 is sufficient; truncate if message exceeds buffer |
| 3 | GetCodeName | Should GetCodeName() be a switch statement or a lookup table? | Switch statement — simple, no init order issues, compiler warns on missing cases | ✅ Switch statement |
| 4 | Fatal Errors | Should kFatal severity trigger automatic Stop() on Server/Client? | No — just report; let the consumer decide whether to stop | ✅ No — just report; consumer decides |
| 5 | Error.h Include | Should Message.h include Error.h or should they be independent headers? | Independent — Error.h is standalone; consumers include it separately if needed | ✅ Include Error.h from Message.h — consumers get both types from a single include |

## Status

`Done` - Implemented and tested
