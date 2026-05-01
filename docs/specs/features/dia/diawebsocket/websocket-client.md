# Feature Spec: WebSocket Client

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaWebSocket | @docs/specs/systems/dia/diawebsocket.md |
| Feature | **WebSocket Client** | (this document) |

## Problem Statement

Provides a thread-safe WebSocket client that connects to remote servers, sends/receives messages, handles reconnection, and integrates with DiaCore threading for use by DiaEditor's GameConnectionManager and future systems.

## Acceptance Criteria

- [x] Client can connect to WebSocket server via URL (e.g., "ws://localhost:8080")
- [x] Client can disconnect cleanly
- [x] Client can send text messages
- [x] Client can send binary messages
- [x] Client provides message callback (fires on main thread during Update())
- [x] Client provides connection callback (fires when connected/disconnected)
- [x] Client provides error callback for connection errors
- [x] Client runs I/O on separate thread using DiaCore::Thread
- [x] Client queues incoming messages from worker thread → main thread
- [x] Client Update() method processes message queue and fires callbacks
- [x] Client IsConnected() returns current connection state
- [x] Client GetState() returns ConnectionState enum (kDisconnected, kConnecting, kConnected, kError)
- [x] Client supports auto-reconnect on disconnect (configurable)
- [x] Client supports reconnect delay configuration (default 2s)
- [x] Client supports connection timeout configuration (default 10s)
- [x] Client handles max message size limit (default 1MB, configurable)

## Design

### API

```cpp
namespace Dia::WebSocket {
    enum class ConnectionState {
        kDisconnected,  // Not connected
        kConnecting,    // Connection in progress
        kConnected,     // Successfully connected
        kError          // Connection failed
    };
    
    class Client {
    public:
        Client();
        virtual ~Client();
        
        // Connection lifecycle
        bool Connect(const char* url);  // Blocks until connected (or timeout/error)
        void Disconnect();
        bool IsConnected() const;
        ConnectionState GetState() const;
        
        void Update();  // Process message queue, fire callbacks (call from main thread)
        
        // Sending messages
        void Send(const void* data, size_t length, MessageType type = MessageType::kText);
        
        // Convenience overloads
        void SendText(const char* text);
        void SendBinary(const void* data, size_t length);
        
        // Configuration (must be called before Connect())
        void SetReconnectOnDisconnect(bool enable);        // Default: false
        void SetReconnectDelay(float seconds);             // Default: 2s
        void SetReconnectMaxAttempts(int maxAttempts);     // Default: 5
        void SetConnectionTimeout(float seconds);          // Default: 10s
        void SetMaxMessageSize(size_t bytes);              // Default: 1MB
        
        // Callbacks (fired on main thread during Update())
        using MessageCallback = std::function<void(const Message& message)>;
        using ConnectionCallback = std::function<void(bool connected)>;
        using ErrorCallback = std::function<void(const char* error)>;
        
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
│ Connect to server          │      │                           │
│ Receive messages    ──────>│───>  │ Update()                  │
│ Queue messages             │      │   Process message queue   │
│                            │      │   Fire callbacks          │
│ Send queued data    <──────│<───  │   Send                    │
│                            │      │                           │
│ Auto-reconnect (if enabled)│      │                           │
└────────────────────────────┘      └───────────────────────────┘
```

**Key Points:**
- Worker thread handles all websocketpp I/O
- Incoming messages queued: worker thread → main thread
- Outgoing messages queued: main thread → worker thread
- Callbacks always fire on main thread (no synchronization needed by user)
- Auto-reconnect happens on worker thread (transparent to user)

### State Machine

```
┌──────────────┐
│ kDisconnected│
└───────┬──────┘
        │ Connect() called
        ▼
┌──────────────┐  timeout/error   ┌────────┐
│ kConnecting  │─────────────────>│ kError │
└───────┬──────┘                  └────────┘
        │ connected successfully
        ▼
┌──────────────┐  disconnect/error
│ kConnected   │─────────────────> (back to kDisconnected or kConnecting if auto-reconnect)
└──────────────┘
```

### websocketpp Integration

