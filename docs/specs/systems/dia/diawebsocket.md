# System Spec: DiaWebSocket

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaWebSocket is a reusable WebSocket communication system that wraps the websocketpp library to provide Dia-friendly server and client abstractions. It enables real-time bidirectional communication for multiple use cases: remote debugging (DiaDebugServer), editor-to-game connections (DiaEditor), multiplayer networking (future DiaNetwork), and remote asset streaming. DiaWebSocket handles text and binary messages, manages connection lifecycles, and uses DiaCore threading primitives for thread safety.

## Responsibilities

- **WebSocket Server** - Accept incoming connections, broadcast/send messages, manage client connections
- **WebSocket Client** - Connect to remote servers, send/receive messages, handle reconnection
- **Message Handling** - Support text (UTF-8 strings) and binary (raw bytes) message types
- **Thread Safety** - Protect shared state with DiaCore::Mutex, run server I/O on separate thread
- **Connection Lifecycle** - Handle connect, disconnect, error, and reconnect events
- **Callback System** - Notify users of messages and connection state changes
- **Error Handling** - Consistent error reporting via DiaCore logging
- **websocketpp Abstraction** - Hide websocketpp implementation details, provide clean Dia-style API

## Public Interfaces

### Core Types

**Message:**
```cpp
namespace Dia::WebSocket {
    enum class MessageType {
        kText,    // UTF-8 text (JSON, XML, plain text)
        kBinary   // Raw bytes (serialized structs, compressed data)
    };
    
    struct Message {
        MessageType type;
        const void* data;
        size_t length;
        int connectionId;  // For server messages (which client sent it)
        
        // Helper: Get data as string (for text messages)
        const char* AsText() const {
            DIA_ASSERT(type == MessageType::kText, "Message is not text");
            return static_cast<const char*>(data);
        }
    };
}
```

### WebSocket Server

**Server:**
```cpp
namespace Dia::WebSocket {
    class Server {
    public:
        Server(uint16_t port);
        virtual ~Server();
        
        // Server lifecycle
        bool Start();
        void Stop();
        bool IsRunning() const;
        void Update();  // Process queued messages (called from main thread)
        
        // Sending messages
        void Broadcast(const void* data, size_t length, MessageType type = MessageType::kText);
        void Send(int connectionId, const void* data, size_t length, MessageType type = MessageType::kText);
        
        // Convenience overloads
        void BroadcastText(const char* text);
        void SendText(int connectionId, const char* text);
        
        // Connection management
        int GetConnectionCount() const;
        DynamicArrayC<int, 16> GetActiveConnectionIds() const;
        void CloseConnection(int connectionId);
        
        // Configuration
        void SetMaxConnections(int max);
        void SetMaxMessageSize(size_t bytes);  // Default 1MB
        
        // Callbacks (called from main thread during Update())
        using MessageCallback = std::function<void(int connectionId, const Message& message)>;
        using ConnectionCallback = std::function<void(int connectionId, bool connected)>;
        using ErrorCallback = std::function<void(int connectionId, const char* error)>;
        
        void SetMessageCallback(MessageCallback callback);
        void SetConnectionCallback(ConnectionCallback callback);
        void SetErrorCallback(ErrorCallback callback);
        
    private:
        void* mServerImpl;  // websocketpp::server<...>*
        Dia::Core::Thread* mServerThread;
        Dia::Core::Mutex mConnectionsMutex;
        
        uint16_t mPort;
        bool mIsRunning;
        
        DynamicArrayC<int, 16> mActiveConnections;
        
        MessageCallback mOnMessage;
        ConnectionCallback mOnConnection;
        ErrorCallback mOnError;
        
        // Message queue (server thread → main thread)
        struct QueuedMessage {
            int connectionId;
            Message message;
            DynamicArrayC<char, 1024> dataBuffer;  // Owns message data
        };
        DynamicArrayC<QueuedMessage, 64> mMessageQueue;
        Dia::Core::Mutex mMessageQueueMutex;
    };
}
```

### WebSocket Client

