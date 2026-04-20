# Feature Spec: DiaAPI Command Gateway

## Problem Statement

Receives command messages from editors, forwards to DiaAPI::CommandRegistry, returns results.

## Acceptance Criteria

- [x] Handle "command" message type
- [x] Forward to DiaAPI::CommandRegistry::Execute
- [x] Return command result (success, output)
- [x] JSON command format

## Design

**Command Message:**
```json
{
  "type": "command",
  "command": "hot_reload",
  "payload": {
    "manifest_path": "C:/path/to/manifest.diaapp"
  }
}
```

**Handler:**
```cpp
void DebugServerModule::HandleCommand(int connId, const Json::Value& json) {
    std::string command = json["command"].asString();
    Json::Value argsJson = json["payload"];
    
    // Convert JSON to CommandArgs
    DiaAPI::CommandArgs args = JsonToCommandArgs(argsJson);
    
    // Execute
    int result = DiaAPI::CommandRegistry::Instance().Execute(command.c_str(), args);
    
    // Send response
    Json::Value response;
    response["type"] = "command_response";
    response["command"] = command;
    response["success"] = (result == 0);
    response["result"] = result;
    
    mServer->SendText(connId, Json::FastWriter().write(response).c_str());
}
```

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaDebugServer | DDS-008 | Forward commands to DiaAPI CommandRegistry | ✅ **Compliant** |

**All binding decisions: COMPLIANT ✅**

## Open Questions

**Resolved:**
- **Decision 10:** Commands queued in WebSocket thread, executed on main thread during DoUpdate() in a single batch. No blocking across frames.
- **Decision 11:** No command timeout for Phase 5 - assume all commands complete within reasonable time. Add timeout in Phase 6+ if needed.
- **Decision 12:** Buffer command output (capture stdout/stderr), send full result when complete via command_response message.

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Async | Should commands run on separate thread? | No - run on main thread during Update(); block until complete | ✅ Queue + main thread execution (Decision 10) |
| 2 | Timeout | Should commands have timeout? | Yes - 30s default; abort if exceeded | ✅ No timeout for Phase 5 (Decision 11) |
| 3 | Output | How to stream command output? | Buffer output; send full result when complete | ✅ Buffer output (Decision 12) |

## Status

`Done`
