# Feature Spec: message-bus

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaapplication.md | **message-bus** |

**Status:** `Done`

---

## Problem Statement

Modules need to communicate without tight coupling (e.g., InputModule sends "key pressed" event, GameplayModule receives it without InputModule knowing about GameplayModule). Direct module-to-module calls create dependencies and coupling. A publish-subscribe message bus decouples senders from receivers.

---

## Solution Overview

The **MessageBus** provides thread-safe, type-erased messaging:

- Modules subscribe to message types via Subscribe(messageType, handler)
- Modules send immediate (synchronous) messages via SendImmediate()
- Modules post queued (asynchronous) messages via PostMessage()
- ProcessingUnit calls ProcessQueue() each frame to dispatch queued messages
- Messages contain type ID, sender ID, timestamp, and payload (void* + size)

### Key Design Points

1. **Type Erasure** - Messages use void* + size, avoiding template bloat
2. **Thread Safety** - PostMessage() is thread-safe (uses mutex), SendImmediate() is not (single-threaded)
3. **FIFO Queue** - Queued messages dispatched in order sent
4. **Multiple Subscribers** - Many modules can subscribe to same message type
5. **Timestamp Tracking** - Messages include TimeAbsolute for debugging/profiling

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | Module can subscribe to message type with handler callback | Unit test: Subscribe(), verify handler stored |
| AC2 | SendImmediate() calls all subscribed handlers immediately | Unit test: Subscribe 2 handlers, send, verify both called |
| AC3 | PostMessage() adds message to queue (does not dispatch immediately) | Unit test: Post message, verify handler NOT called yet |
| AC4 | ProcessQueue() dispatches all queued messages in FIFO order | Unit test: Post 3 messages, ProcessQueue(), verify order |
| AC5 | Message includes type, senderId, timestamp, data, and size | Unit test: Verify Message struct fields populated |
| AC6 | Template SendImmediate<T>() provides type-safe API | Unit test: SendImmediate<int>(42), verify received |
| AC7 | Template PostMessage<T>() provides type-safe API | Unit test: PostMessage<Vec3>(pos), ProcessQueue(), verify received |
| AC8 | Message.GetData<T>() validates size before casting | Unit test: Send int, GetData<float>() returns nullptr (size mismatch) |
| AC9 | Unsubscribe() removes handler from subscription list | Unit test: Subscribe, Unsubscribe, send, verify handler not called |
| AC10 | PostMessage() is thread-safe (can call from any thread) | Integration test: Post from separate thread, ProcessQueue() on main, verify dispatched |
| AC11 | GetQueuedMessageCount() returns number of pending messages | Unit test: Post 3, verify returns 3 |
| AC12 | GetSubscriberCount() returns number of handlers for message type | Unit test: Subscribe 2 handlers, verify returns 2 |

---

## Public API

```cpp
namespace Dia::Application {

// Message structure
struct Message {
    Dia::Core::StringCRC type;          // Message type ID
    Dia::Core::StringCRC senderId;      // Module that sent it
    Dia::Core::TimeAbsolute timestamp;  // When sent
    const void* data;                   // Payload (type-erased)
    size_t dataSize;                    // Size of payload in bytes
    
    // Type-safe data access
    template<typename T>
    const T* GetData() const {
        if (dataSize != sizeof(T)) {
            return nullptr;  // Size mismatch
        }
        return static_cast<const T*>(data);
    }
};

// Handler callback
using MessageHandler = std::function<void(const Message&)>;

// Message bus
class MessageBus {
public:
    MessageBus();
    ~MessageBus();
    
    // Subscribe to message type (thread-safe)
    void Subscribe(const Dia::Core::StringCRC& messageType,
                  const Dia::Core::StringCRC& subscriberId,
                  MessageHandler handler);
    
    // Unsubscribe from message type
    void Unsubscribe(const Dia::Core::StringCRC& messageType,
                    const Dia::Core::StringCRC& subscriberId);
    
    // Send message immediately (synchronous)
    void SendImmediate(const Dia::Core::StringCRC& messageType,
                      const Dia::Core::StringCRC& senderId,
                      const void* data,
                      size_t dataSize);
    
    // Post message to queue (asynchronous, thread-safe)
    void PostMessage(const Dia::Core::StringCRC& messageType,
                    const Dia::Core::StringCRC& senderId,
                    const void* data,
                    size_t dataSize);
    
    // Process all queued messages (called by ProcessingUnit::DoUpdate)
    void ProcessQueue();
    
    // Type-safe templates
    template<typename T>
    void SendImmediate(const Dia::Core::StringCRC& messageType,
                      const Dia::Core::StringCRC& senderId,
                      const T& data) {
        SendImmediate(messageType, senderId, &data, sizeof(T));
    }
    
    template<typename T>
    void PostMessage(const Dia::Core::StringCRC& messageType,
                    const Dia::Core::StringCRC& senderId,
                    const T& data) {
        PostMessage(messageType, senderId, &data, sizeof(T));
    }
    
    // Statistics
    size_t GetQueuedMessageCount() const;
    size_t GetSubscriberCount(const Dia::Core::StringCRC& messageType) const;
};

}
```