**Client:**
```cpp
namespace Dia::WebSocket {
    enum class ConnectionState {
        kDisconnected,
        kConnecting,
        kConnected,
        kError
    };
    
    class Client {
    public:
        Client();
        virtual ~Client();
        
        // Connection lifecycle
        bool Connect(const char* url);  // e.g., "ws://localhost:8080"
        void Disconnect();
        bool IsConnected() const;
        ConnectionState GetState() const;
        
        void Update();  // Process incoming messages (called from main thread)
        
        // Sending messages
        void Send(const void* data, size_t length, MessageType type = MessageType::kText);
        
        // Convenience overloads
        void SendText(const char* text);
        void SendBinary(const void* data, size_t length);
        
        // Configuration
        void SetReconnectOnDisconnect(bool enable);
        void SetReconnectDelay(float seconds);  // Default 2s
        void SetConnectionTimeout(float seconds);  // Default 10s
        
        // Callbacks (called from main thread during Update())
        using MessageCallback = std::function<void(const Message& message)>;
        using ConnectionCallback = std::function<void(bool connected)>;
        using ErrorCallback = std::function<void(const char* error)>;
        
        void SetMessageCallback(MessageCallback callback);
        void SetConnectionCallback(ConnectionCallback callback);
        void SetErrorCallback(ErrorCallback callback);
        
    private:
        void* mClientImpl;  // websocketpp::client<...>*
        Dia::Core::Thread* mClientThread;
        
        ConnectionState mState;
        const char* mUrl;
        
        bool mReconnectOnDisconnect;
        float mReconnectDelay;
        
        MessageCallback mOnMessage;
        ConnectionCallback mOnConnection;
        ErrorCallback mOnError;
        
        // Message queue (client thread → main thread)
        struct QueuedMessage {
            Message message;
            DynamicArrayC<char, 1024> dataBuffer;  // Owns message data
        };
        DynamicArrayC<QueuedMessage, 64> mMessageQueue;
        Dia::Core::Mutex mMessageQueueMutex;
    };
}
```

## Dependencies

### Required Systems (from Dia)
- **DiaCore** - Containers (DynamicArrayC), StringCRC, logging (DIA_LOG), Threading (Thread, Mutex, ScopedLock), assertions (DIA_ASSERT)

### External Dependencies
- **websocketpp** - WebSocket protocol implementation
  - Version: Latest stable (0.8.2+)
  - License: BSD 3-Clause
  - Mode: Header-only, standalone (no Boost required)
  - Platform: Cross-platform (Windows, Linux, macOS)

## Non-Responsibilities

What DiaWebSocket explicitly does NOT handle:

