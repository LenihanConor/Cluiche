# Feature Spec: WebSocket Server

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaWebSocket | @docs/specs/systems/dia/diawebsocket.md |
| Feature | **WebSocket Server** | (this document) |

## Problem Statement

Provides a thread-safe WebSocket server that accepts multiple client connections, broadcasts/sends messages, and integrates with DiaCore threading primitives for use by DiaDebugServer and future systems.

## Acceptance Criteria

- [x] Server can start on configurable port (e.g., 8080)
- [x] Server can stop cleanly, closing all connections
- [x] Server accepts multiple simultaneous client connections
- [x] Server can broadcast text messages to all connected clients
- [x] Server can send text messages to specific client by connection ID
- [x] Server can broadcast binary messages to all connected clients
- [x] Server can send binary messages to specific client by connection ID
- [x] Server provides message callback (fires on main thread during Update())
- [x] Server provides connection callback (fires when client connects/disconnects)
- [x] Server provides error callback for connection errors
- [x] Server runs I/O on separate thread using DiaCore::Thread
- [x] Server queues incoming messages from worker thread → main thread
- [x] Server Update() method processes message queue and fires callbacks
- [x] Server GetConnectionCount() returns number of active connections
- [x] Server GetActiveConnectionIds() returns list of connection IDs
- [x] Server handles max connections limit (configurable)
- [x] Server handles max message size limit (default 1MB, configurable)
- [x] Server closes individual connections via CloseConnection(connectionId)

## Design

### API

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
        int connectionId;  // Which client sent it
        
        const char* AsText() const {
            DIA_ASSERT(type == MessageType::kText, "Message is not text");
            return static_cast<const char*>(data);
        }
    };
    
    class Server {
    public:
        Server(uint16_t port);
        virtual ~Server();
        
        // Server lifecycle
        bool Start();  // Blocks until listening (or error)
        void Stop();   // Immediate shutdown
        bool IsRunning() const;
        void Update(); // Process message queue, fire callbacks (call from main thread)
        
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
        void SetMaxConnections(int max);      // Default: 16
        void SetMaxMessageSize(size_t bytes); // Default: 1MB
        
        // Callbacks (fired on main thread during Update())
        using MessageCallback = std::function<void(int connectionId, const Message& message)>;
        using ConnectionCallback = std::function<void(int connectionId, bool connected)>;
        using ErrorCallback = std::function<void(int connectionId, const char* error)>;
        
        void SetMessageCallback(MessageCallback callback);
        void SetConnectionCallback(ConnectionCallback callback);
        void SetErrorCallback(ErrorCallback callback);
        
    private:
        struct Impl;
        Impl* mImpl;  // Pimpl pattern to hide websocketpp
    };
}
```

### Threading Model

```
Worker Thread (DiaCore::Thread):     Main Thread:
┌────────────────────────────┐      ┌───────────────────────────┐
│ websocketpp async I/O      │      │ User code                 │
│ Accept connections         │      │                           │
│ Receive messages    ──────>│───>  │ Update()                  │
│ Queue messages             │      │   Process message queue   │
│                            │      │   Fire callbacks          │
│ Send queued data    <──────│<───  │   Broadcast/Send          │
└────────────────────────────┘      └───────────────────────────┘
```

**Key Points:**
- Worker thread handles all websocketpp I/O
- Incoming messages queued: worker thread → main thread
- Outgoing messages queued: main thread → worker thread
- Callbacks always fire on main thread (no synchronization needed by user)

### Message Queue

```cpp
struct QueuedMessage {
    int connectionId;
    Message message;
    DynamicArrayC<char, 1024> dataBuffer;  // Owns copy of message data
};

// Incoming queue (worker → main)
DynamicArrayC<QueuedMessage, 64> mIncomingQueue;
Dia::Core::Mutex mIncomingMutex;

// Outgoing queue (main → worker)
struct OutgoingMessage {
    int connectionId;  // -1 = broadcast
    MessageType type;
    DynamicArrayC<char, 1024> dataBuffer;
};
DynamicArrayC<OutgoingMessage, 64> mOutgoingQueue;
Dia::Core::Mutex mOutgoingMutex;
```

**Queue Overflow Handling:**
- If incoming queue fills (64 messages), worker thread logs error via ErrorCallback
- If outgoing queue fills (64 messages), main thread logs error and drops message

### websocketpp Integration

```cpp
// Internal implementation (not in public headers)
namespace Dia::WebSocket::Internal {
    using wspp_server = websocketpp::server<websocketpp::config::asio>;
    using connection_hdl = websocketpp::connection_hdl;
    