```cpp
// Internal implementation
namespace Dia::WebSocket::Internal {
    using wspp_client = websocketpp::client<websocketpp::config::asio_client>;
    using connection_ptr = wspp_client::connection_ptr;
    
    struct Client::Impl {
        wspp_client mClient;
        Dia::Core::Thread* mWorkerThread;
        
        ConnectionState mState;
        const char* mUrl;
        
        // Configuration
        bool mReconnectOnDisconnect;
        float mReconnectDelay;
        int mReconnectMaxAttempts;
        int mReconnectAttemptCount;
        float mConnectionTimeout;
        size_t mMaxMessageSize;
        
        // Callbacks
        MessageCallback mOnMessage;
        ConnectionCallback mOnConnection;
        ErrorCallback mOnError;
        
        // Message queues
        DynamicArrayC<QueuedMessage, 64> mIncomingQueue;
        Dia::Core::Mutex mIncomingMutex;
        
        DynamicArrayC<OutgoingMessage, 64> mOutgoingQueue;
        Dia::Core::Mutex mOutgoingMutex;
        
        // State
        bool mIsRunning;
        connection_ptr mConnection;
        Dia::Core::Mutex mStateMutex;
        
        // websocketpp handlers
        void OnOpen(websocketpp::connection_hdl hdl);
        void OnClose(websocketpp::connection_hdl hdl);
        void OnMessage(websocketpp::connection_hdl hdl, wspp_client::message_ptr msg);
        void OnFail(websocketpp::connection_hdl hdl);
        
        // Thread entry point
        void WorkerThreadMain();
        
        // Auto-reconnect
        void AttemptReconnect();
    };
}
```

### Lifecycle

**Connect():**
```cpp
bool Client::Connect(const char* url) {
    // If already connected, log and ignore
    if (mImpl->mState == ConnectionState::kConnected) {
        DIA_LOG_WARNING("WebSocket client already connected - ignoring Connect() call");
        return true;
    }
    
    // If connecting, log and ignore
    if (mImpl->mState == ConnectionState::kConnecting) {
        DIA_LOG_WARNING("WebSocket client already connecting - ignoring Connect() call");
        return false;
    }
    
    mImpl->mUrl = url;
    mImpl->mReconnectAttemptCount = 0;
    
    // Initialize websocketpp client
    mImpl->mClient.init_asio();
    mImpl->mClient.set_open_handler([this](connection_hdl hdl) { mImpl->OnOpen(hdl); });
    mImpl->mClient.set_close_handler([this](connection_hdl hdl) { mImpl->OnClose(hdl); });
    mImpl->mClient.set_message_handler([this](connection_hdl hdl, message_ptr msg) { mImpl->OnMessage(hdl, msg); });
    mImpl->mClient.set_fail_handler([this](connection_hdl hdl) { mImpl->OnFail(hdl); });
    
    // Start worker thread
    mImpl->mIsRunning = true;
    mImpl->mWorkerThread = new Dia::Core::Thread("WebSocket Client", [this]() {
        mImpl->WorkerThreadMain();
    });
    
    // Attempt connection
    {
        Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mStateMutex);
        mImpl->mState = ConnectionState::kConnecting;
    }
    
    try {
        websocketpp::lib::error_code ec;
        mImpl->mConnection = mImpl->mClient.get_connection(url, ec);
        
        if (ec) {
            DIA_LOG_ERROR("WebSocket connection failed: %s", ec.message().c_str());
            mImpl->mState = ConnectionState::kError;
            return false;
        }
        
        mImpl->mClient.connect(mImpl->mConnection);
        
    } catch (const std::exception& e) {
        DIA_LOG_ERROR("WebSocket connection exception: %s", e.what());
        mImpl->mState = ConnectionState::kError;
        return false;
    }
    
    // Block until connected or timeout
    float elapsed = 0.0f;
    while (mImpl->mState == ConnectionState::kConnecting && elapsed < mImpl->mConnectionTimeout) {
        Dia::Core::ThisThread::SleepMs(100);
        elapsed += 0.1f;
    }
    
    if (mImpl->mState == ConnectionState::kConnected) {
        return true;
    } else {
        DIA_LOG_ERROR("WebSocket connection timeout after %.1fs", elapsed);
        mImpl->mState = ConnectionState::kError;
        return false;
    }
}

void Client::Impl::WorkerThreadMain() {
    while (mIsRunning) {
        mClient.poll();  // Non-blocking I/O
        
        // Process outgoing messages (same as Server)
        // ... (omitted for brevity)
        
        Dia::Core::ThisThread::SleepMs(1);
    }
}
```

**Disconnect():**
```cpp
void Client::Disconnect() {
    if (mImpl->mState == ConnectionState::kDisconnected) return;
    
    // Disable auto-reconnect
    bool wasAutoReconnect = mImpl->mReconnectOnDisconnect;
    mImpl->mReconnectOnDisconnect = false;
    
    // Close connection
    if (mImpl->mConnection) {
        try {
            mImpl->mClient.close(mImpl->mConnection, websocketpp::close::status::normal, "Client disconnecting");
        } catch (const std::exception& e) {
            DIA_LOG_ERROR("WebSocket disconnect error: %s", e.what());
        }
    }
    
    // Stop worker thread
    mImpl->mIsRunning = false;
    if (mImpl->mWorkerThread) {
        mImpl->mWorkerThread->Join();
        delete mImpl->mWorkerThread;
        mImpl->mWorkerThread = nullptr;
    }
    
    mImpl->mState = ConnectionState::kDisconnected;
    mImpl->mReconnectOnDisconnect = wasAutoReconnect;  // Restore setting
}
```

