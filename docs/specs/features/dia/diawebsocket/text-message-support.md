# Feature Spec: Text Message Support

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaWebSocket | @docs/specs/systems/dia/diawebsocket.md |
| Feature | **Text Message Support** | (this document) |

## Problem Statement

Provides UTF-8 text message support (MessageType::kText) for JSON, XML, and plain text communication over WebSocket, enabling human-readable protocols for debugging and editor communication.

## Acceptance Criteria

- [x] MessageType::kText enum value defined
- [x] Server can send text messages via SendText(connectionId, text) and BroadcastText(text)
- [x] Client can send text messages via SendText(text)
- [x] Text messages sent with websocketpp text opcode
- [x] Incoming text messages queued with type = MessageType::kText
- [x] Message::AsText() helper returns const char* for text messages
- [x] Text messages automatically null-terminated when queued (for safety)
- [x] Max message size limit applied (default 1MB, configurable)
- [x] Text convenience methods wrap generic Send() method
- [x] No UTF-8 validation (trust user input; add in Phase 7+ if needed)

## Design

### MessageType Enum

```cpp
namespace Dia::WebSocket {
    enum class MessageType {
        kText,    // UTF-8 text (JSON, XML, plain text)
        kBinary   // Raw bytes (serialized structs, compressed data)
    };
}
```

### Message Helper

```cpp
struct Message {
    MessageType type;
    const void* data;
    size_t length;
    int connectionId;  // Server only
    
    // Helper: Get data as string (for text messages)
    const char* AsText() const {
        DIA_ASSERT(type == MessageType::kText, "Message is not text");
        return static_cast<const char*>(data);
    }
};
```

**Usage:**
```cpp
server->SetMessageCallback([](int connId, const Message& msg) {
    if (msg.type == MessageType::kText) {
        const char* text = msg.AsText();
        // Process JSON, XML, etc.
    }
});
```

### Server API

**Convenience Methods:**
```cpp
class Server {
public:
    // Text-specific convenience methods
    void BroadcastText(const char* text);
    void SendText(int connectionId, const char* text);
    
    // Generic method (still available)
    void Broadcast(const void* data, size_t length, MessageType type = MessageType::kText);
    void Send(int connectionId, const void* data, size_t length, MessageType type = MessageType::kText);
};
```

**Implementation:**
```cpp
void Server::BroadcastText(const char* text) {
    Broadcast(text, strlen(text), MessageType::kText);
}

void Server::SendText(int connectionId, const char* text) {
    Send(connectionId, text, strlen(text), MessageType::kText);
}

void Server::Broadcast(const void* data, size_t length, MessageType type) {
    // Check max message size
    if (length > mImpl->mMaxMessageSize) {
        DIA_LOG_ERROR("WebSocket message too large: %zu bytes (max %zu)", 
                      length, mImpl->mMaxMessageSize);
        return;
    }
    
    OutgoingMessage msg;
    msg.connectionId = -1;  // Broadcast
    msg.type = type;
    
    // Copy data + null terminator for text
    if (type == MessageType::kText) {
        msg.dataBuffer.Resize(length + 1);
        memcpy(msg.dataBuffer.GetData(), data, length);
        msg.dataBuffer[length] = '\0';  // Null-terminate
    } else {
        msg.dataBuffer.Resize(length);
        memcpy(msg.dataBuffer.GetData(), data, length);
    }
    
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mOutgoingMutex);
    
    if (mImpl->mOutgoingQueue.IsFull()) {
        DIA_LOG_ERROR("WebSocket outgoing queue full - dropping message");
        return;
    }
    
    mImpl->mOutgoingQueue.Add(msg);
}
```

**Worker Thread Sends:**
```cpp
void Server::Impl::WorkerThreadMain() {
    while (mIsRunning) {
        mServer.poll();
        
        // Process outgoing messages
        {
            Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mOutgoingMutex);
            
            for (const auto& msg : mOutgoingQueue) {
                websocketpp::frame::opcode::value opcode = 
                    (msg.type == MessageType::kText) 
                    ? websocketpp::frame::opcode::text 
                    : websocketpp::frame::opcode::binary;
                
                if (msg.connectionId == -1) {
                    // Broadcast to all
                    for (const auto& [connId, hdl] : mConnections) {
                        try {
                            mServer.send(hdl, msg.dataBuffer.GetData(), 
                                        msg.dataBuffer.Size(), opcode);
                        } catch (const std::exception& e) {
                            DIA_LOG_ERROR("WebSocket send failed: %s", e.what());
                        }
                    }
                } else {
                    // Send to specific connection
                    auto it = mConnections.find(msg.connectionId);
                    if (it != mConnections.end()) {
                        try {
                            mServer.send(it->second, msg.dataBuffer.GetData(),
                                        msg.dataBuffer.Size(), opcode);
                        } catch (const std::exception& e) {
                            DIA_LOG_ERROR("WebSocket send failed: %s", e.what());
                        }
                    }
                }
            }
            
            mOutgoingQueue.Clear();
        }
        
        Dia::Core::ThisThread::SleepMs(1);
    }
}
```

