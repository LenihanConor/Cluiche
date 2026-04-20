# Feature Spec: Protocol Command Handling

## Problem Statement

Handles debug protocol commands (get_state, list_data_types) that are server-specific (not DiaAPI commands).

## Acceptance Criteria

- [x] Handle get_state command (return current PU/Phase/Module state)
- [x] Handle list_data_types command (return available subscription types)
- [x] Extensible for custom commands

## Design

**get_state Command:**
```json
{
  "type": "command",
  "command": "get_state",
  "payload": {
    "state_type": "processing_units"
  }
}
```

**Response:**
```json
{
  "type": "command_response",
  "command": "get_state",
  "success": true,
  "payload": {
    "processing_units": [
      { "id": "MainProcessingUnit", "current_phase": "UpdatePhase" }
    ]
  }
}
```

**Handler:**
```cpp
void DebugServerModule::HandleCommand(int connId, const Json::Value& json) {
    std::string command = json["command"].asString();
    
    // Check if protocol command
    if (command == "get_state") {
        HandleGetState(connId, json["payload"]);
        return;
    }
    
    // Otherwise forward to DiaAPI
    HandleDiaAPICommand(connId, json);
}
```

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaDebugServer | DDS-001 | Generic system | ✅ **Compliant** - Protocol commands are debug-server specific |

**All binding decisions: COMPLIANT ✅**

## Open Questions

**Resolved:**
- **Decision 13:** Extensible protocol commands via RegisterProtocolCommand(name, handler) for custom game-specific debug commands.
- **Decision 14:** list_commands protocol command returns all available protocol commands + all DiaAPI commands.
- **Decision 15:** Protocol versioning via semantic versioning (e.g., "1.0.0") in welcome message. Editor checks compatibility on connect.
- **Decision 16:** Minimal command set for Phase 5: get_state (with optional pu_id parameter), list_commands, get_server_stats. Add more in Phase 6+.

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Extensibility | Allow custom protocol commands? | Yes - RegisterProtocolCommand() method | ✅ Extensible (Decision 13) |
| 2 | Discovery | How do editors discover available commands? | list_commands protocol command returns all available | ✅ list_commands (Decision 14) |
| 3 | Versioning | Should protocol have version number? | Yes - include in welcome message; reject incompatible versions | ✅ Semantic versioning (Decision 15) |
| 4 | Command Set | Which commands for Phase 5? | get_state, list_commands, get_server_stats | ✅ Minimal set (Decision 16) |

## Status

`Done`
