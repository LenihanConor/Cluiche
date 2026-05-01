# Feature Spec: Event Broadcasting

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaDebugServer | @docs/specs/systems/dia/diadebugserver.md |
| Feature | **Event Broadcasting** | (this document) |

## Problem Statement

Broadcasts MessageBus events (phase transitions, module state changes) to subscribed editors in real-time.

## Acceptance Criteria

- [ ] Observe MessageBus for phase_transition events
- [ ] Broadcast events immediately (push, not poll)
- [ ] JSON event format with timestamp
- [ ] Only send to subscribed connections
- [ ] Support multiple event types (phase_transition, module_state_change)
- [ ] Include event metadata (source PU, timestamp)

## Design

**Phase Transition Observer:**
```cpp
void DebugServerModule::DoStart(const IStartData* startData) {
    // Subscribe to MessageBus for phase transitions
    GetProcessingUnit()->GetMessageBus()->Subscribe(
        StringCRC("phase_transition"),
        [this](const Message& msg) {
            BroadcastPhaseTransition(msg);
        }
    );
}

void DebugServerModule::BroadcastPhaseTransition(const Message& msg) {
    // Only send to editors subscribed to "phase_transition"
    auto subscribers = mSubscriptionManager->GetSubscribers(StringCRC("phase_transition"));
    
    if (subscribers.Size() == 0) return;
    
    Json::Value json = StateSerializer::SerializePhaseTransition(
        msg.GetData<PhaseTransitionData>()->fromPhase,
        msg.GetData<PhaseTransitionData>()->toPhase,
        msg.GetTimestamp()
    );
    
    std::string jsonStr = Json::FastWriter().write(json);
    
    for (int connId : subscribers) {
        mServer->SendText(connId, jsonStr.c_str());
    }
}
```

**JSON Format:**
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

## Implementation Files

- `Dia/DiaDebugServer/DebugServerModule.cpp` - MessageBus subscription and broadcasting

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaDebugServer | DDS-007 | Push phase transitions as immediate events | ✅ **Compliant** - Events sent immediately via MessageBus observer |

**All binding decisions: COMPLIANT ✅**

## Open Questions

**Resolved:**
- **Decision 7:** Curated event set for Phase 5: phase_transition, module_state_change, hot_reload_complete, error. Add more in Phase 6+ based on usage.
- **Decision 8:** No event throttling for Phase 5 - send all events immediately. Add throttling in Phase 6+ if bandwidth becomes issue.
- **Decision 9:** Include timestamp in JSON; editors sort by timestamp. No sequence numbers for Phase 5 (single game process = monotonic time).

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Event Types | Which events beyond phase_transition? | module_state_change, hot_reload_complete, error events | ✅ Curated set (Decision 7) |
| 2 | Throttling | Should high-frequency events be throttled? | No throttling for phase transitions; throttle per-frame events | ✅ No throttling for Phase 5 (Decision 8) |
| 3 | Ordering | How to maintain event ordering? | Include sequence number in JSON; editors sort by timestamp | ✅ Timestamp only (Decision 9) |

## Status

`Done`
