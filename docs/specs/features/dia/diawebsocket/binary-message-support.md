# Feature Spec: Binary Message Support

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaWebSocket | @docs/specs/systems/dia/diawebsocket.md |
| Feature | **Binary Message Support** | (this document) |

## Problem Statement

Provides raw binary message support (MessageType::kBinary) for performance-critical data like serialized structs, compressed payloads, and non-text protocols over WebSocket.

## Acceptance Criteria

- [x] MessageType::kBinary enum value defined
- [x] Server can send binary via SendBinary(connectionId, data, length) and BroadcastBinary(data, length)
- [x] Client can send binary via SendBinary(data, length)
- [x] Binary messages sent with websocketpp binary opcode
- [x] Incoming binary messages queued with type = MessageType::kBinary
- [x] No null terminator added to binary data (raw bytes preserved)
- [x] Max message size limit applied (default 1MB, configurable)
- [x] Binary convenience methods wrap generic Send() method
- [x] Support arbitrary byte sequences including 0x00

## Design

### MessageType Enum

```cpp
namespace Dia::WebSocket {
    enum class MessageType {
        kText,    // UTF-8 text (JSON, XML, plain text)
        kBinary   // Raw bytes (serialized structs, compressed data) - THIS FEATURE
    };
}
```

### Server API

**Convenience Methods:**
```cpp
class Server {
public:
    // Binary-specific convenience methods
    void BroadcastBinary(const void* data, size_t length);
    void SendBinary(int connectionId, const void* data, size_t length);
    
    // Generic method (still available)
    void Broadcast(const void* data, size_t length, MessageType type = MessageType::kText);
    void Send(int connectionId, const void* data, size_t length, MessageType type = MessageType::kText);
};
```

**Implementation:**
```cpp
void Server::BroadcastBinary(const void* data, size_t length) {
    Broadcast(data, length, MessageType::kBinary);
}

void Server::SendBinary(int connectionId, const void* data, size_t length) {
    Send(connectionId, data, length, MessageType::kBinary);
}

void Server::Send(int connectionId, const void* data, size_t length, MessageType type) {
    // Check max message size
    if (length > mImpl->mMaxMessageSize) {
        DIA_LOG_ERROR("WebSocket message too large: %zu bytes (max %zu)", 
                      length, mImpl->mMaxMessageSize);
        return;
    }
    
    OutgoingMessage msg;
    msg.connectionId = connectionId;
    msg.type = type;
    
    // Copy data (no null terminator for binary)
    if (type == MessageType::kText) {
        msg.dataBuffer.Resize(length + 1);
        memcpy(msg.dataBuffer.GetData(), data, length);
        msg.dataBuffer[length] = '\0';  // Null-terminate text only
    } else {
        msg.dataBuffer.Resize(length);  // Binary: exact size
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
                // Select opcode based on message type
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
    // Binary-specific convenience method
    void SendBinary(const void* data, size_t length);
    
    // Generic method (still available)
    void Send(const void* data, size_t length, MessageType type = MessageType::kText);
};
```

**Implementation:**
```cpp
void Client::SendBinary(const void* data, size_t length) {
    Send(data, length, MessageType::kBinary);
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
    
    // Copy data (no null terminator for binary)
    if (type == MessageType::kText) {
        msg.dataBuffer.Resize(length + 1);
        memcpy(msg.dataBuffer.GetData(), data, length);
        msg.dataBuffer[length] = '\0';  // Null-terminate text only
    } else {
        msg.dataBuffer.Resize(length);  // Binary: exact size
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

### Receiving Binary Messages

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
    
    // Copy message data (no null terminator for binary)
    if (msgType == MessageType::kText) {
        queuedMsg.dataBuffer.Resize(msg->get_payload().size() + 1);
        memcpy(queuedMsg.dataBuffer.GetData(), 
               msg->get_payload().data(), 
               msg->get_payload().size());
        queuedMsg.dataBuffer[msg->get_payload().size()] = '\0';  // Text only
    } else {
        queuedMsg.dataBuffer.Resize(msg->get_payload().size());  // Binary: exact size
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
server->SetMessageCallback([](int connId, const Message& msg) {
    if (msg.type == MessageType::kBinary) {
        const void* binaryData = msg.data;
        size_t length = msg.length;
        
        // Access raw bytes
        const uint8_t* bytes = static_cast<const uint8_t*>(binaryData);
        for (size_t i = 0; i < length; ++i) {
            uint8_t byte = bytes[i];  // Includes 0x00, 0xFF, etc.
        }
        
        // Or cast to struct
        struct MyData {
            int x;
            float y;
        };
        
        if (length == sizeof(MyData)) {
            const MyData* data = static_cast<const MyData*>(binaryData);
            // Use data->x, data->y
        }
    }
});
```

### Use Cases