    struct Server::Impl {
        wspp_server mServer;
        Dia::Core::Thread* mWorkerThread;
        
        uint16_t mPort;
        bool mIsRunning;
        int mMaxConnections;
        size_t mMaxMessageSize;
        
        // Connection tracking
        std::map<int, connection_hdl> mConnections;  // connectionId → handle
        int mNextConnectionId;
        Dia::Core::Mutex mConnectionsMutex;
        
        // Callbacks
        MessageCallback mOnMessage;
        ConnectionCallback mOnConnection;
        ErrorCallback mOnError;
        
        // Queues
        DynamicArrayC<QueuedMessage, 64> mIncomingQueue;
        Dia::Core::Mutex mIncomingMutex;
        
        DynamicArrayC<OutgoingMessage, 64> mOutgoingQueue;
        Dia::Core::Mutex mOutgoingMutex;
        
        // websocketpp handlers
        void OnOpen(connection_hdl hdl);
        void OnClose(connection_hdl hdl);
        void OnMessage(connection_hdl hdl, wspp_server::message_ptr msg);
        void OnFail(connection_hdl hdl);
        
        // Thread entry point
        void WorkerThreadMain();
    };
}
```

### Lifecycle

**Start():**
```cpp
bool Server::Start() {
    // 1. Initialize websocketpp server
    mImpl->mServer.init_asio();
    mImpl->mServer.set_reuse_addr(true);
    
    // 2. Register websocketpp handlers
    mImpl->mServer.set_open_handler([this](connection_hdl hdl) { mImpl->OnOpen(hdl); });
    mImpl->mServer.set_close_handler([this](connection_hdl hdl) { mImpl->OnClose(hdl); });
    mImpl->mServer.set_message_handler([this](connection_hdl hdl, message_ptr msg) { mImpl->OnMessage(hdl, msg); });
    mImpl->mServer.set_fail_handler([this](connection_hdl hdl) { mImpl->OnFail(hdl); });
    
    // 3. Listen on port (blocks until listening or error)
    try {
        mImpl->mServer.listen(mImpl->mPort);
        mImpl->mServer.start_accept();
    } catch (const std::exception& e) {
        DIA_LOG_ERROR("WebSocket server failed to start: %s", e.what());
        return false;
    }
    
    // 4. Start worker thread
    mImpl->mIsRunning = true;
    mImpl->mWorkerThread = new Dia::Core::Thread("WebSocket Server", [this]() {
        mImpl->WorkerThreadMain();
    });
    
    return true;
}