- **Protocol Definition** - Just transports bytes; higher-level protocols (JSON, Protobuf) are user responsibility
- **Compression** - websocketpp supports it, but not exposed initially (can add later)
- **Encryption (TLS/SSL)** - websocketpp supports wss://, but not exposed initially (security is future work)
- **Authentication** - No built-in auth; users implement via message protocol
- **Message Serialization** - User serializes to JSON/binary before calling Send()
- **High-Level Networking** - No RPC, no sync/replication (that's future DiaNetwork)

## Related Systems

| System | Relationship | Interface |
|--------|--------------|-----------|
| DiaDebugServer | Consumer | Uses WebSocket::Server for remote debugging |
| DiaEditor | Consumer | Uses WebSocket::Client for connecting to games (GameConnectionManager) |
| DiaNetwork (future) | Consumer | Uses WebSocket::Server/Client for multiplayer |
| DiaCore | Dependency | Threading, containers, logging |

## Inherited Binding Decisions

These decisions from parent platform and application specs are binding constraints on DiaWebSocket:

| Source | ID | Decision | Impact on DiaWebSocket |
|--------|----|----------|------------------------|
| Platform | PD-001 | C++ as primary language | DiaWebSocket written in C++ |
| Platform | PD-002 | Windows as primary platform | Test on Windows; websocketpp is cross-platform |
| Platform | PD-003 | Visual Studio + MSBuild | Built as .vcxproj in Dia solution |
| Platform | PD-004 | Spec-driven development | This spec approved before implementation |
| Dia | AD-001 | Module system with YAML frontmatter | Document as `dia.dia.diawebsocket.architecture.module.md` |
| Dia | AD-002 | No STL in public APIs | Public methods use `const char*`, DynamicArrayC, not std::string, std::vector |
| Dia | AD-003 | Namespace convention: `Dia::<Module>::` | All classes in `Dia::WebSocket::` namespace |

## System-Specific Decisions

Decisions specific to DiaWebSocket. Binding decisions constrain all features within this system.

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| DWS-001 | Wrap websocketpp, don't expose it in public API | Isolate external dependency; easier to swap libraries later | Proposed | Yes |
| DWS-002 | Server runs on separate thread, client optionally on thread | Server needs async I/O for multiple connections; client can be main-thread | Proposed | Yes |
| DWS-003 | Update() pattern for callbacks (not raw threads) | Callbacks on main thread avoid synchronization issues; fits Dia patterns | Proposed | Yes |
| DWS-004 | Support text and binary message types | Text for JSON/debug; binary for performance-critical data | Proposed | Yes |
| DWS-005 | Message queue between worker thread and main thread | Decouple I/O from user code; callbacks on predictable thread | Proposed | Yes |
| DWS-006 | No compression or encryption initially | Keep simple for Phase 5; add in Phase 7+ if needed | Proposed | No |
| DWS-007 | websocketpp in standalone mode (no Boost) | Smaller dependency footprint; easier to integrate | Proposed | Yes |
| DWS-008 | Default max message size: 1MB | Reasonable for JSON; prevents memory exhaustion; configurable | Proposed | No |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced on all features · `No` = guidance only

## Features

Features within the DiaWebSocket system (create with `/spec-feature`):

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| WebSocket Server | Accept connections, broadcast/send messages, manage clients | [websocket-server.md](../../features/dia/diawebsocket/websocket-server.md) | Done |
| WebSocket Client | Connect to server, send/receive messages, reconnect logic | [websocket-client.md](../../features/dia/diawebsocket/websocket-client.md) | Done |
| Threading Model | Worker thread + message queues → main thread callbacks | [threading-model.md](../../features/dia/diawebsocket/threading-model.md) | Done |
| Text Message Support | UTF-8 text messages (JSON, XML, plain text) with null termination | [text-message-support.md](../../features/dia/diawebsocket/text-message-support.md) | Done |
| Binary Message Support | Raw byte messages for structs, compressed data, images | [binary-message-support.md](../../features/dia/diawebsocket/binary-message-support.md) | Done |
| Connection Management | Track active connections, close connections, limits | [connection-management.md](../../features/dia/diawebsocket/connection-management.md) | Done |
| Error Handling | Structured error codes, severity levels, unified error format | [error-handling.md](../../features/dia/diawebsocket/error-handling.md) | Done |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Architecture | Why wrap websocketpp? | Isolate external dependency; easier to swap; provide Dia-style API (DWS-001) |
| 2 | Threading | Why separate thread for server? | Async I/O for multiple connections; non-blocking (DWS-002) |
| 3 | Threading | Why Update() pattern for callbacks? | Callbacks on main thread avoid sync issues; fits Dia Module pattern (DWS-003) |
| 4 | Message Types | Text and binary? | Text for JSON/debug protocols; binary for performance (DWS-004) |
| 5 | Message Queue | Why queue messages? | Decouple worker thread from user code; callbacks on predictable thread (DWS-005) |
| 6 | Encryption | Support TLS/SSL? | Not initially (DWS-006) - Phase 5 localhost only; add in Phase 7+ for security |
| 7 | Boost | Require Boost? | No (DWS-007) - websocketpp standalone mode, smaller footprint |
| 8 | Max Message Size | What limit? | Default 1MB (DWS-008) - reasonable for JSON; configurable; prevents memory exhaustion |
| 9 | Reconnection | Auto-reconnect? | Client has optional auto-reconnect with configurable delay |
| 10 | Multiple Servers | Can run multiple Server instances? | Yes - each on different port; no global state |

## Usage Examples

### Server Example (DiaDebugServer use case)

```cpp
#include <DiaWebSocket/Server.h>

using namespace Dia::WebSocket;

class DebugServerModule : public Module {
public:
    void DoStart(const IStartData* startData) override {
        mServer = new Server(8080);
        
        mServer->SetMessageCallback([this](int connId, const Message& msg) {
            // Handle incoming message from editor
            if (msg.type == MessageType::kText) {
                const char* json = msg.AsText();
                // Parse JSON, execute command, send response
            }
        });
        
        mServer->SetConnectionCallback([this](int connId, bool connected) {
            if (connected) {
                DIA_LOG("Editor connected: %d", connId);
            } else {
                DIA_LOG("Editor disconnected: %d", connId);
            }
        });
        
        mServer->Start();
    }
    
    void DoUpdate() override {
        mServer->Update();  // Process incoming messages, fire callbacks
        
        // Broadcast metrics every 500ms
        mMetricsTimer += GetDeltaTime();
        if (mMetricsTimer >= 0.5f) {
            const char* metricsJson = "{ \"fps\": 60.0, \"memory_mb\": 512 }";
            mServer->BroadcastText(metricsJson);
            mMetricsTimer = 0.0f;
        }
    }
    
    void DoStop() override {
        mServer->Stop();
        delete mServer;
    }
    
private:
    Server* mServer;
    float mMetricsTimer = 0.0f;
};
```

### Client Example (DiaEditor use case)

```cpp
#include <DiaWebSocket/Client.h>

using namespace Dia::WebSocket;

class GameConnectionManager {
public:
    void ConnectToGame(const char* address, uint16_t port) {
        char url[256];
        sprintf(url, "ws://%s:%d", address, port);
        
        mClient = new Client();
        
        mClient->SetMessageCallback([this](const Message& msg) {
            // Handle incoming data from game
            if (msg.type == MessageType::kText) {
                const char* json = msg.AsText();
                // Parse JSON, update UI
            }
        });
        
        mClient->SetConnectionCallback([this](bool connected) {
            if (connected) {
                // Subscribe to data
                mClient->SendText("{ \"type\": \"subscribe\", \"data_type\": \"processing_unit_state\" }");
            }
        });
        
        mClient->Connect(url);
    }
    
    void Update() {
        if (mClient) {
            mClient->Update();  // Process incoming messages
        }
    }
    
    void SendCommand(const char* commandJson) {
        if (mClient && mClient->IsConnected()) {
            mClient->SendText(commandJson);
        }
    }
    
private:
    Client* mClient = nullptr;
};
```

## Status

`Done` - All 7 features implemented and tested (42 GoogleTests passing). Server, Client, Threading, Text/Binary messages, Connection Management, Error Handling.

## Notes

**websocketpp Integration:**
DiaWebSocket uses websocketpp in standalone mode (no Boost dependency). The implementation uses websocketpp's async I/O model with a custom event loop integrated into DiaCore::Thread.

**Thread Safety Model:**
```
Server Thread:               Main Thread:
├─ Accept connections        ├─ User calls Update()
├─ Receive messages   ────>  ├─ Process message queue
├─ Queue messages            ├─ Fire callbacks
└─ Send queued data   <────  └─ User calls Send/Broadcast
```

All user callbacks fire on the main thread (during Update()), avoiding synchronization issues. The worker thread only handles I/O and queuing.

**Performance Considerations:**
- Message queue size: 64 messages (configurable)
- If queue fills, new messages dropped (overflow handling)
- Max message size: 1MB default (configurable)
- Text messages copied to queue (UTF-8 validation)
- Binary messages copied to queue (raw bytes)

**Future Enhancements (Phase 7+):**
- TLS/SSL support (wss:// URLs)
- Compression (per-message deflate)
- Authentication/authorization hooks
- Message fragmentation for large payloads
- Ping/pong keepalive configuration
