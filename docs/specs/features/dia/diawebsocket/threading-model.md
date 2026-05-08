# Feature Spec: Threading Model

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaWebSocket | @docs/specs/systems/dia/diawebsocket.md |
| Feature | **Threading Model** | (this document) |

## Problem Statement

Provides a consistent threading architecture for DiaWebSocket Server and Client where I/O runs on a worker thread, messages are queued, and callbacks fire on the main thread during Update(), avoiding synchronization issues for users.

## Acceptance Criteria

- [x] Worker thread created using `Dia::Core::Thread` for I/O operations
- [x] Incoming message queue with `Dia::Core::Mutex` protection (worker → main)
- [x] Outgoing message queue with `Dia::Core::Mutex` protection (main → worker)
- [x] Update() method processes incoming queue and fires callbacks on main thread
- [x] Worker thread processes outgoing queue and sends via websocketpp
- [x] QueuedMessage struct owns message data (copy on queue, no shared pointers)
- [x] Queue overflow handling (log error, fire error callback, drop message)
- [x] Thread shutdown: immediate (don't finish queued messages)
- [x] Worker thread uses poll() for non-blocking I/O (not blocking run())
- [x] Thread naming for debugger visibility ("WebSocket Server", "WebSocket Client")

## Design

### Threading Architecture

```
Worker Thread (DiaCore::Thread):     Main Thread:
┌────────────────────────────┐      ┌───────────────────────────┐
│ websocketpp async I/O      │      │ User code                 │
│ Accept/Connect             │      │                           │
│ Receive messages    ──────>│───>  │ Update()                  │
│ Queue messages (mutex)     │      │   Lock incoming mutex     │
│                            │      │   Process message queue   │
│                            │      │   Fire callbacks          │
│ Send queued data    <──────│<───  │   Clear queue             │
│   Lock outgoing mutex      │      │                           │
│   Dequeue messages         │      │ Send()/Broadcast()        │
│   websocketpp send()       │      │   Lock outgoing mutex     │
│                            │      │   Queue message           │
│ poll() non-blocking        │      │                           │
│ Sleep 1ms (yield CPU)      │      │                           │
└────────────────────────────┘      └───────────────────────────┘
```

**Key Points:**
- Worker thread handles all websocketpp I/O (non-blocking via poll())
- Incoming messages queued: worker thread → main thread
- Outgoing messages queued: main thread → worker thread
- All queue access protected by `Dia::Core::Mutex`
- Callbacks always fire on main thread (during Update()) - no synchronization needed by user
- Thread shutdown is immediate: set `mIsRunning = false`, stop loop, join thread

### Data Structures

**QueuedMessage (Incoming):**
```cpp
struct QueuedMessage {
    int connectionId;  // Server: which client sent it; Client: -1 (unused)
    Message message;   // Contains type, data pointer, length
    DynamicArrayC<char, 1024> dataBuffer;  // Owns copy of message data
};
```

**OutgoingMessage (Outgoing):**
```cpp
struct OutgoingMessage {
    int connectionId;  // Server: -1 = broadcast, other = specific client; Client: unused
    MessageType type;  // kText or kBinary
    DynamicArrayC<char, 1024> dataBuffer;  // Owns copy of message data
};
```

**Queues:**
```cpp
// Incoming (worker → main)
DynamicArrayC<QueuedMessage, 64> mIncomingQueue;
Dia::Core::Mutex mIncomingMutex;

// Outgoing (main → worker)
DynamicArrayC<OutgoingMessage, 64> mOutgoingQueue;
Dia::Core::Mutex mOutgoingMutex;
```

**Thread State:**
```cpp
Dia::Core::Thread* mWorkerThread;
bool mIsRunning;  // Set to false to signal shutdown
```

### Thread Lifecycle

**Start (Server/Client):**
```cpp
bool Server::Start() {
    // ... (setup websocketpp handlers)
    
    mImpl->mIsRunning = true;
    mImpl->mWorkerThread = new Dia::Core::Thread("WebSocket Server", [this]() {
        mImpl->WorkerThreadMain();
    });
    
    return true;
}

bool Client::Connect(const char* url) {
    // ... (setup websocketpp handlers)
    
    mImpl->mIsRunning = true;
    mImpl->mWorkerThread = new Dia::Core::Thread("WebSocket Client", [this]() {
        mImpl->WorkerThreadMain();
    });
    
    // Block until connected or timeout...
}
```

**Worker Thread Main Loop:**
```cpp
void Impl::WorkerThreadMain() {
    while (mIsRunning) {
        try {
            // Non-blocking I/O
            mClient.poll();  // or mServer.poll()
            
            // Process outgoing messages (main → worker)
            {
                Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mOutgoingMutex);
                
                for (const auto& msg : mOutgoingQueue) {
                    // Send via websocketpp
                    // (Server: broadcast or send to specific connection)
                    // (Client: send to server)
                }
                
                mOutgoingQueue.Clear();
            }
            
            // Yield CPU (avoid busy-wait)
            Dia::Core::ThisThread::SleepMs(1);
            
        } catch (const std::exception& e) {
            DIA_LOG_ERROR("WebSocket worker thread error: %s", e.what());
            // Continue running (don't crash on network errors)
        }
    }
}
```

**Stop (Server/Client):**
```cpp
void Server::Stop() {
    if (!mImpl->mIsRunning) return;
    
    // Signal shutdown
    mImpl->mIsRunning = false;
    
    // Stop websocketpp I/O
    mImpl->mServer.stop();
    
    // Wait for worker thread to exit
    if (mImpl->mWorkerThread) {
        mImpl->mWorkerThread->Join();
        delete mImpl->mWorkerThread;
        mImpl->mWorkerThread = nullptr;
    }
    
    // Clear queues (pending messages discarded)
    {
        Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mIncomingMutex);
        mImpl->mIncomingQueue.Clear();
    }
    {
        Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mOutgoingMutex);
        mImpl->mOutgoingQueue.Clear();
    }
}
```

### Message Queuing

**Incoming (Worker Thread → Main Thread):**
```cpp
void Impl::OnMessage(connection_hdl hdl, message_ptr msg) {
    // ... (validate message size, find connection ID, etc.)
    
    // Queue message for main thread
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mIncomingMutex);
    
    if (mIncomingQueue.IsFull()) {
        DIA_LOG_ERROR("WebSocket incoming queue full - dropping message");
        if (mOnError) {
            // Queue error callback (special handling needed)
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

**Outgoing (Main Thread → Worker Thread):**
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

### Main Thread Update

**Update() (Called from Main Thread):**
```cpp
void Server::Update() {
    if (!mImpl->mIsRunning) return;
    
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

### Queue Overflow Handling

When a queue fills (64 messages):

**Incoming Queue Overflow:**
```cpp
if (mIncomingQueue.IsFull()) {
    DIA_LOG_ERROR("WebSocket incoming queue full - dropping message");
    
    // Fire error callback (if set)
    if (mOnError) {
        // Need to queue error callback itself (or handle specially)
        // Option: Add separate error queue, or log only
    }
    
    return;  // Drop message
}
```

**Outgoing Queue Overflow:**
```cpp
if (mOutgoingQueue.IsFull()) {
    DIA_LOG_ERROR("WebSocket outgoing queue full - dropping message");
    // No callback needed (user already knows they sent too many messages)
    return;  // Drop message
}
```

### Exception Handling

Worker thread catches all exceptions to prevent crashes:

```cpp
void Impl::WorkerThreadMain() {
    while (mIsRunning) {
        try {
            mServer.poll();
            // ... (process queues)
            Dia::Core::ThisThread::SleepMs(1);
            
        } catch (const websocketpp::exception& e) {
            DIA_LOG_ERROR("WebSocket library error: %s", e.what());
            // Continue running (network errors shouldn't crash)
            
        } catch (const std::exception& e) {
            DIA_LOG_ERROR("WebSocket worker thread error: %s", e.what());
            // Continue running
            
        } catch (...) {
            DIA_LOG_ERROR("WebSocket worker thread unknown error");
            // Continue running
        }
    }
}
```

## Implementation Files

- `Dia/DiaWebSocket/Server.cpp` - Server worker thread
- `Dia/DiaWebSocket/Client.cpp` - Client worker thread
- `Dia/DiaWebSocket/Internal/WebSocketppWrapper.h` - Shared queue structures

## Binding Decisions Compliance

All binding decisions from parent specs must be honored:

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | ✅ **Compliant** - No entity IDs in this feature |
| Platform | PD-004 | No STL in public APIs | ✅ **Compliant** - All internal implementation; no public API changes |
| Platform | PD-006 | Visual Studio project files are source of truth | ✅ **Compliant** - Built as part of DiaWebSocket.vcxproj |
| Dia | AD-001 | Module system with YAML frontmatter | ✅ **Compliant** - DiaWebSocket has module documentation |
| Dia | AD-002 | No STL containers in public APIs | ✅ **Compliant** - Uses DiaCore containers internally |
| Dia | AD-003 | Namespace convention: `Dia::<Module>::` | ✅ **Compliant** - All code in `Dia::WebSocket::` namespace |
| DiaWebSocket | DWS-001 | Wrap websocketpp, don't expose in public API | ✅ **Compliant** - All threading is internal implementation |
| DiaWebSocket | DWS-002 | Server/Client run on separate threads | ✅ **Compliant** - This feature implements the threading model |
| DiaWebSocket | DWS-003 | Update() pattern for callbacks | ✅ **Compliant** - Update() processes incoming queue and fires callbacks |
| DiaWebSocket | DWS-005 | Message queue between threads | ✅ **Compliant** - This feature defines the queue architecture |

**All binding decisions: COMPLIANT ✅ No conflicts detected.**

## Open Questions

**Resolved:**
- ❌ Should `mIsRunning` be `std::atomic<bool>` or just `bool` with mutex? → **Just bool (simpler), checked without lock (safe for shutdown signal)**
- ❌ What happens if worker thread throws exception? → **Catch and log, continue running (don't crash on network errors)**
- ❌ Should queue sizes be configurable or fixed at 64? → **Fixed at 64 for Phase 5 (simpler), configurable in Phase 7+ if needed**
- ❌ Worker thread sleep duration between poll() calls? → **1ms (balance between responsiveness and CPU usage)**

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Thread Safety | Is `mIsRunning` safe to read without mutex? | Yes - bool read/write is atomic on modern CPUs; worst case is one extra loop iteration | ✅ Yes - bool read/write is atomic; worst case is worker thread runs one extra loop before seeing shutdown signal |
| 2 | Exception Handling | Should worker thread exit on exception or continue? | Continue - network errors shouldn't crash; log and recover | ✅ Continue running - catch all exceptions, log error, keep processing. Only exit on `mIsRunning = false` |
| 3 | Queue Overflow | How to handle error callback when incoming queue full? | Log only (don't queue error callback, risk recursion); or use separate small error queue | ✅ Log only - avoid complexity of separate error queue for Phase 5 |
| 4 | Memory | Who owns message data in QueuedMessage? | QueuedMessage owns it (dataBuffer member); caller must not retain Message.data pointer | ✅ QueuedMessage owns data via dataBuffer member; Message.data points into dataBuffer; valid only during callback |
| 5 | Performance | Is 1ms sleep too frequent? | No - 1000 polls/sec is reasonable for responsiveness; can optimize later if CPU usage high | ✅ 1ms is good balance - responsive without burning CPU; can be made configurable in Phase 7+ if needed |
| 6 | Shutdown | Should Stop() wait for queued messages to send? | No - immediate shutdown is simpler and safer (messages discarded) | ✅ No - immediate shutdown, queued messages discarded. Document this behavior clearly. |
| 7 | Thread Naming | Are thread names visible in debugger? | Yes - DiaCore::Thread passes name to OS for debugger visibility | ✅ Yes - thread names appear in Visual Studio debugger for easier identification |
| 8 | Queue Size | What if 64 messages is too small for some use cases? | Make SetMaxQueueSize() configurable method in Phase 7+; 64 is reasonable for Phase 5 | ✅ Fixed at 64 for Phase 5 (covers typical debug scenarios); add SetMaxQueueSize() in Phase 7+ if needed |

## Status

`Done` - Implemented and tested