### Client API

**Convenience Methods:**
```cpp
class Client {
public:
    // Text-specific convenience method
    void SendText(const char* text);
    
    // Generic method (still available)
    void Send(const void* data, size_t length, MessageType type = MessageType::kText);
};
```

**Implementation:**
```cpp
void Client::SendText(const char* text) {
    Send(text, strlen(text), MessageType::kText);
}

void Client::Send(const void* data, size_t length, MessageType type) {
    // Check max message size
    if (length > mImpl->mMaxMessageSize) {
        DIA_LOG_ERROR("WebSocket message too large: %zu bytes (max %zu)", 
                      length, mImpl->mMaxMessageSize);
        return;
    }
    
    OutgoingMessage msg;
    msg.type = type;
    
    // Copy data + null terminator for text
    if (type == MessageType::kText) {
        msg.dataBuffer.Resize(length + 1);
        memcpy(msg.dataBuffer.GetData(), data, length);
        msg.dataBuffer[length] = '\0';  // Null-terminate
    } else {
        msg.dataBuffer.Resize(length);
        memcpy(msg.dataBuffer.GetData(), data, length);
    }
    
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mOutgoingMutex);
    
    if (mImpl->mOutgoingQueue.IsFull()) {
        DIA_LOG_ERROR("WebSocket outgoing queue full - dropping message");
        return;
    }
    
    mImpl->mOutgoingQueue.Add(msg);
}
```

### Receiving Text Messages

**OnMessage Handler (Worker Thread):**
```cpp
void Impl::OnMessage(connection_hdl hdl, message_ptr msg) {
    // ... (validate size, find connection ID)
    
    // Determine message type from opcode
    MessageType msgType = (msg->get_opcode() == websocketpp::frame::opcode::text)
                          ? MessageType::kText
                          : MessageType::kBinary;
    
    // Queue message for main thread
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mIncomingMutex);
    
    if (mIncomingQueue.IsFull()) {
        DIA_LOG_ERROR("WebSocket incoming queue full - dropping message");
        return;
    }
    
    QueuedMessage queuedMsg;
    queuedMsg.connectionId = connId;
    queuedMsg.message.type = msgType;
    queuedMsg.message.length = msg->get_payload().size();
    
    // Copy message data + null terminator for text
    if (msgType == MessageType::kText) {
        queuedMsg.dataBuffer.Resize(msg->get_payload().size() + 1);
        memcpy(queuedMsg.dataBuffer.GetData(), 
               msg->get_payload().data(), 
               msg->get_payload().size());
        queuedMsg.dataBuffer[msg->get_payload().size()] = '\0';  // Null-terminate
    } else {
        queuedMsg.dataBuffer.Resize(msg->get_payload().size());
        memcpy(queuedMsg.dataBuffer.GetData(), 
               msg->get_payload().data(), 
               msg->get_payload().size());
    }
    
    queuedMsg.message.data = queuedMsg.dataBuffer.GetData();
    mIncomingQueue.Add(queuedMsg);
}
```

**Main Thread Callback:**
```cpp
void Server::Update() {
    if (!mImpl->mIsRunning) return;
    
    Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mIncomingMutex);
    
    for (const auto& queuedMsg : mImpl->mIncomingQueue) {
        if (mImpl->mOnMessage) {
            // User callback fires here
            mImpl->mOnMessage(queuedMsg.connectionId, queuedMsg.message);
            
            // After callback returns, message.data pointer becomes invalid
        }
    }
    
    mImpl->mIncomingQueue.Clear();
}
```

### Embedded Nulls Limitation

**Behavior:**
```cpp
// Sending text with embedded null
const char* text = "Hello\0World";  // Contains embedded null
server->SendText(text);  // Will only send "Hello" (strlen stops at first null)

// Workaround: Use generic Send() with explicit length
server->Send(text, 11, MessageType::kText);  // Sends full 11 bytes

// Receiving text with embedded nulls
client->SetMessageCallback([](const Message& msg) {
    if (msg.type == MessageType::kText) {
        const char* text = msg.AsText();  // Returns "Hello" (stops at first null)
        
        // Use msg.length to get actual size
        for (size_t i = 0; i < msg.length; ++i) {
            char c = static_cast<const char*>(msg.data)[i];  // Access full data
        }
    }
});
```

**Documentation:**
- `SendText()` uses `strlen()`, which stops at first null character
- `AsText()` returns C string, which stops at first null character
- Use generic `Send(data, length, kText)` if embedded nulls needed
- Use `msg.length` to access full data including embedded nulls

### Max Message Size