**Update():**
```cpp
void Client::Update() {
    if (!mImpl->mIsRunning) return;
    
    // Process incoming messages (worker thread → main thread)
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mIncomingMutex);
    
    for (const auto& queuedMsg : mImpl->mIncomingQueue) {
        if (mImpl->mOnMessage) {
            mImpl->mOnMessage(queuedMsg.message);
        }
    }
    
    mImpl->mIncomingQueue.Clear();
}
```

### Connection Handling

**OnOpen (worker thread):**
```cpp
void Client::Impl::OnOpen(websocketpp::connection_hdl hdl) {
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mStateMutex);
    mState = ConnectionState::kConnected;
    mReconnectAttemptCount = 0;  // Reset retry count on successful connection
    
    DIA_LOG("WebSocket connected to %s", mUrl);
    
    // Queue connection callback (connected = true)
    if (mOnConnection) {
        // Queue for main thread during next Update()
        // (omitted for brevity - similar pattern to message queue)
    }
}
```

**OnClose (worker thread):**
```cpp
void Client::Impl::OnClose(websocketpp::connection_hdl hdl) {
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mStateMutex);
    mState = ConnectionState::kDisconnected;
    
    DIA_LOG("WebSocket disconnected from %s", mUrl);
    
    // Queue connection callback (connected = false)
    if (mOnConnection) {
        // Queue callback...
    }
    
    // Attempt auto-reconnect if enabled
    if (mReconnectOnDisconnect && mIsRunning) {
        AttemptReconnect();
    }
}
```

**OnFail (worker thread):**
```cpp
void Client::Impl::OnFail(websocketpp::connection_hdl hdl) {
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mStateMutex);
    mState = ConnectionState::kError;
    
    DIA_LOG_ERROR("WebSocket connection failed to %s", mUrl);
    
    // Queue error callback
    if (mOnError) {
        // Queue callback...
    }
    
    // Attempt auto-reconnect if enabled
    if (mReconnectOnDisconnect && mIsRunning) {
        AttemptReconnect();
    }
}
```

**OnMessage (worker thread):**
```cpp
void Client::Impl::OnMessage(websocketpp::connection_hdl hdl, wspp_client::message_ptr msg) {
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
    queuedMsg.message.type = (msg->get_opcode() == websocketpp::frame::opcode::text)
                              ? MessageType::kText
                              : MessageType::kBinary;
    queuedMsg.message.length = msg->get_payload().size();
    
    // Copy message data
    queuedMsg.dataBuffer.Resize(msg->get_payload().size());
    memcpy(queuedMsg.dataBuffer.GetData(), msg->get_payload().data(), msg->get_payload().size());
    queuedMsg.message.data = queuedMsg.dataBuffer.GetData();
    
    mIncomingQueue.Add(queuedMsg);
}
```

### Auto-Reconnect

**AttemptReconnect:**
```cpp
void Client::Impl::AttemptReconnect() {
    if (mReconnectAttemptCount >= mReconnectMaxAttempts) {
        DIA_LOG_ERROR("WebSocket max reconnect attempts (%d) reached - giving up", mReconnectMaxAttempts);
        
        // Queue error callback
        if (mOnError) {
            // Queue callback...
        }
        return;
    }
    
    mReconnectAttemptCount++;
    
    DIA_LOG("WebSocket reconnecting (attempt %d/%d) after %.1fs delay...", 
            mReconnectAttemptCount, mReconnectMaxAttempts, mReconnectDelay);
    
    // Sleep on worker thread
    Dia::Core::ThisThread::SleepMs(static_cast<int>(mReconnectDelay * 1000));
    
    // Check if still running (might have been stopped during sleep)
    if (!mIsRunning) {
        DIA_LOG("WebSocket reconnect aborted - client stopped");
        return;
    }
    
    // Attempt reconnection
    {
        Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mStateMutex);
        mState = ConnectionState::kConnecting;
    }
    
    try {
        websocketpp::lib::error_code ec;
        mConnection = mClient.get_connection(mUrl, ec);
        
        if (ec) {
            DIA_LOG_ERROR("WebSocket reconnect failed: %s", ec.message().c_str());
            mState = ConnectionState::kError;
            return;  // OnFail will be called, which will retry again
        }
        
        mClient.connect(mConnection);
        
    } catch (const std::exception& e) {
        DIA_LOG_ERROR("WebSocket reconnect exception: %s", e.what());
        mState = ConnectionState::kError;
    }
}
```