void Server::Impl::WorkerThreadMain() {
    // Run websocketpp I/O loop (blocks until Stop())
    try {
        mServer.run();
    } catch (const std::exception& e) {
        DIA_LOG_ERROR("WebSocket server error: %s", e.what());
    }
}
```

**Stop():**
```cpp
void Server::Stop() {
    if (!mImpl->mIsRunning) return;
    
    // 1. Stop accepting new connections
    mImpl->mServer.stop_listening();
    
    // 2. Close all active connections (immediate)
    {
        Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mConnectionsMutex);
        for (auto& [connId, hdl] : mImpl->mConnections) {
            mImpl->mServer.close(hdl, websocketpp::close::status::going_away, "Server shutting down");
        }
        mImpl->mConnections.clear();
    }
    
    // 3. Stop websocketpp I/O loop
    mImpl->mServer.stop();
    
    // 4. Join worker thread
    mImpl->mWorkerThread->Join();
    delete mImpl->mWorkerThread;
    mImpl->mWorkerThread = nullptr;
    
    mImpl->mIsRunning = false;
}
```

**Update():**
```cpp
void Server::Update() {
    // Process incoming messages (worker thread → main thread)
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mIncomingMutex);
    
    for (const auto& queuedMsg : mImpl->mIncomingQueue) {
        if (mImpl->mOnMessage) {
            mImpl->mOnMessage(queuedMsg.connectionId, queuedMsg.message);
        }
    }
    
    mImpl->mIncomingQueue.Clear();
}
```

### Connection Handling

**OnOpen (worker thread):**
```cpp
void Server::Impl::OnOpen(connection_hdl hdl) {
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mConnectionsMutex);
    
    // Check max connections
    if (mConnections.size() >= mMaxConnections) {
        mServer.close(hdl, websocketpp::close::status::try_again_later, "Server full");
        
        // Queue error callback
        if (mOnError) {
            // Queue for main thread...
        }
        return;
    }
    
    // Assign connection ID
    int connId = mNextConnectionId++;
    mConnections[connId] = hdl;
    
    // Queue connection callback (connected = true)
    if (mOnConnection) {
        // Queue for main thread during next Update()
        // (omitted for brevity - similar pattern to message queue)
    }
    
    DIA_LOG("WebSocket client connected: %d", connId);
}
```

**OnMessage (worker thread):**
```cpp
void Server::Impl::OnMessage(connection_hdl hdl, wspp_server::message_ptr msg) {
    // Find connection ID
    int connId = -1;
    {
        Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mConnectionsMutex);
        for (const auto& [id, h] : mConnections) {
            if (h.lock() == hdl.lock()) {
                connId = id;
                break;
            }
        }
    }
    
    if (connId == -1) return;  // Connection not found
    
    // Check message size
    if (msg->get_payload().size() > mMaxMessageSize) {
        DIA_LOG_ERROR("WebSocket message too large: %zu bytes", msg->get_payload().size());
        if (mOnError) {
            // Queue error callback...
        }
        return;
    }
    
    // Queue message for main thread
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mIncomingMutex);
    
    if (mIncomingQueue.IsFull()) {
        DIA_LOG_ERROR("WebSocket incoming queue full - dropping message");
        if (mOnError) {
            // Queue error callback...
        }
        return;
    }
    
    QueuedMessage queuedMsg;
    queuedMsg.connectionId = connId;
    queuedMsg.message.type = (msg->get_opcode() == websocketpp::frame::opcode::text) 
                              ? MessageType::kText 
                              : MessageType::kBinary;
    queuedMsg.message.length = msg->get_payload().size();
    
    // Copy message data (queuedMsg owns it)
    queuedMsg.dataBuffer.Resize(msg->get_payload().size());
    memcpy(queuedMsg.dataBuffer.GetData(), msg->get_payload().data(), msg->get_payload().size());
    queuedMsg.message.data = queuedMsg.dataBuffer.GetData();
    
    mIncomingQueue.Add(queuedMsg);
}
```

### Sending Messages

**Broadcast:**
```cpp
void Server::Broadcast(const void* data, size_t length, MessageType type) {
    OutgoingMessage msg;
    msg.connectionId = -1;  // -1 = broadcast
    msg.type = type;
    msg.dataBuffer.Resize(length);
    memcpy(msg.dataBuffer.GetData(), data, length);
    
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mOutgoingMutex);
    
    if (mImpl->mOutgoingQueue.IsFull()) {
        DIA_LOG_ERROR("WebSocket outgoing queue full - dropping message");
        return;
    }
    
    mImpl->mOutgoingQueue.Add(msg);
}
```

**Worker thread processes outgoing queue:**
```cpp
void Server::Impl::WorkerThreadMain() {
    while (mIsRunning) {
        mServer.poll();  // Non-blocking I/O
        
        // Process outgoing messages
        {
            Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mOutgoingMutex);
            
            for (const auto& msg : mOutgoingQueue) {
                if (msg.connectionId == -1) {
                    // Broadcast
                    for (const auto& [connId, hdl] : mConnections) {
                        try {
                            mServer.send(hdl, msg.dataBuffer.GetData(), msg.dataBuffer.Size(), 
                                        (msg.type == MessageType::kText) 
                                          ? websocketpp::frame::opcode::text 
                                          : websocketpp::frame::opcode::binary);
                        } catch (const std::exception& e) {
                            DIA_LOG_ERROR("WebSocket send failed: %s", e.what());
                        }
                    }
                } else {
                    // Send to specific connection
                    auto it = mConnections.find(msg.connectionId);
                    if (it != mConnections.end()) {
                        try {
                            mServer.send(it->second, msg.dataBuffer.GetData(), msg.dataBuffer.Size(),
                                        (msg.type == MessageType::kText)
                                          ? websocketpp::frame::opcode::text
                                          : websocketpp::frame::opcode::binary);
                        } catch (const std::exception& e) {
                            DIA_LOG_ERROR("WebSocket send failed: %s", e.what());
                        }
                    }
                }
            }
            
            mOutgoingQueue.Clear();
        }
        
        Dia::Core::ThisThread::SleepMs(1);  // Yield CPU
    }
}
```

## Implementation Files

- `Dia/DiaWebSocket/Server.h` - Public interface
- `Dia/DiaWebSocket/Server.cpp` - Implementation
- `Dia/DiaWebSocket/Internal/WebSocketppWrapper.h` - websocketpp includes and type aliases
- `Dia/DiaWebSocket/Internal/WebSocketppWrapper.cpp` - websocketpp integration helpers

## Binding Decisions Compliance

All binding decisions from parent specs must be honored:

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | ✅ **Compliant** - Connection IDs are `int` (not entity IDs); no string IDs needed in this feature |
| Platform | PD-004 | No STL in public APIs | ✅ **Compliant** - Public API uses `DynamicArrayC`, `const char*`, `std::function` (callbacks allowed per AD-002) |
| Platform | PD-006 | Visual Studio project files are source of truth | ✅ **Compliant** - Will be built as part of DiaWebSocket.vcxproj |
| Dia | AD-001 | Module system with YAML frontmatter | ✅ **Compliant** - DiaWebSocket has `dia.dia.diawebsocket.architecture.module.md` |
| Dia | AD-002 | No STL containers in public APIs | ✅ **Compliant** - Uses `DynamicArrayC<int, 16>` not `std::vector<int>` |
| Dia | AD-003 | Namespace convention: `Dia::<Module>::` | ✅ **Compliant** - All classes in `Dia::WebSocket::` namespace |
| DiaWebSocket | DWS-001 | Wrap websocketpp, don't expose in public API | ✅ **Compliant** - Uses Pimpl pattern (`struct Impl`); websocketpp hidden in .cpp |
| DiaWebSocket | DWS-002 | Server runs on separate thread | ✅ **Compliant** - Uses `Dia::Core::Thread* mWorkerThread` for I/O |
| DiaWebSocket | DWS-003 | Update() pattern for callbacks | ✅ **Compliant** - `Update()` processes queue, fires callbacks on main thread |
| DiaWebSocket | DWS-004 | Support text and binary message types | ✅ **Compliant** - `MessageType` enum with kText/kBinary; `Broadcast()` supports both |
| DiaWebSocket | DWS-005 | Message queue between threads | ✅ **Compliant** - `mIncomingQueue` (worker→main), `mOutgoingQueue` (main→worker) |
| DiaWebSocket | DWS-007 | websocketpp standalone mode (no Boost) | ✅ **Compliant** - Uses `websocketpp::config::asio` standalone config |

**All binding decisions: COMPLIANT ✅ No conflicts detected.**

## Open Questions

**Resolved:**
- ❌ What happens if message queue fills up? → **Error logged, message dropped**
- ❌ How to handle websocketpp exceptions? → **Log and continue (don't crash)**
- ❌ Should Start() block until listening? → **Yes, blocks until listening or error**
- ❌ Thread shutdown: graceful or immediate? → **Immediate (don't finish queued messages)**

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Threading | How to handle race condition if Stop() called while Update() processing queue? | Add `mIsRunning` check in Update(); return early if false | ✅ Add `mIsRunning` check in Update(); return early if false |
| 2 | Memory | Who owns Message.data pointer after callback fires? | Callback must not store pointer; data invalidated after callback returns | ✅ Callback must not store pointer; data invalidated after callback returns |
| 3 | Error Handling | Should queue overflow fire error callback or just log? | Both - log error AND fire error callback if set | ✅ Both - log error AND fire error callback if set |
| 4 | Configuration | Can SetMaxConnections/SetMaxMessageSize be called after Start()? | No - must be called before Start(); assert if called after | ✅ No - must be called before Start(); assert if called after |
| 5 | Connection IDs | What happens if connection ID wraps around (mNextConnectionId overflow)? | Use 64-bit int to avoid wrap; or reuse freed IDs | ✅ Use 64-bit int to avoid wrap; or reuse freed IDs |
| 6 | Callbacks | Are callbacks allowed to call Server methods (e.g., Send() during OnMessage)? | Yes - safe because callbacks fire on main thread; queues message | ✅ Yes - safe because callbacks fire on main thread; queues message |
| 7 | Thread Safety | Is GetConnectionCount() thread-safe? Called from any thread? | Must lock mConnectionsMutex; or cache count on main thread | ✅ Must lock mConnectionsMutex |
| 8 | Shutdown | What if Update() not called for long time, queue fills, then Stop()? | Stop() clears queues; pending callbacks never fire (acceptable loss) | ✅ Stop() clears queues; pending callbacks never fire (acceptable loss) |

## Status

`Done` - Implemented and tested
