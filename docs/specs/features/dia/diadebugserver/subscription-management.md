# Feature Spec: Subscription Management

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaDebugServer | @docs/specs/systems/dia/diadebugserver.md |
| Feature | **Subscription Management** | (this document) |

## Problem Statement

Tracks which editors are subscribed to which data types (processing_unit_state, phase_transition, etc.) and only sends subscribed data to reduce bandwidth.

## Acceptance Criteria

- [x] Handle subscribe/unsubscribe JSON messages
- [x] Track subscriptions per connection
- [x] Support data types: processing_unit_state, phase_transition, module_state
- [x] Unsubscribe all on disconnect
- [x] Thread-safe subscription storage

## Design

### Subscription Structure

**Data Types:**
```cpp
struct Subscription {
    int connectionId;
    StringCRC dataType;  // e.g., "processing_unit_state"
    Json::Value filter;  // Optional filter (e.g., specific PU ID)
};
```

**SubscriptionManager:**
```cpp
class SubscriptionManager {
public:
    void Subscribe(int connectionId, const StringCRC& dataType, const Json::Value& filter);
    void Unsubscribe(int connectionId, const StringCRC& dataType);
    void UnsubscribeAll(int connectionId);
    
    DynamicArrayC<int, 16> GetSubscribers(const StringCRC& dataType) const;
    bool IsSubscribed(int connectionId, const StringCRC& dataType) const;
    
private:
    DynamicArrayC<Subscription, 64> mSubscriptions;
    Dia::Core::Mutex mMutex;
};
```

### Subscribe Message

**JSON:**
```json
{
  "type": "subscribe",
  "data_type": "processing_unit_state",
  "filter": {
    "pu_id": "MainProcessingUnit"
  }
}
```

**Handler:**
```cpp
void DebugServerModule::HandleSubscribe(int connId, const Json::Value& json) {
    std::string dataTypeStr = json["data_type"].asString();
    StringCRC dataType(dataTypeStr.c_str());
    Json::Value filter = json.get("filter", Json::Value::null);
    
    mSubscriptionManager->Subscribe(connId, dataType, filter);
    
    DIA_LOG("Connection %d subscribed to %s", connId, dataTypeStr.c_str());
    
    // Send initial data immediately
    SendDataUpdate(connId, dataType);
}
```

### NotifySubscribers

**DebugServerModule::NotifySubscribers:**
```cpp
void DebugServerModule::NotifySubscribers(const StringCRC& dataType, const Json::Value& payload) {
    auto subscribers = mSubscriptionManager->GetSubscribers(dataType);
    
    if (subscribers.Size() == 0) return;  // No one subscribed
    
    // Build message
    Json::Value json;
    json["type"] = "data_update";
    json["data_type"] = dataType.GetString();
    json["timestamp"] = GetTimestamp();
    json["payload"] = payload;
    
    std::string jsonStr = Json::FastWriter().write(json);
    
    // Send to each subscriber
    for (int connId : subscribers) {
        mServer->SendText(connId, jsonStr.c_str());
    }
}
```

## Implementation Files

- `Dia/DiaDebugServer/SubscriptionManager.h/cpp`
- `Dia/DiaDebugServer/DebugServerModule.cpp` - HandleSubscribe

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaDebugServer | DDS-002 | Subscription-based specialized data | ✅ **Compliant** |

**All binding decisions: COMPLIANT ✅**

## Open Questions

**Resolved:**
- **Decision 3:** No subscription persistence across reconnects - editor re-subscribes on each connection.

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Persistence | Save subscriptions on disconnect? | No - editor re-subscribes on reconnect | ✅ No persistence (Decision 3) |
| 2 | Validation | Reject unknown data types? | No - queue subscription; data may become available later | ✅ Accept all, queue if needed (Phase 5) |
| 3 | Wildcards | Support "all" data type? | Phase 7+ - explicit subscriptions only for Phase 5 | ✅ Explicit only for Phase 5 |

## Status

`Done`