## Implementation Files

- `Dia/DiaWebSocket/Client.h` - Public interface
- `Dia/DiaWebSocket/Client.cpp` - Implementation
- `Dia/DiaWebSocket/Internal/WebSocketppWrapper.h/cpp` - Shared with Server

## Binding Decisions Compliance

All binding decisions from parent specs must be honored:

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | ✅ **Compliant** - Connection state uses enums, not string IDs; no entity IDs in this feature |
| Platform | PD-004 | No STL in public APIs | ✅ **Compliant** - Public API uses `const char*` for strings, `std::function` for callbacks (allowed per AD-002) |
| Platform | PD-006 | Visual Studio project files are source of truth | ✅ **Compliant** - Will be built as part of DiaWebSocket.vcxproj |
| Dia | AD-001 | Module system with YAML frontmatter | ✅ **Compliant** - DiaWebSocket has `dia.dia.diawebsocket.architecture.module.md` |
| Dia | AD-002 | No STL containers in public APIs | ✅ **Compliant** - No containers exposed; uses `const char*` for strings, callbacks only use std::function |
| Dia | AD-003 | Namespace convention: `Dia::<Module>::` | ✅ **Compliant** - All classes in `Dia::WebSocket::` namespace |
| DiaWebSocket | DWS-001 | Wrap websocketpp, don't expose in public API | ✅ **Compliant** - Uses Pimpl pattern (`struct Impl`); websocketpp hidden in .cpp |
| DiaWebSocket | DWS-002 | Client runs on separate thread | ✅ **Compliant** - Uses `Dia::Core::Thread* mWorkerThread` for I/O |
| DiaWebSocket | DWS-003 | Update() pattern for callbacks | ✅ **Compliant** - `Update()` processes queue, fires callbacks on main thread |
| DiaWebSocket | DWS-004 | Support text and binary message types | ✅ **Compliant** - `MessageType` enum with kText/kBinary; `Send()` supports both |
| DiaWebSocket | DWS-005 | Message queue between threads | ✅ **Compliant** - `mIncomingQueue` (worker→main), `mOutgoingQueue` (main→worker) |
| DiaWebSocket | DWS-007 | websocketpp standalone mode (no Boost) | ✅ **Compliant** - Uses `websocketpp::config::asio_client` standalone config |

**All binding decisions: COMPLIANT ✅ No conflicts detected.**

## Open Questions

**Resolved:**
- ❌ Connect() while already connected? → **Ignore (no-op) but log warning**
- ❌ Message queue overflow? → **Same as Server: log error, fire error callback, drop message**
- ❌ Connect() blocking? → **Yes, blocks until connected or timeout**
- ❌ Auto-reconnect max attempts? → **5 attempts then log and give up**
- ❌ Thread shutdown during reconnect? → **Abort immediately (simpler)**

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Threading | How to handle race condition if Disconnect() called during auto-reconnect attempt? | Set `mIsRunning = false` immediately; worker thread checks on wake from sleep | ✅ Set `mIsRunning = false` immediately; worker thread checks on wake from sleep and aborts reconnect |
| 2 | Memory | Who owns Message.data pointer after callback fires? | Callback must not store pointer; data invalidated after callback returns | ✅ Callback must not store pointer; data invalidated after callback returns |
| 3 | Reconnection | Should reconnect attempts use exponential backoff? | No - fixed delay simpler for Phase 5; can add later if needed | ✅ No - fixed delay (2s default) is simpler; exponential backoff can be added in Phase 7+ if needed |
| 4 | Configuration | Can SetReconnectDelay/SetReconnectMaxAttempts be called after Connect()? | No - must be called before Connect(); assert if called after | ✅ No - must be called before Connect(); assert or log warning if called after |
| 5 | Thread Safety | Is GetState() thread-safe? Called from any thread? | Must lock `mStateMutex`; or cache state on main thread | ✅ Must lock `mStateMutex` - simple and correct |
| 6 | Callbacks | Are callbacks allowed to call Client methods (e.g., Send() during OnMessage)? | Yes - safe because callbacks fire on main thread; queues message for worker | ✅ Yes - safe because callbacks fire on main thread; Send() queues message for worker thread |
| 7 | Connection Timeout | What if Connect() times out but connection succeeds afterward? | Timeout transitions to kError; late success ignored (connection closed) | ✅ Timeout returns false and sets kError; if late OnOpen arrives, log warning and close connection |
| 8 | Auto-Reconnect | Should reconnect attempts be visible to user via callbacks? | Fire connection callback on each attempt (connected=false → kConnecting → result) | ✅ Fire connection callback when reconnect starts (kConnecting) and when it succeeds/fails |

## Status

`Done` - Implemented and tested