**1. Serialized Structs:**
```cpp
struct GameState {
    float playerX;
    float playerY;
    int health;
    int score;
};

// Send
GameState state = { 10.5f, 20.3f, 100, 5000 };
server->SendBinary(connId, &state, sizeof(GameState));

// Receive
client->SetMessageCallback([](const Message& msg) {
    if (msg.type == MessageType::kBinary && msg.length == sizeof(GameState)) {
        const GameState* state = static_cast<const GameState*>(msg.data);
        // Use state->playerX, state->playerY, etc.
    }
});
```

**2. Compressed Data:**
```cpp
// After compressing JSON with zlib/gzip
const void* compressedData = /* ... */;
size_t compressedSize = /* ... */;

server->BroadcastBinary(compressedData, compressedSize);

// Receiver decompresses
client->SetMessageCallback([](const Message& msg) {
    if (msg.type == MessageType::kBinary) {
        // Decompress msg.data (msg.length bytes)
        // Parse as JSON
    }
});
```

**3. Image Data:**
```cpp
// Send raw RGBA pixel data
const uint8_t* pixels = /* 1920*1080*4 bytes */;
size_t imageSize = 1920 * 1080 * 4;

if (imageSize <= 1024*1024) {  // Under 1MB limit
    server->SendBinary(connId, pixels, imageSize);
}
```

**4. Custom Protocol:**
```cpp
// Binary protocol: [uint8_t type][uint32_t length][payload...]
struct PacketHeader {
    uint8_t type;
    uint32_t length;
};

// Send
uint8_t buffer[1024];
PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
header->type = 0x01;  // Includes 0x00 byte
header->length = 512;
memcpy(buffer + sizeof(PacketHeader), payloadData, 512);

server->SendBinary(connId, buffer, sizeof(PacketHeader) + 512);

// Receive
client->SetMessageCallback([](const Message& msg) {
    if (msg.type == MessageType::kBinary && msg.length >= sizeof(PacketHeader)) {
        const PacketHeader* header = static_cast<const PacketHeader*>(msg.data);
        const void* payload = static_cast<const uint8_t*>(msg.data) + sizeof(PacketHeader);
        // Handle packet based on header->type
    }
});
```

## Implementation Files

- `Dia/DiaWebSocket/Server.h` - SendBinary, BroadcastBinary declarations
- `Dia/DiaWebSocket/Server.cpp` - Binary convenience methods implementation
- `Dia/DiaWebSocket/Client.h` - SendBinary declaration
- `Dia/DiaWebSocket/Client.cpp` - Binary convenience method implementation

## Binding Decisions Compliance

All binding decisions from parent specs must be honored:

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | ✅ **Compliant** - No entity IDs in this feature |
| Platform | PD-004 | No STL in public APIs | ✅ **Compliant** - Uses `const void*` and `size_t` |
| Platform | PD-006 | Visual Studio project files are source of truth | ✅ **Compliant** - Built as part of DiaWebSocket.vcxproj |
| Dia | AD-001 | Module system with YAML frontmatter | ✅ **Compliant** - DiaWebSocket has module documentation |
| Dia | AD-002 | No STL containers in public APIs | ✅ **Compliant** - No containers exposed |
| Dia | AD-003 | Namespace convention: `Dia::<Module>::` | ✅ **Compliant** - All code in `Dia::WebSocket::` namespace |
| DiaWebSocket | DWS-001 | Wrap websocketpp, don't expose in public API | ✅ **Compliant** - websocketpp opcode handling is internal |
| DiaWebSocket | DWS-003 | Update() pattern for callbacks | ✅ **Compliant** - Binary messages delivered via Update() callback |
| DiaWebSocket | DWS-004 | Support text and binary message types | ✅ **Compliant** - This feature implements binary message type |
| DiaWebSocket | DWS-008 | Default max message size: 1MB | ✅ **Compliant** - 1MB default, configurable via SetMaxMessageSize() |

**All binding decisions: COMPLIANT ✅ No conflicts detected.**

## Open Questions

**Resolved:**
- ✅ **None** - Binary is simpler than text (raw bytes, no encoding/termination concerns)

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Endianness | Should we handle endianness conversion for structs? | No - user responsibility; binary protocol should define byte order | ✅ No - user handles endianness; document that binary data is sent as-is |
| 2 | Alignment | Are struct alignment issues a concern? | Yes - document that packed structs recommended or use explicit serialization | ✅ Document: packed structs (#pragma pack) or explicit serialization recommended for cross-platform |
| 3 | Max Size | Is 1MB reasonable for binary data (images, etc.)? | Yes for Phase 5; large assets should use separate transfer mechanism | ✅ 1MB is reasonable for debug data; large assets (textures, meshes) use HTTP/file transfer |
| 4 | Compression | Should we auto-compress large binary messages? | No - user can compress before sending if needed | ✅ No auto-compression - adds latency and complexity; user can compress before Send() |
| 5 | Validation | Should we validate binary message structure? | No - raw bytes, no structure assumed | ✅ No validation - binary is opaque; protocol-specific validation is user responsibility |

## Status

`Done` - Implemented and tested
