# Feature Spec: WebSocket Client (GameConnectionManager)

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEditor | @docs/specs/systems/dia/diaeditor.md |
| Feature | **WebSocket Client** | (this document) |

## Problem Statement

Connects editor to running game's DiaDebugServer via WebSocket for live data streaming, event monitoring, and command execution.

## Acceptance Criteria

- [x] GameConnectionManager module manages WebSocket connection
- [x] ConnectRemote(address, port) initiates connection — implemented as Connect(host, port)
- [x] Disconnect() closes connection cleanly
- [x] IsConnected() returns connection state
- [x] Subscribe to data types (processing_unit_state, phase_transition) — Subscribe/Unsubscribe
- [x] Send commands to game (hot_reload, etc.) — SendCommand/SendCommandWithResponse
- [x] Handle connection/disconnection callbacks — SetConnectionCallback, SetRawMessageCallback
- [x] Parse incoming JSON messages and route to subscribers — HandleMessage with topic + command_response routing
- [x] Uses DiaWebSocket::Client internally
- [x] Auto-reconnect on disconnect (configurable) — SetAutoReconnect/SetAutoReconnectDelay/SetAutoReconnectMaxAttempts

## Design

### GameConnectionManager

```cpp
namespace Dia::Editor {
    enum class ConnectionResult {
        kSuccess,
        kFailed,
        kTimeout,
        kAlreadyConnected
    };
    
    class GameConnectionManager : public Dia::Application::Module {
    public:
        GameConnectionManager(Dia::Application::ProcessingUnit* pu, const StringCRC& id);
        virtual ~GameConnectionManager();
        
        // Module lifecycle
        virtual void DoStart(const IStartData* startData) override;
        virtual void DoUpdate() override;
        virtual void DoStop() override;
        
        // Connection
        ConnectionResult ConnectRemote(const char* address, uint16_t port = 8080);
        void Disconnect();
        bool IsConnected() const;
        
        // Data subscription
        using DataCallback = Dia::Core::Functor<void(const Json::Value& data)>;
        void SubscribeToData(const StringCRC& dataType, DataCallback callback);
        void UnsubscribeFromData(const StringCRC& dataType);
        
        // Commands
        void SendCommand(const char* command, const Json::Value& args);
        using CommandResponseCallback = Dia::Core::Functor<void(bool success, const Json::Value& result)>;
        void SendCommandWithResponse(const char* command, 
                                     const Json::Value& args,
                                     CommandResponseCallback callback);
        
        // Connection callbacks
        using ConnectionCallback = Dia::Core::Functor<void(bool connected)>;
        void SetConnectionCallback(ConnectionCallback callback);
        
    private:
        Dia::WebSocket::Client* mClient;
        
        struct Subscription {
            StringCRC dataType;
            DataCallback callback;
        };
        DynamicArrayC<Subscription, 16> mSubscriptions;
        
        struct PendingCommand {
            StringCRC command;
            CommandResponseCallback callback;
        };
        DynamicArrayC<PendingCommand, 8> mPendingCommands;
        
        ConnectionCallback mOnConnectionChange;
        
        void HandleMessage(const Dia::WebSocket::Message& msg);
        void ProcessDataUpdate(const Json::Value& json);
        void ProcessCommandResponse(const Json::Value& json);
        void ProcessEvent(const Json::Value& json);
    };
}
```

### ConnectRemote

```cpp
ConnectionResult GameConnectionManager::ConnectRemote(const char* address, uint16_t port) {
    if (IsConnected()) {
        DIA_LOG_WARNING("GameConnectionManager: Already connected");
        return ConnectionResult::kAlreadyConnected;
    }
    
    // Build WebSocket URL
    char url[256];
    sprintf(url, "ws://%s:%d", address, port);
    
    DIA_LOG("Connecting to game at %s...", url);
    
    // Connect
    bool success = mClient->Connect(url);
    
    if (success) {
        DIA_LOG("Connected to game successfully");
        return ConnectionResult::kSuccess;
    } else {
        DIA_LOG_ERROR("Failed to connect to game");
        return ConnectionResult::kFailed;
    }
}
```

### Subscribe to Data

```cpp
void GameConnectionManager::SubscribeToData(const StringCRC& dataType, DataCallback callback) {
    // Store subscription
    Subscription sub;
    sub.dataType = dataType;
    sub.callback = callback;
    mSubscriptions.Add(sub);
    
    // Send subscribe message to game
    Json::Value json;
    json["type"] = "subscribe";
    json["data_type"] = dataType.GetString();
    
    mClient->SendText(Json::FastWriter().write(json).c_str());
    
    DIA_LOG("Subscribed to: %s", dataType.GetString());
}
```

