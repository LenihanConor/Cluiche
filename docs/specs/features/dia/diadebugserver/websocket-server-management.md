# Feature Spec: WebSocket Server Management

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaDebugServer | @docs/specs/systems/dia/diadebugserver.md |
| Feature | **WebSocket Server Management** | (this document) |

## Problem Statement

Manages WebSocket server lifecycle (start/stop/update) in the game process, accepting editor connections for remote debugging and live data streaming.

## Acceptance Criteria

- [x] DebugServerModule derives from Dia::Application::Module
- [x] Start WebSocket server on configurable port (default 8080)
- [x] Stop server cleanly on module shutdown
- [x] Update server every frame (process messages)
- [x] Handle multiple simultaneous editor connections
- [x] Auto-start server in Debug builds, disabled in Release
- [x] Configuration via .diaapp manifest (editor.debug_server section)
- [x] Logging for server lifecycle and connections

## Design

### DebugServerModule

**Class Definition:**
```cpp
namespace Dia::DebugServer {
    class DebugServerModule : public Dia::Application::Module {
    public:
        DebugServerModule(Dia::Application::ProcessingUnit* pu, const StringCRC& id);
        virtual ~DebugServerModule();
        
        // Module lifecycle
        virtual void DoStart(const IStartData* startData) override;
        virtual void DoUpdate() override;
        virtual void DoStop() override;
        
        // Server control
        bool StartServer();
        void StopServer();
        bool IsServerRunning() const { return mServer && mServer->IsRunning(); }
        
        // Configuration
        void SetPort(uint16_t port) { mPort = port; }
        void EnableAutoStart(bool enable) { mAutoStart = enable; }
        
        // Connection info
        int GetConnectionCount() const;
        
    private:
        Dia::WebSocket::Server* mServer;
        uint16_t mPort;
        bool mAutoStart;
    };
}
```

### Module Registration

**In .diaapp manifest:**
```json
{
  "processing_units": [{
    "id": "MainProcessingUnit",
    "modules": [
      {
        "type": "DebugServerModule",
        "instance_id": "debug_server",
        "config": {
          "port": 8080,
          "auto_start": true
        }
      }
    ]
  }]
}
```

**Registration Macro:**
```cpp
// In DebugServerModule.cpp
REGISTER_MODULE(DebugServerModule, "DebugServerModule");
```

### DoStart

**DebugServerModule::DoStart:**
```cpp
void DebugServerModule::DoStart(const IStartData* startData) {
    DIA_LOG("DebugServerModule starting...");
    
    // Load configuration from manifest
    if (startData && startData->config) {
        mPort = startData->config["port"].asInt();
        mAutoStart = startData->config["auto_start"].asBool();
    }
    
    // Create WebSocket server
    mServer = new Dia::WebSocket::Server(mPort);
    
    // Register callbacks
    mServer->SetConnectionCallback([this](int connId, bool connected) {
        HandleConnection(connId, connected);
    });
    
    mServer->SetMessageCallback([this](int connId, const Dia::WebSocket::Message& msg) {
        HandleMessage(connId, msg);
    });
    
    // Auto-start if enabled
    if (mAutoStart) {
        StartServer();
    }
}
```

### StartServer

**DebugServerModule::StartServer:**
```cpp
bool DebugServerModule::StartServer() {
    if (!mServer) {
        DIA_LOG_ERROR("DebugServerModule: Server not initialized");
        return false;
    }
    
    if (mServer->IsRunning()) {
        DIA_LOG_WARNING("DebugServerModule: Server already running");
        return true;
    }
    
    bool success = mServer->Start();
    
    if (success) {
        DIA_LOG("DebugServer listening on port %d", mPort);
    } else {
        DIA_LOG_ERROR("DebugServer failed to start on port %d", mPort);
    }
    
    return success;
}
```

### DoUpdate

**DebugServerModule::DoUpdate:**
```cpp
void DebugServerModule::DoUpdate() {
    if (!mServer || !mServer->IsRunning()) return;
    
    // Process WebSocket messages
    mServer->Update();
    
    // (Other features handle broadcasting metrics, etc.)
}
```

### DoStop

**DebugServerModule::DoStop:**
```cpp
void DebugServerModule::DoStop() {
    DIA_LOG("DebugServerModule stopping...");
    
    if (mServer) {
        mServer->Stop();
        delete mServer;
        mServer = nullptr;
    }
}
```

### Connection Handling

**HandleConnection:**
```cpp
void DebugServerModule::HandleConnection(int connId, bool connected) {
    if (connected) {
        DIA_LOG("Editor connected: connection %d", connId);
        
        // Send initial data (core metrics, available data types)
        SendWelcomeMessage(connId);
    } else {
        DIA_LOG("Editor disconnected: connection %d", connId);
        
        // Clean up subscriptions for this connection
        UnsubscribeAll(connId);
    }
}
```

### Message Handling

**HandleMessage:**
```cpp
void DebugServerModule::HandleMessage(int connId, const Dia::WebSocket::Message& msg) {
    if (msg.type != Dia::WebSocket::MessageType::kText) {
        DIA_LOG_WARNING("DebugServer: Ignoring non-text message");
        return;
    }
    
    // Parse JSON
    Json::Value json;
    Json::Reader reader;
    if (!reader.parse(msg.AsText(), json)) {
        DIA_LOG_ERROR("DebugServer: Failed to parse JSON");
        return;
    }
    
    std::string type = json["type"].asString();
    
    // Dispatch to feature handlers
    if (type == "subscribe") {
        HandleSubscribe(connId, json);
    } else if (type == "unsubscribe") {
        HandleUnsubscribe(connId, json);
    } else if (type == "command") {
        HandleCommand(connId, json);
    } else {
        DIA_LOG_WARNING("DebugServer: Unknown message type: %s", type.c_str());
    }
}
```

### Release Build Exclusion

**Conditional Compilation:**
```cpp
#ifdef DIA_DEBUG
    // Include DebugServerModule in Debug builds
    pu->AddModule(new Dia::DebugServer::DebugServerModule(pu, "debug_server"));
#endif
// Release builds: module not included
```

### GetConnectionCount

**DebugServerModule::GetConnectionCount:**
```cpp
int DebugServerModule::GetConnectionCount() const {
    if (!mServer) return 0;
    return mServer->GetConnectionCount();
}
```

## Implementation Files

- `Dia/DiaDebugServer/DebugServerModule.h` - Module interface
- `Dia/DiaDebugServer/DebugServerModule.cpp` - Server lifecycle management

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Dia | AD-004 | ProcessingUnit/Phase/Module for apps | ✅ **Compliant** - DebugServerModule extends Module |
| DiaDebugServer | DDS-001 | Generic system (not DiaApplication-specific) | ✅ **Compliant** - Can be used by any Dia app |
| DiaDebugServer | DDS-004 | Always listen in Debug, disabled in Release | ✅ **Compliant** - Conditional compilation |
| DiaDebugServer | DDS-009 | Use DiaWebSocket::Server | ✅ **Compliant** - Uses WebSocket::Server |

**All binding decisions: COMPLIANT ✅**

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | Should server be optional or always enabled in Debug? | Auto-start by default; can be disabled via config |
| 2 | Multiple apps on same port? | Port conflict - each app needs unique port or only one server active |
| 3 | Should server start even if no editors connected? | Yes - always listening; minimal overhead |

## Status

`Done`