---

## Implementation Notes

### Internal Message Storage

```cpp
struct MessageData {
    StringCRC type;
    StringCRC senderId;
    TimeAbsolute timestamp;
    std::vector<char> payload;  // Owns copied data
    
    Message GetMessage() const {
        Message msg;
        msg.type = type;
        msg.senderId = senderId;
        msg.timestamp = timestamp;
        msg.data = payload.data();
        msg.dataSize = payload.size();
        return msg;
    }
};
```

### Thread Safety

- **Subscribe/Unsubscribe**: Protected by mMutex
- **PostMessage**: Protected by mMutex (adds to queue)
- **ProcessQueue**: Protected by mMutex (drains queue)
- **SendImmediate**: NOT thread-safe (assumes single-threaded dispatch)

### Message Flow

**Immediate (Synchronous):**
```
Module → SendImmediate() → DispatchMessage() → Handler1(), Handler2(), ... → returns
```

**Queued (Asynchronous):**
```
Module → PostMessage() → [message added to queue] → returns
... later ...
ProcessingUnit::DoUpdate() → MessageBus::ProcessQueue() → Handler1(), Handler2(), ...
```

---

## Dependencies

### Required Modules
- **DiaCore/CRC** - StringCRC for message types and sender IDs
- **DiaCore/Time** - TimeAbsolute for timestamps
- **Standard Library** - std::function, std::mutex, std::deque, std::vector

### Dependent Features
- **processing-unit** - Calls ProcessQueue() each update

---

## Testing Strategy

### Unit Tests (Cluiche/Tests/GoogleTests/Application/TestMessageBus.cpp)

1. **Subscribe/Unsubscribe**
   - Subscribe handler, verify GetSubscriberCount() == 1
   - Unsubscribe, verify GetSubscriberCount() == 0

2. **SendImmediate**
   - Subscribe handler with flag
   - SendImmediate(), verify handler called and flag set

3. **PostMessage + ProcessQueue**
   - PostMessage(), verify GetQueuedMessageCount() == 1
   - ProcessQueue(), verify handler called, queue empty

4. **FIFO order**
   - Post messages A, B, C
   - ProcessQueue(), verify handlers called in order A→B→C

5. **Multiple subscribers**
   - Subscribe handler1 and handler2 to same type
   - SendImmediate(), verify both called

6. **Type-safe API**
   - SendImmediate<Vec3>(position)
   - Handler receives Message, GetData<Vec3>() returns correct value

7. **Size validation**
   - Send int (4 bytes)
   - GetData<double>() returns nullptr (size mismatch)

8. **Thread safety**
   - Spawn thread calling PostMessage()
   - Main thread calls ProcessQueue()
   - Verify no crashes, message dispatched

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | Use StringCRC for all IDs | ✅ **Compliant** - Message types and sender IDs are StringCRC |
| SD-007 | DiaApplicationFlow | Type-erased void* with size tracking | ✅ **Compliant** - Message uses void* + size, not templates |

---

## Files Affected

- `Dia/DiaApplicationFlow/MessageBus.h`
- `Dia/DiaApplicationFlow/MessageBus.cpp`
- `Cluiche/Tests/GoogleTests/Application/TestMessageBus.cpp`

---

## Examples

### Example 1: Subscribe and Receive

```cpp
class GameplayModule : public Module {
    void DoBuildDependancies(IBuildDependencyData* deps) override {
        auto& messageBus = GetAssociatedProcessingUnit()->GetMessageBus();
        
        // Subscribe to "key_pressed" messages
        messageBus.Subscribe(
            StringCRC("key_pressed"),
            GetUniqueId(),
            [this](const Message& msg) {
                OnKeyPressed(msg);
            }
        );
    }
    
    void OnKeyPressed(const Message& msg) {
        struct KeyData { int keyCode; };
        const KeyData* key = msg.GetData<KeyData>();
        if (key && key->keyCode == KEY_SPACE) {
            Jump();
        }
    }
};
```

### Example 2: Send Message

```cpp
class InputModule : public Module {
    void DoUpdate() override {
        if (KeyPressed(KEY_SPACE)) {
            auto& messageBus = GetAssociatedProcessingUnit()->GetMessageBus();
            
            struct KeyData { int keyCode; };
            KeyData data = { KEY_SPACE };
            
            // Send immediately (synchronous)
            messageBus.SendImmediate<KeyData>(
                StringCRC("key_pressed"),
                GetUniqueId(),
                data
            );
        }
    }
};
```

### Example 3: Post Queued Message

```cpp
void PhysicsModule::OnCollision(Entity a, Entity b) {
    auto& messageBus = GetAssociatedProcessingUnit()->GetMessageBus();
    
    struct CollisionData {
        EntityID entityA;
        EntityID entityB;
        Vec3 contactPoint;
    };
    
    CollisionData data = { a.id, b.id, contactPoint };
    
    // Post to queue (dispatched next frame)
    messageBus.PostMessage<CollisionData>(
        StringCRC("collision"),
        GetUniqueId(),
        data
    );
}
```

---

## Status

`Done` - Implemented and tested