**Configuration:**
```cpp
class Server {
public:
    void SetMaxMessageSize(size_t bytes);  // Default 1MB
};

class Client {
public:
    void SetMaxMessageSize(size_t bytes);  // Default 1MB
};
```

**Enforcement:**
```cpp
void Server::Broadcast(const void* data, size_t length, MessageType type) {
    if (length > mImpl->mMaxMessageSize) {
        DIA_LOG_ERROR("WebSocket message too large: %zu bytes (max %zu)", 
                      length, mImpl->mMaxMessageSize);
        return;  // Reject message
    }
    
    // ... (proceed to queue)
}

void Impl::OnMessage(connection_hdl hdl, message_ptr msg) {
    if (msg->get_payload().size() > mMaxMessageSize) {
        DIA_LOG_ERROR("WebSocket message too large: %zu bytes (max %zu)", 
                      msg->get_payload().size(), mMaxMessageSize);
        
        if (mOnError) {
            // Fire error callback
        }
        
        return;  // Drop message
    }
    
    // ... (proceed to queue)
}
```

## Implementation Files

- `Dia/DiaWebSocket/Server.h` - SendText, BroadcastText declarations
- `Dia/DiaWebSocket/Server.cpp` - Text convenience methods implementation
- `Dia/DiaWebSocket/Client.h` - SendText declaration
- `Dia/DiaWebSocket/Client.cpp` - Text convenience method implementation
- `Dia/DiaWebSocket/Message.h` - Message struct with AsText() helper

## Binding Decisions Compliance

All binding decisions from parent specs must be honored:

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | ✅ **Compliant** - No entity IDs in this feature |
| Platform | PD-004 | No STL in public APIs | ✅ **Compliant** - Uses `const char*` for text strings |
| Platform | PD-006 | Visual Studio project files are source of truth | ✅ **Compliant** - Built as part of DiaWebSocket.vcxproj |
| Dia | AD-001 | Module system with YAML frontmatter | ✅ **Compliant** - DiaWebSocket has module documentation |
| Dia | AD-002 | No STL containers in public APIs | ✅ **Compliant** - Uses `const char*`, no std::string |
| Dia | AD-003 | Namespace convention: `Dia::<Module>::` | ✅ **Compliant** - All code in `Dia::WebSocket::` namespace |
| DiaWebSocket | DWS-001 | Wrap websocketpp, don't expose in public API | ✅ **Compliant** - websocketpp opcode handling is internal |
| DiaWebSocket | DWS-003 | Update() pattern for callbacks | ✅ **Compliant** - Text messages delivered via Update() callback |
| DiaWebSocket | DWS-004 | Support text and binary message types | ✅ **Compliant** - This feature implements text message type |
| DiaWebSocket | DWS-008 | Default max message size: 1MB | ✅ **Compliant** - 1MB default, configurable via SetMaxMessageSize() |

**All binding decisions: COMPLIANT ✅ No conflicts detected.**

## Open Questions

**Resolved:**
- ❌ Should we validate UTF-8 encoding? → **No validation initially; trust user; add in Phase 7+ if needed**
- ❌ Should text messages be automatically null-terminated? → **Yes, add null terminator when copying to dataBuffer**
- ❌ How to handle embedded nulls in text? → **Use msg.length for actual size; AsText() stops at first null (standard C behavior); document limitation**

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | UTF-8 Validation | Should we validate UTF-8 encoding for text messages? | No - adds overhead; trust user input; websocketpp handles protocol correctly | ✅ No validation - simpler and faster; add in Phase 7+ if corruption issues arise |
| 2 | Null Termination | Does null termination break binary data accidentally sent as text? | Yes - but that's user error; text should be text, binary should be binary | ✅ User error - should use MessageType::kBinary for binary data; null terminator only affects text |
| 3 | Memory Overhead | What is memory cost of null terminator? | 1 byte per message (negligible); simplifies C string usage | ✅ 1 byte per message - negligible cost for safety and convenience |
| 4 | Embedded Nulls | Should SendText() accept length parameter to support embedded nulls? | No - use generic Send(data, length, kText) for that case; keep convenience methods simple | ✅ No - convenience methods use strlen(); generic Send() available for advanced cases |
| 5 | Message Size Limit | Should text and binary have different size limits? | No - single limit simpler; both use same queue and buffer infrastructure | ✅ Single limit (1MB default) - simpler configuration; can be made type-specific in Phase 7+ if needed |
| 6 | String Encoding | Should we support other encodings (UTF-16, Latin-1)? | No - UTF-8 only for Phase 5; WebSocket standard uses UTF-8 for text frames | ✅ UTF-8 only - WebSocket spec mandates UTF-8 for text frames; other encodings can use binary type |
| 7 | Max Size Config | Should SetMaxMessageSize() be per-type (text vs binary) or global? | Global - simpler; both types share queue infrastructure | ✅ Global limit - simpler API; per-type limits can be added in Phase 7+ if needed |

## Status

`Done` - Implemented and tested