### Send Command

```cpp
void GameConnectionManager::SendCommand(const char* command, const Json::Value& args) {
    if (!IsConnected()) {
        DIA_LOG_ERROR("Cannot send command: not connected");
        return;
    }
    
    Json::Value json;
    json["type"] = "command";
    json["command"] = command;
    json["payload"] = args;
    
    mClient->SendText(Json::FastWriter().write(json).c_str());
    
    DIA_LOG("Sent command: %s", command);
}

void GameConnectionManager::SendCommandWithResponse(
    const char* command,
    const Json::Value& args,
    CommandResponseCallback callback) {
    
    SendCommand(command, args);
    
    // Store pending callback
    PendingCommand pending;
    pending.command = command;
    pending.callback = callback;
    mPendingCommands.Add(pending);
}
```

### Handle Incoming Messages

```cpp
void GameConnectionManager::DoUpdate() {
    if (!mClient) return;
    
    mClient->Update();  // Process WebSocket messages
}

void GameConnectionManager::HandleMessage(const Dia::WebSocket::Message& msg) {
    if (msg.type != Dia::WebSocket::MessageType::kText) return;
    
    // Parse JSON
    Json::Value json;
    Json::Reader reader;
    if (!reader.parse(msg.AsText(), json)) {
        DIA_LOG_ERROR("Failed to parse game message");
        return;
    }
    
    const char* type = json["type"].asCString();
    StringCRC typeCRC(type);
    
    if (typeCRC == StringCRC("core_metrics")) {
        ProcessDataUpdate(json);  // Always broadcast
    } else if (typeCRC == StringCRC("data_update")) {
        ProcessDataUpdate(json);
    } else if (typeCRC == StringCRC("event")) {
        ProcessEvent(json);
    } else if (typeCRC == StringCRC("command_response")) {
        ProcessCommandResponse(json);
    }
}

void GameConnectionManager::ProcessDataUpdate(const Json::Value& json) {
    StringCRC dataType(json.get("data_type", "core_metrics").asCString());
    
    // Find subscribers
    for (const auto& sub : mSubscriptions) {
        if (sub.dataType == dataType || dataType == StringCRC("core_metrics")) {
            sub.callback(json["payload"]);
        }
    }
}

void GameConnectionManager::ProcessCommandResponse(const Json::Value& json) {
    StringCRC command(json["command"].asCString());
    bool success = json["success"].asBool();
    
    // Find pending command
    for (int i = 0; i < mPendingCommands.Size(); ++i) {
        if (mPendingCommands[i].command == command) {
            mPendingCommands[i].callback(success, json);
            mPendingCommands.RemoveAt(i);
            break;
        }
    }
}
```

## Implementation Files

- `Dia/DiaEditor/LiveConnection/GameConnectionManager.h`
- `Dia/DiaEditor/LiveConnection/GameConnectionManager.cpp`

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | **Compliant** — data types, command IDs are StringCRC |
| Platform | PD-002 | PU/Phase/Module architecture | **Compliant** — GameConnectionManager is a Module |
| Platform | PD-004 | No STL in public APIs | **Compliant** — uses Dia::Core::Functor, StringCRC, DynamicArrayC |
| Dia | AD-002 | No STL in public APIs | **Compliant** — reinforces PD-004 |
| Dia | AD-003 | Namespace convention | **Compliant** — all types in `Dia::Editor::` |
| DiaEditor | SED-004 | WebSocket protocol uses JSON | **Compliant** — JSON messages over WebSocket |
| DiaEditor | SED-008 | Observer pattern (not polling) | **N/A** — connection module, not model |
| DiaEditor | SED-010 | Use DiaDebugProtocol for wire types | **Compliant** — uses DiaDebugProtocol message types |

**All binding decisions: COMPLIANT**

## Open Questions

**Unresolved:**
- Should editor cache received data or discard after callback?
- How to handle message ordering across reconnects?
- Should we support multiple simultaneous game connections?

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Caching | Cache received data? | No - subscribers responsible for caching if needed | |
| 2 | Reconnect | Auto-resubscribe on reconnect? | Yes - store subscriptions and re-send on connect | |
| 3 | Multi-Game | Connect to multiple games? | Phase 7+ - single connection for Phase 5 | |
| 4 | Threading | Message callbacks on which thread? | Main thread (during Update()) - safe for UI | |

## Status

`Done` - Implemented and tested
